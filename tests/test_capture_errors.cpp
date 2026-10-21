// Capture failures and recovery through the visible Capture action and grim process boundary.
#include "../window.h"

#include <QApplication>
#include <QFile>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include <functional>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template <typename T> T* named(QWidget& root, const char* name) {
    auto* widget = root.findChild<T*>(QString::fromLatin1(name));
    require(widget != nullptr, name);
    return widget;
}

bool waitUntil(const std::function<bool()>& condition, int timeoutMs = 5000) {
    for (int elapsed = 0; elapsed < timeoutMs; elapsed += 20) {
        if (condition()) return true;
        QTest::qWait(20);
    }
    return condition();
}

bool captureAndDismissWarning(QPushButton* capture, QString* warningText) {
    bool sawWarning = false;
    QTimer responder;
    responder.setInterval(10);
    QObject::connect(&responder, &QTimer::timeout, [&] {
        auto* warning = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        if (!warning) return;
        sawWarning = true;
        if (warningText) *warningText = warning->text();
        warning->accept();
    });
    QTimer bailout;
    bailout.setSingleShot(true);
    QObject::connect(&bailout, &QTimer::timeout, [] {
        if (auto* modal = QApplication::activeModalWidget()) modal->close();
    });
    bailout.start(6000);
    responder.start();
    QTest::mouseClick(capture, Qt::LeftButton);
    const bool returned = waitUntil([&] {
        return sawWarning && QApplication::activeModalWidget() == nullptr;
    }, 6500);
    responder.stop();
    bailout.stop();
    return returned && sawWarning;
}

void writeFakeGrim(const QTemporaryDir& files) {
    QFile executable(files.filePath(QStringLiteral("grim")));
    require(executable.open(QIODevice::WriteOnly | QIODevice::Truncate),
            "could not create fake grim");
    const QByteArray script =
        "#!/bin/sh\n"
        "printf x >> \"$RULER_TEST_GRIM_CALLS\"\n"
        "case \"$RULER_TEST_GRIM_MODE\" in\n"
        "  failure) echo 'test capture unavailable' >&2; exit 7 ;;\n"
        "  malformed) printf 'not a PNG'; exit 0 ;;\n"
        "  valid) cat \"$RULER_TEST_GRIM_PNG\"; exit 0 ;;\n"
        "  *) echo 'unknown test mode' >&2; exit 9 ;;\n"
        "esac\n";
    require(executable.write(script) == script.size(), "could not write fake grim");
    executable.close();
    require(executable.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                      QFileDevice::ExeOwner | QFileDevice::ReadGroup |
                                      QFileDevice::ExeGroup), "could not make fake grim executable");
}

int invocationCount(const QString& path) {
    QFile calls(path);
    if (!calls.open(QIODevice::ReadOnly)) return 0;
    return calls.readAll().size();
}

QPoint widgetPoint(Canvas* canvas, QPointF imagePoint, QSize imageSize) {
    const QSizeF drawn = imageSize.scaled(canvas->size(), Qt::KeepAspectRatio);
    const QPointF origin((canvas->width() - drawn.width()) / 2,
                         (canvas->height() - drawn.height()) / 2);
    return (origin + QPointF(imagePoint.x() * drawn.width() / imageSize.width(),
                             imagePoint.y() * drawn.height() / imageSize.height())).toPoint();
}
} // namespace

int main(int argc, char** argv) {
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    try {
        QTemporaryDir files;
        require(files.isValid(), "temporary directory unavailable");
        QImage fixture(64, 48, QImage::Format_ARGB32);
        fixture.fill(Qt::white);
        const QString png = files.filePath(QStringLiteral("fixture.png"));
        const QString calls = files.filePath(QStringLiteral("calls"));
        require(fixture.save(png, "PNG"), "could not write valid capture fixture");
        writeFakeGrim(files);
        qputenv("PATH", QFile::encodeName(files.path()) + QByteArray(":") + qgetenv("PATH"));
        qputenv("RULER_TEST_GRIM_PNG", QFile::encodeName(png));
        qputenv("RULER_TEST_GRIM_CALLS", QFile::encodeName(calls));

        RulerWindow window;
        window.show();
        require(waitUntil([&] { return window.isVisible(); }), "controls did not open");
        auto* capture = named<QPushButton>(window, "captureButton");
        auto* source = named<QLabel>(window, "sourceStatus");
        auto* status = named<QLabel>(window, "statusMessage");
        const QString initialSource = source->text();

        qputenv("RULER_TEST_GRIM_MODE", "failure");
        QString warning;
        require(captureAndDismissWarning(capture, &warning), "nonzero grim exit did not show a warning");
        require(warning.contains(QStringLiteral("test capture unavailable")) &&
                status->text().contains(QStringLiteral("захват"), Qt::CaseInsensitive),
                "nonzero grim exit was not explained visibly");
        require(source->text() == initialSource && window.historyCount() == 0 &&
                !window.canvas()->isVisible(), "failed capture created a source or history");

        qputenv("RULER_TEST_GRIM_MODE", "malformed");
        warning.clear();
        require(captureAndDismissWarning(capture, &warning), "malformed PNG did not show a warning");
        require(!warning.isEmpty() && source->text() == initialSource &&
                window.historyCount() == 0 && !window.canvas()->isVisible(),
                "malformed PNG was accepted or damaged the empty session");

        qputenv("RULER_TEST_GRIM_MODE", "valid");
        const int beforeRepeatedClick = invocationCount(calls);
        QTest::mouseClick(capture, Qt::LeftButton);
        // Capture hides the controls while its delayed process launch is pending.
        // The same public button action must still be ignored during that interval.
        capture->click();
        require(waitUntil([&] { return window.canvas()->isVisible(); }),
                "valid capture did not recover after errors");
        require(source->text().contains(QStringLiteral("64×48")) && window.historyCount() == 0,
                "valid recovery did not expose the fixture as the source");
        require(invocationCount(calls) == beforeRepeatedClick + 1,
                "repeated capture action launched grim more than once");

        Canvas* canvas = window.canvas();
        QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier,
                          widgetPoint(canvas, {10, 10}, fixture.size()));
        QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier,
                          widgetPoint(canvas, {50, 10}, fixture.size()));
        QTest::keyClick(canvas, Qt::Key_Return);
        require(waitUntil([&] { return window.isVisible() && !canvas->isVisible(); }) &&
                window.historyCount() == 1,
                "valid capture could not produce a measurement before the next failure");
        const QString sourceBeforeError = source->text();
        const QString savedBeforeError = files.filePath(QStringLiteral("source-before-error.png"));
        require(window.saveSourceImage(savedBeforeError) && QImage(savedBeforeError) == fixture,
                "source before the next failure did not match the valid capture");

        qputenv("RULER_TEST_GRIM_MODE", "failure");
        warning.clear();
        require(captureAndDismissWarning(capture, &warning),
                "later nonzero grim exit did not show a warning");
        const QString savedAfterError = files.filePath(QStringLiteral("source-after-error.png"));
        require(window.historyCount() == 1 && source->text() == sourceBeforeError &&
                !canvas->isVisible() && window.saveSourceImage(savedAfterError) &&
                QImage(savedAfterError) == fixture,
                "later capture failure damaged the prior source or measurement");

        window.hide();
        canvas->hide();
        std::cout << "capture error tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << "capture error test failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
