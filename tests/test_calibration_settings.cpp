#include "../window.h"

#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScreen>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include <cmath>
#include <cstdio>

#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "test_calibration_settings failed at line %d: %s\n", __LINE__, #condition); \
    return 1; \
} } while (false)

namespace {
QSettings settings() {
    return QSettings(QSettings::IniFormat, QSettings::UserScope,
                     QStringLiteral("desktop-ruler"), QStringLiteral("desktop-ruler"));
}

QString scaleLabel(const RulerWindow& window) {
    for (auto* label : window.findChildren<QLabel*>())
        if (label->text().startsWith(QStringLiteral("1 px = "))) return label->text();
    return {};
}

QPoint widgetPoint(Canvas* canvas, QPointF imagePoint, QSize imageSize) {
    const QSizeF drawn = imageSize.scaled(canvas->size(), Qt::KeepAspectRatio);
    const QPointF origin((canvas->width() - drawn.width()) / 2,
                         (canvas->height() - drawn.height()) / 2);
    return (origin + QPointF(imagePoint.x() * drawn.width() / imageSize.width(),
                             imagePoint.y() * drawn.height() / imageSize.height())).toPoint();
}

bool calibrate(RulerWindow& window, const QImage& image, double knownLength,
               const QString& unit, bool accept) {
    window.selectMode(ruler::Mode::Calibrate);
    window.openImage(image);
    auto* canvas = window.canvas();
    if (!QTest::qWaitForWindowExposed(canvas)) return false;
    const QPoint start = widgetPoint(canvas, {50, 100}, image.size());
    const QPoint end = widgetPoint(canvas, {550, 100}, image.size());
    QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, start);

    bool sawDialog = false;
    QTimer responder;
    responder.setInterval(10);
    QObject::connect(&responder, &QTimer::timeout, [&] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        if (!dialog || dialog->objectName() != QStringLiteral("calibrationDialog") || sawDialog) return;
        sawDialog = true;
        if (auto* known = dialog->findChild<QDoubleSpinBox*>(QStringLiteral("knownLength")))
            known->setValue(knownLength);
        if (auto* knownUnit = dialog->findChild<QLineEdit*>(QStringLiteral("knownUnit")))
            knownUnit->setText(unit);
        if (auto* buttons = dialog->findChild<QDialogButtonBox*>()) {
            if (auto* button = buttons->button(accept ? QDialogButtonBox::Ok : QDialogButtonBox::Cancel))
                button->click();
        }
    });
    QTimer bailout;
    bailout.setSingleShot(true);
    QObject::connect(&bailout, &QTimer::timeout, [] {
        if (auto* modal = QApplication::activeModalWidget()) modal->close();
    });
    bailout.start(4000);
    responder.start();
    QTest::mouseClick(canvas, Qt::RightButton, Qt::NoModifier, end);
    responder.stop();
    bailout.stop();
    return sawDialog;
}
} // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    QTemporaryDir settingsDir;
    CHECK(settingsDir.isValid());
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());

    auto store = settings();
    store.setValue(QStringLiteral("shortcuts/capture"), QStringLiteral("Ctrl+Shift+C"));
    store.sync();
    CHECK(store.status() == QSettings::NoError);

    QImage image(QGuiApplication::primaryScreen()->size(), QImage::Format_ARGB32);
    CHECK(image.width() >= 600 && image.height() >= 200);
    image.fill(Qt::white);
    {
        RulerWindow window;
        window.show();
        CHECK(calibrate(window, image, 50, QStringLiteral("mm"), true));
        store.sync();
        CHECK(std::abs(store.value(QStringLiteral("calibration/scale")).toDouble() - 0.1) < 1e-12);
        CHECK(store.value(QStringLiteral("calibration/unit")).toString() == QStringLiteral("mm"));
        CHECK(scaleLabel(window) == QStringLiteral("1 px = 0.1 mm"));
    }
    {
        RulerWindow window;
        CHECK(scaleLabel(window) == QStringLiteral("1 px = 0.1 mm"));
        CHECK(window.findChild<QLineEdit*>(QStringLiteral("knownUnit"))->text() == QStringLiteral("mm"));
        CHECK(window.findChild<QPushButton*>(QStringLiteral("resetScaleButton"))->isEnabled());
        window.show();
        window.selectMode(ruler::Mode::Path);
        window.openImage(image);
        auto* canvas = window.canvas();
        CHECK(QTest::qWaitForWindowExposed(canvas));
        QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier,
                          widgetPoint(canvas, {50, 100}, image.size()));
        QTest::mouseClick(canvas, Qt::RightButton, Qt::NoModifier,
                          widgetPoint(canvas, {150, 100}, image.size()));
        CHECK(window.historyCount() == 1);
        CHECK(window.findChild<QLabel*>(QStringLiteral("latestResult"))->text().contains(QStringLiteral("10.00 mm")));
        CHECK(calibrate(window, image, 100, QStringLiteral("cm"), false));
        store.sync();
        CHECK(std::abs(store.value(QStringLiteral("calibration/scale")).toDouble() - 0.1) < 1e-12);
        CHECK(store.value(QStringLiteral("calibration/unit")).toString() == QStringLiteral("mm"));
        CHECK(scaleLabel(window) == QStringLiteral("1 px = 0.1 mm"));

        QTest::mouseClick(window.findChild<QPushButton*>(QStringLiteral("resetScaleButton")), Qt::LeftButton);
        store.sync();
        CHECK(!store.contains(QStringLiteral("calibration/scale")));
        CHECK(!store.contains(QStringLiteral("calibration/unit")));
        CHECK(scaleLabel(window) == QStringLiteral("1 px = 1 px"));
    }
    {
        RulerWindow window;
        CHECK(scaleLabel(window) == QStringLiteral("1 px = 1 px"));
        CHECK(!window.findChild<QPushButton*>(QStringLiteral("resetScaleButton"))->isEnabled());
    }
    CHECK(store.value(QStringLiteral("shortcuts/capture")).toString() == QStringLiteral("Ctrl+Shift+C"));

    const auto malformed = [&](const QVariant& scale, const QString& unit) {
        store.setValue(QStringLiteral("calibration/scale"), scale);
        store.setValue(QStringLiteral("calibration/unit"), unit);
        store.sync();
        RulerWindow window;
        return scaleLabel(window) == QStringLiteral("1 px = 1 px") &&
            !window.findChild<QPushButton*>(QStringLiteral("resetScaleButton"))->isEnabled();
    };
    CHECK(malformed(QStringLiteral("nan"), QStringLiteral("mm")));
    CHECK(malformed(0, QStringLiteral("mm")));
    CHECK(malformed(0.1, QStringLiteral("  ")));
    CHECK(malformed(0.1, QString(33, QLatin1Char('x'))));
    CHECK(store.value(QStringLiteral("shortcuts/capture")).toString() == QStringLiteral("Ctrl+Shift+C"));
    return 0;
}
