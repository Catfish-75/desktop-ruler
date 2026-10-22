// Exercises the visible controls. Capture is stubbed at the grim process boundary;
// native compositor, tray and focus behavior need separate interactive acceptance.
#include "../window.h"

#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QToolButton>

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

QPoint widgetPoint(Canvas* canvas, QPointF imagePoint, QSize imageSize) {
    const QSizeF drawn = imageSize.scaled(canvas->size(), Qt::KeepAspectRatio);
    const QPointF origin((canvas->width()-drawn.width())/2,
                         (canvas->height()-drawn.height())/2);
    return (origin+QPointF(imagePoint.x()*drawn.width()/imageSize.width(),
                            imagePoint.y()*drawn.height()/imageSize.height())).toPoint();
}

bool saveThroughDialog(QPushButton* button, const QString& path, bool cancel = false) {
    bool sawDialog = false;
    QTimer responder;
    responder.setInterval(10);
    QObject::connect(&responder, &QTimer::timeout, [&] {
        auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
        if (!dialog || sawDialog) return;
        sawDialog = true;
        if (cancel) dialog->reject();
        else {
            QTimer::singleShot(100, dialog, [dialog, path] {
                auto* buttons = dialog->findChild<QDialogButtonBox*>();
                if (auto* entry = dialog->findChild<QLineEdit*>(QStringLiteral("fileNameEdit")))
                    entry->setText(path);
                if (buttons && buttons->button(QDialogButtonBox::Save))
                    buttons->button(QDialogButtonBox::Save)->click();
            });
        }
    });
    QTimer bailout;
    bailout.setSingleShot(true);
    QObject::connect(&bailout, &QTimer::timeout, [] {
        if (auto* modal = QApplication::activeModalWidget()) modal->close();
    });
    bailout.start(4000);
    responder.start();
    QTest::mouseClick(button, Qt::LeftButton);
    responder.stop();
    bailout.stop();
    return sawDialog;
}

bool openBrokenThroughDialog(QPushButton* button, const QString& path) {
    bool sawFile = false, sawError = false;
    QTimer responder;
    responder.setInterval(10);
    QObject::connect(&responder, &QTimer::timeout, [&] {
        if (auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget())) {
            if (!sawFile) {
                sawFile = true;
                QTimer::singleShot(100, dialog, [dialog, path] {
                    auto* buttons = dialog->findChild<QDialogButtonBox*>();
                    if (auto* entry = dialog->findChild<QLineEdit*>(QStringLiteral("fileNameEdit")))
                        entry->setText(path);
                    if (buttons && buttons->button(QDialogButtonBox::Open))
                        buttons->button(QDialogButtonBox::Open)->click();
                });
            }
        } else if (auto* warning = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
            sawError = true;
            warning->accept();
        }
    });
    QTimer bailout;
    bailout.setSingleShot(true);
    QObject::connect(&bailout, &QTimer::timeout, [] {
        if (auto* modal = QApplication::activeModalWidget()) modal->close();
    });
    bailout.start(4000);
    responder.start();
    QTest::mouseClick(button, Qt::LeftButton);
    responder.stop();
    bailout.stop();
    return sawFile && sawError;
}

bool finishCalibration(QToolButton* done, double length, const QString& unit,
                       bool expectInvalidUnit = false) {
    bool sawDialog = false;
    bool rejectedBlankUnit = false;
    QTimer responder;
    responder.setInterval(10);
    QObject::connect(&responder, &QTimer::timeout, [&] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        if (!dialog || dialog->objectName() != QStringLiteral("calibrationDialog") || sawDialog) return;
        sawDialog = true;
        auto* known = named<QDoubleSpinBox>(*dialog, "knownLength");
        auto* knownUnit = named<QLineEdit>(*dialog, "knownUnit");
        auto* buttons = dialog->findChild<QDialogButtonBox*>();
        if (!buttons) return;
        known->setValue(length);
        knownUnit->setText(unit);
        buttons->button(QDialogButtonBox::Ok)->click();
        if (expectInvalidUnit) {
            auto* error = named<QLabel>(*dialog, "calibrationError");
            rejectedBlankUnit = dialog->isVisible() && error->isVisible() &&
                !error->text().isEmpty();
            buttons->button(QDialogButtonBox::Cancel)->click();
        }
    });
    QTimer bailout;
    bailout.setSingleShot(true);
    QObject::connect(&bailout, &QTimer::timeout, [] {
        if (auto* modal = QApplication::activeModalWidget()) modal->close();
    });
    bailout.start(4000);
    responder.start();
    QTest::mouseClick(done, Qt::LeftButton);
    responder.stop();
    bailout.stop();
    return sawDialog && (!expectInvalidUnit || rejectedBlankUnit);
}

void checkActionsInside(QWidget& window) {
    for (const char* name : {"measureButton", "historyButton", "saveSourceButton",
                             "screenshotRegionButton", "helpButton", "shortcutsButton", "quitButton"}) {
        auto* button = named<QPushButton>(window, name);
        const QRect inWindow(button->mapTo(&window, QPoint(0, 0)), button->size());
        require(button->isVisible() && inWindow.left() >= 0 && inWindow.top() >= 0 &&
                inWindow.right() < window.width() && inWindow.bottom() < window.height(),
                "a main action is outside the 941x508 window");
    }
}

void writeFixtureAndGrim(const QTemporaryDir& files, const QImage& fixture) {
    const QString png = files.filePath(QStringLiteral("fixture.png"));
    require(fixture.save(png, "PNG"), "could not write capture fixture");
    QFile script(files.filePath(QStringLiteral("grim")));
    require(script.open(QIODevice::WriteOnly | QIODevice::Truncate), "could not write fake grim");
    require(script.write("#!/bin/sh\ncat \"$RULER_TEST_GRIM_PNG\"\n") > 0, "fake grim write failed");
    script.close();
    require(script.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                  QFileDevice::ExeOwner | QFileDevice::ReadGroup |
                                  QFileDevice::ExeGroup), "could not chmod fake grim");
    qputenv("RULER_TEST_GRIM_PNG", QFile::encodeName(png));
    qputenv("PATH", QFile::encodeName(files.path())+QByteArray(":")+qgetenv("PATH"));
}
} // namespace

int main(int argc, char** argv) {
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    try {
        QTemporaryDir settingsDir;
        require(settingsDir.isValid(), "temporary shortcut settings unavailable");
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());
        QTemporaryDir files;
        require(files.isValid(), "temporary directory unavailable");
        QImage fixture(400, 300, QImage::Format_ARGB32);
        fixture.fill(Qt::white);
        for (int y = 50; y < 100; ++y)
            for (int x = 50; x < 100; ++x) fixture.setPixelColor(x, y, Qt::red);
        writeFixtureAndGrim(files, fixture);

        RulerWindow window;
        window.resize(941, 508);
        window.show();
        require(waitUntil([&] { return window.isVisible(); }), "controls did not open");
        require(window.size() == QSize(941, 508), "window could not be set to 941x508");
        auto* mode = named<QComboBox>(window, "modeSelector");
        auto* language = named<QComboBox>(window, "languageSelector");
        auto* measure = named<QPushButton>(window, "measureButton");
        auto* capture = named<QPushButton>(window, "captureButton");
        auto* source = named<QLabel>(window, "sourceStatus");
        auto* result = named<QLabel>(window, "latestResult");
        auto* resultPanel = named<QWidget>(window, "resultPanel");
        auto* label = named<QLineEdit>(window, "measurementLabel");
        auto* known = named<QDoubleSpinBox>(window, "knownLength");
        auto* tolerance = named<QSpinBox>(window, "colorTolerance");
        auto* options = named<QPushButton>(window, "modeOptionsButton");
        auto* zoom = named<QSpinBox>(window, "magnifierZoom");
        auto* history = named<QTableWidget>(window, "historyTable");
        auto* historyButton = named<QPushButton>(window, "historyButton");
        auto* historyPanel = named<QWidget>(window, "historyPanel");
        auto* fullPng = named<QPushButton>(window, "saveSourceButton");
        auto* markedPng = named<QPushButton>(window, "saveAnnotatedButton");
        auto* csv = named<QPushButton>(window, "exportCsvButton");
        auto* selectedCopy = named<QPushButton>(window, "copySelectedButton");
        auto* clear = named<QPushButton>(window, "clearHistoryButton");

        // Empty-session affordances and actual narrow-window visibility.
        require(source->text().contains(QStringLiteral("не выбран")), "empty source is not explicit");
        require(measure->isEnabled() && capture->isEnabled(), "capture entry points unavailable");
        require(!resultPanel->isVisible() && !historyPanel->isVisible() && !historyButton->isEnabled(),
                "empty result or history is presented as complete");
        require(!fullPng->isEnabled() && !markedPng->isEnabled() && !selectedCopy->isEnabled() &&
                !csv->isEnabled() && !clear->isEnabled(), "history actions enabled without data");
        checkActionsInside(window);

        // Selecting the visible mode control reveals only the relevant inputs.
        mode->setCurrentIndex(static_cast<int>(ruler::Mode::Calibrate));
        require(!known->isVisible() && !tolerance->isVisible() && !label->isVisible(),
                "calibration asks for a value before a line is traced");
        checkActionsInside(window);
        mode->setCurrentIndex(static_cast<int>(ruler::Mode::AutoArea));
        require(tolerance->isVisible() && !known->isVisible(), "auto area tolerance not contextual");
        checkActionsInside(window);
        mode->setCurrentIndex(static_cast<int>(ruler::Mode::Magnifier));
        require(options->isVisible() && !zoom->isVisible(), "magnifier options not collapsed");
        QTest::mouseClick(options, Qt::LeftButton);
        require(zoom->isVisible(), "magnifier zoom cannot be opened");
        mode->setCurrentIndex(static_cast<int>(ruler::Mode::Path));
        require(!label->isVisible() && !options->isVisible() && !zoom->isVisible(),
                "ordinary mode asks for a result name before completion");

        // The primary action must capture when no image exists, then expose a usable canvas.
        QTest::mouseClick(measure, Qt::LeftButton);
        auto* picker = [&]() -> QPushButton* {
            for (auto* top : QApplication::topLevelWidgets())
                if (auto* button = top->findChild<QPushButton*>(QStringLiteral("captureTakeHereButton")))
                    return button;
            return nullptr;
        }();
        require(picker && picker->isVisible(), "capture picker did not open");
        QTest::mouseClick(picker, Qt::LeftButton);
        Canvas* canvas = window.canvas();
        require(waitUntil([&] { return canvas->isVisible(); }), "primary Measure did not capture");
        require(source->text().contains(QStringLiteral("400×300")), "captured source is not identified");
        require(fullPng->isEnabled(), "full image action unavailable after capture");
        const QSize imageSize = fixture.size();
        QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier,
                          widgetPoint(canvas, {100, 150}, imageSize));
        QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier,
                          widgetPoint(canvas, {300, 150}, imageSize));
        QTest::mouseClick(named<QToolButton>(*canvas, "canvasDoneButton"), Qt::LeftButton);
        require(waitUntil([&] { return window.isVisible() && !canvas->isVisible(); }),
                "Done did not return to controls");
        require(window.historyCount() == 1 && resultPanel->isVisible() && !historyPanel->isVisible() &&
                measure->text() == QStringLiteral("Следующее измерение") && label->isVisible(),
                "completion did not present the result-to-next flow");
        require(result->text().contains(QStringLiteral("200")), "latest result lacks measured distance");
        require(waitUntil([&] { return window.isActiveWindow() && measure->hasFocus(); }, 2000),
                "controls were not active before the History shortcut");
        QTest::keyClick(measure, Qt::Key_H, Qt::ControlModifier | Qt::AltModifier);
        require(waitUntil([&] { return historyPanel->isVisible() && history->hasFocus(); }),
                "History shortcut did not expand and focus the table");
        QTest::mouseClick(historyButton, Qt::LeftButton);
        QTest::mouseClick(label, Qt::LeftButton);
        QTest::keyClick(label, Qt::Key_A, Qt::ControlModifier);
        QTest::keyClicks(label, "bolt");
        QTest::mouseClick(historyButton, Qt::LeftButton);
        require(historyPanel->isVisible() && history->item(0, 0)->text() == QStringLiteral("bolt"),
                "post-result name is missing from on-demand history");
        require(markedPng->isEnabled() && selectedCopy->isEnabled() && csv->isEnabled(),
                "result actions unavailable after completion");
        QTest::mouseClick(historyButton, Qt::LeftButton);
        checkActionsInside(window);
        QTest::mouseClick(measure, Qt::LeftButton);
        require(waitUntil([&] { return canvas->isVisible(); }),
                "Next measurement did not reuse the current image");
        QTest::keyClick(canvas, Qt::Key_Escape);
        require(waitUntil([&] { return window.isVisible() && !canvas->isVisible(); }) &&
                label->text() == QStringLiteral("bolt") &&
                history->item(0, 0)->text() == QStringLiteral("bolt"),
                "cancelling Next measurement renamed the previous result");
        QTest::mouseClick(historyButton, Qt::LeftButton);

        // User-facing save dialogs preserve the distinction between full image and marked crop.
        const QString fullPath = files.filePath(QStringLiteral("full.png"));
        const QString cropPath = files.filePath(QStringLiteral("crop.png"));
        require(saveThroughDialog(fullPng, fullPath), "full image save dialog missing");
        require(saveThroughDialog(markedPng, cropPath), "marked crop save dialog missing");
        const QImage full(fullPath), crop(cropPath);
        require(full.size() == fixture.size(), "full image PNG changed the source size");
        require(!crop.isNull(), "marked PNG action did not save an image");
        require(crop.width() < full.width(), "marked PNG was not a crop");
        const QString csvPath = files.filePath(QStringLiteral("results.csv"));
        require(saveThroughDialog(csv, csvPath), "CSV save dialog missing");
        require(QFile::exists(csvPath), "visible CSV action did not create its selected file");
        QFile exported(csvPath);
        require(exported.open(QIODevice::ReadOnly), "visible CSV output cannot be read");
        require(exported.readAll().contains("bolt"), "visible CSV output omitted the history row");
        const QString cancelled = files.filePath(QStringLiteral("cancelled.csv"));
        require(saveThroughDialog(csv, cancelled, true) && !QFile::exists(cancelled),
                "cancelled CSV dialog wrote a file");

        const QString brokenPath = files.filePath(QStringLiteral("broken.png"));
        QFile broken(brokenPath);
        require(broken.open(QIODevice::WriteOnly | QIODevice::Truncate), "broken fixture unavailable");
        broken.write("this is not a PNG"); broken.close();
        const QString sourceBeforeError = source->text();
        require(openBrokenThroughDialog(named<QPushButton>(window, "openImageButton"), brokenPath),
                "broken image was not rejected through the visible Open action");
        require(window.historyCount() == 1 && source->text() == sourceBeforeError && !canvas->isVisible(),
                "broken image damaged the current session");

        // Calibration asks for the known value after drawing and keeps old scale on Cancel.
        mode->setCurrentIndex(static_cast<int>(ruler::Mode::Calibrate));
        QTest::mouseClick(measure, Qt::LeftButton);
        require(waitUntil([&] { return canvas->isVisible(); }),
                "calibration did not let the user draw first");
        QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier,
                          widgetPoint(canvas, {100, 150}, imageSize));
        QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier,
                          widgetPoint(canvas, {300, 150}, imageSize));
        require(finishCalibration(named<QToolButton>(*canvas, "canvasDoneButton"), 100, {}, true),
                "blank unit was accepted or calibration dialog could not be cancelled");
        require(waitUntil([&] { return window.isVisible() && !canvas->isVisible(); }) &&
                window.historyCount() == 1 && source->text() == sourceBeforeError &&
                !named<QPushButton>(window, "resetScaleButton")->isEnabled(),
                "cancelled calibration changed the scale or existing result");
        QTest::mouseClick(measure, Qt::LeftButton);
        require(waitUntil([&] { return canvas->isVisible(); }),
                "calibration could not be repeated after Cancel");
        QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier,
                          widgetPoint(canvas, {100, 150}, imageSize));
        QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier,
                          widgetPoint(canvas, {300, 150}, imageSize));
        require(finishCalibration(named<QToolButton>(*canvas, "canvasDoneButton"), 100,
                                  QStringLiteral("mm")), "valid calibration dialog did not apply");
        require(waitUntil([&] { return window.isVisible() && !canvas->isVisible(); }) &&
                window.historyCount() == 1 &&
                named<QPushButton>(window, "resetScaleButton")->isEnabled() &&
                named<QLabel>(window, "statusMessage")->text() == QStringLiteral("Масштаб установлен"),
                "valid calibration did not apply the measured scale");

        mode->setCurrentIndex(static_cast<int>(ruler::Mode::Path));
        QTest::mouseClick(measure, Qt::LeftButton);
        require(waitUntil([&] { return canvas->isVisible(); }),
                "measurement after calibration did not start");
        QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier,
                          widgetPoint(canvas, {100, 150}, imageSize));
        QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier,
                          widgetPoint(canvas, {300, 150}, imageSize));
        QTest::mouseClick(named<QToolButton>(*canvas, "canvasDoneButton"), Qt::LeftButton);
        require(waitUntil([&] { return window.isVisible() && !canvas->isVisible(); }) &&
                window.historyCount() == 2 && label->text() == QStringLiteral("2") &&
                history->item(0, 0)->text() == QStringLiteral("bolt") &&
                result->text().contains(QStringLiteral("100.00 mm")),
                "new result reused the previous name or ignored calibration");

        // Language switching updates the visible action and source/result descriptions.
        language->setCurrentIndex(1);
        require(measure->text() == QStringLiteral("Next measurement") &&
                source->text().startsWith(QStringLiteral("Source:")) &&
                result->text().startsWith(QStringLiteral("Latest result:")) &&
                !label->accessibleName().isEmpty(), "English UI state is incomplete");
        language->setCurrentIndex(0);
        require(result->text().startsWith(QStringLiteral("Последний результат:")),
                "Russian result heading not restored");

        window.hide();
        canvas->hide();
        std::cout << "workflow tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << "workflow test failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
