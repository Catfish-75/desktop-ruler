#include "../canvas.h"

#include <QApplication>
#include <QMouseEvent>
#include <QToolButton>
#include <QTest>
#include <cmath>
#include <cstdio>

#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "test_canvas_regression failed at line %d: %s\n", __LINE__, #condition); \
    return 1; \
} } while (false)

static void settle() {
    QApplication::processEvents();
}

static void sendHeldMove(QWidget* target, QPoint to) {
    QMouseEvent move(QEvent::MouseMove, QPointF(to), target->mapToGlobal(to),
                     Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(target, &move);
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    Canvas canvas;
    canvas.resize(400, 300);
    QImage pixels(400, 300, QImage::Format_ARGB32);
    pixels.fill(Qt::black);
    pixels.setPixelColor(350, 250, Qt::red);
    canvas.setImage(pixels);
    canvas.setHudText(QStringLiteral("Measurement"));
    canvas.setLanguage(true);
    canvas.show();
    CHECK(QTest::qWaitForWindowExposed(&canvas));
    CHECK(canvas.cursor().shape() == Qt::CrossCursor);

    auto* done = canvas.findChild<QToolButton*>(QStringLiteral("canvasDoneButton"));
    auto* undo = canvas.findChild<QToolButton*>(QStringLiteral("canvasUndoButton"));
    auto* cancel = canvas.findChild<QToolButton*>(QStringLiteral("canvasCancelButton"));
    auto* controls = canvas.findChild<QWidget*>(QStringLiteral("canvasControls"));
    CHECK(done && undo && cancel && controls && controls->isVisible());
    CHECK(done->text() == QStringLiteral("Done"));
    CHECK(undo->text() == QStringLiteral("Undo"));
    CHECK(cancel->text() == QStringLiteral("Cancel"));
    CHECK(controls->geometry().right() >= canvas.width()-18);
    CHECK(controls->geometry().top() >= 8 && controls->geometry().top() <= 12);

    canvas.setMode(ruler::Mode::Path);
    settle();
    const QImage pathHud = canvas.grab().toImage();
    canvas.setMode(ruler::Mode::Angle);
    settle();
    const QImage angleHud = canvas.grab().toImage();
    CHECK(pathHud != angleHud);

    canvas.setLanguage(false);
    CHECK(done->text() == QStringLiteral("Готово"));
    CHECK(undo->text() == QStringLiteral("Отменить"));
    CHECK(cancel->text() == QStringLiteral("Назад"));
    CHECK(controls->geometry().right() >= canvas.width()-18);
    canvas.setLanguage(true);

    int errors = 0;
    QString lastError;
    int completed = 0;
    double length = 0;
    double angleResult = 0;
    int countResult = 0;
    int cancelled = 0;
    QVector<QPointF> completedPoints;
    canvas.setOnError([&](QString message) { ++errors; lastError = std::move(message); });
    canvas.setOnComplete([&](ruler::Measurement value, QVector<QPointF> points, QImage) {
        ++completed; length = value.length; angleResult = value.angle; countResult = value.count;
        completedPoints = std::move(points);
    });
    canvas.setOnCancel([&] { ++cancelled; });

    // The last visible preview segment becomes the endpoint when right-click
    // finishes calibration. Re-clicking an existing endpoint must not add it
    // twice, and a zero-length attempt must remain invalid.
    canvas.setMode(ruler::Mode::Calibrate);
    const int completeBeforeRightClick = completed;
    const int errorsBeforeRightClick = errors;
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(20, 100));
    QTest::mouseMove(&canvas, QPoint(320, 100));
    QTest::mouseClick(&canvas, Qt::RightButton, Qt::NoModifier, QPoint(320, 100));
    CHECK(completed == completeBeforeRightClick + 1 && errors == errorsBeforeRightClick);
    CHECK(std::abs(length - 300) < 1e-9 && completedPoints.size() == 2);

    canvas.setMode(ruler::Mode::Calibrate);
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(20, 100));
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(320, 100));
    QTest::mouseClick(&canvas, Qt::RightButton, Qt::NoModifier, QPoint(320, 100));
    CHECK(completed == completeBeforeRightClick + 2 && errors == errorsBeforeRightClick);
    CHECK(std::abs(length - 300) < 1e-9 && completedPoints.size() == 2);

    canvas.setMode(ruler::Mode::Calibrate);
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(20, 100));
    QTest::mouseClick(&canvas, Qt::RightButton, Qt::NoModifier, QPoint(20, 100));
    CHECK(completed == completeBeforeRightClick + 2 && errors == errorsBeforeRightClick + 1);

    canvas.setMode(ruler::Mode::Calibrate);
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(20, 40));
    QTest::mouseClick(&canvas, Qt::RightButton, Qt::NoModifier, controls->geometry().center());
    CHECK(completed == completeBeforeRightClick + 2 && errors == errorsBeforeRightClick + 2);

    canvas.setMode(ruler::Mode::Area);
    const int errorsBeforeArea = errors;
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(20, 20));
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(120, 120));
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(20, 120));
    QTest::mouseClick(&canvas, Qt::RightButton, Qt::NoModifier, QPoint(120, 20));
    CHECK(completed == completeBeforeRightClick + 2 && errors == errorsBeforeArea + 1);
    canvas.undo();
    QTest::mouseClick(done, Qt::LeftButton);
    CHECK(completed == completeBeforeRightClick + 3 && completedPoints.size() == 3);

    canvas.setMode(ruler::Mode::Angle);
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(100, 100));
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(200, 100));
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(100, 200));
    CHECK(completed == completeBeforeRightClick + 3);
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(200, 200));
    QTest::mouseClick(&canvas, Qt::RightButton, Qt::NoModifier, QPoint(200, 200));
    CHECK(completed == completeBeforeRightClick + 4 && completedPoints.size() == 3);
    CHECK(std::abs(angleResult - 90) < 1e-9);

    canvas.setMode(ruler::Mode::Angle);
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(100, 100));
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(200, 100));
    QTest::mouseClick(&canvas, Qt::RightButton, Qt::NoModifier, QPoint(100, 200));
    CHECK(completed == completeBeforeRightClick + 5 && completedPoints.size() == 3);
    CHECK(std::abs(angleResult - 90) < 1e-9);
    canvas.setMode(ruler::Mode::Path);
    completed = 0;
    errors = 0;

    // Switching modes during a held path stroke must not carry its drawing
    // state into count mode or add a phantom mark on the next mouse move.
    canvas.setMode(ruler::Mode::Path);
    QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(20, 40));
    canvas.setMode(ruler::Mode::Count);
    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(20, 40));
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(40, 50));
    QMouseEvent heldMove(QEvent::MouseMove, QPointF(60, 50), canvas.mapToGlobal(QPoint(60, 50)),
                         Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&canvas, &heldMove);
    QTest::mouseClick(done, Qt::LeftButton);
    CHECK(completed == 1 && countResult == 1);
    completed = 0;

    // Maximum zoom remains visible in a 400x300 canvas and keeps each source
    // pixel at 24x24; the reduced source window must not silently disappear.
    canvas.setMode(ruler::Mode::Magnifier);
    canvas.setMagnifierZoom(24);
    QTest::mouseMove(&canvas, QPoint(349, 250));
    QTest::mouseMove(&canvas, QPoint(350, 250));
    settle();
    const QImage magnified = canvas.grab().toImage();
    CHECK(magnified.pixelColor(130, 182).red() > 200);
    CHECK(magnified.pixelColor(137, 182) == QColor(Qt::black));

    // Changing mode or image clears a stale cursor/lens.
    canvas.setMode(ruler::Mode::Path);
    canvas.setMode(ruler::Mode::Magnifier);
    settle();
    CHECK(canvas.grab().toImage().pixelColor(130, 182) == QColor(Qt::black));
    QTest::mouseMove(&canvas, QPoint(349, 250));
    QTest::mouseMove(&canvas, QPoint(350, 250));
    settle();
    CHECK(canvas.grab().toImage().pixelColor(130, 182).red() > 200);
    QImage replacement(400, 300, QImage::Format_ARGB32);
    replacement.fill(Qt::black);
    canvas.setImage(replacement);
    settle();
    CHECK(canvas.grab().toImage().pixelColor(130, 182) == QColor(Qt::black));

    // Magnifier completion actions leave the canvas via the normal cancel path.
    QTest::keyClick(&canvas, Qt::Key_Return);
    CHECK(cancelled == 1 && completed == 0);
    canvas.setMode(ruler::Mode::Magnifier);
    QTest::mouseClick(&canvas, Qt::RightButton, Qt::NoModifier, QPoint(90, 100));
    CHECK(cancelled == 2 && completed == 0);
    QTest::mouseClick(done, Qt::LeftButton);
    CHECK(cancelled == 3 && completed == 0);
    canvas.setMode(ruler::Mode::Magnifier);
    QTest::mouseClick(cancel, Qt::LeftButton);
    CHECK(cancelled == 4 && completed == 0);

    // Finishing automatic area before choosing a seed reports a visible error.
    canvas.setMode(ruler::Mode::AutoArea);
    QTest::mouseClick(done, Qt::LeftButton);
    CHECK(errors == 1);
    CHECK(lastError == QStringLiteral("At least one point is required"));

    // The visible Undo button and Done button provide a complete mouse-only path.
    canvas.setImage(replacement);
    canvas.setMode(ruler::Mode::Path);
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(40, 60));
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(50, 60));
    CHECK(undo->isEnabled());
    QTest::mouseClick(undo, Qt::LeftButton);
    QTest::mouseClick(done, Qt::LeftButton);
    CHECK(errors == 2 && completed == 0);
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(50, 60));
    QTest::mouseClick(done, Qt::LeftButton);
    CHECK(completed == 1 && length == 10);

    // Region screenshots accept either opposite-corner clicks or a drag,
    // crop the original image with half-open pixel bounds, and return to the
    // regular measurement controls after saving.
    QImage coordinateImage(400, 300, QImage::Format_ARGB32);
    for (int y = 0; y < coordinateImage.height(); ++y)
        for (int x = 0; x < coordinateImage.width(); ++x)
            coordinateImage.setPixelColor(x, y, QColor(x % 256, y % 256, (x+y) % 256));
    canvas.setImage(coordinateImage);
    canvas.setMode(ruler::Mode::Path);
    int screenshots = 0;
    QImage screenshot;
    canvas.setOnScreenshot([&](QImage crop) { ++screenshots; screenshot = std::move(crop); });
    const int cancelBeforeScreenshots = cancelled;
    const int errorBeforeScreenshots = errors;
    canvas.setScreenshotSelection(true);
    CHECK(done->text() == QStringLiteral("Save region"));
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(100, 80));
    CHECK(screenshots == 0);
    QTest::mouseClick(done, Qt::LeftButton);
    CHECK(errors == errorBeforeScreenshots+1 && screenshots == 0);
    CHECK(undo->isEnabled()); // failed finish keeps the first corner editable
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(200, 180));
    QTest::keyClick(&canvas, Qt::Key_Return);
    CHECK(screenshots == 1 && screenshot.size() == QSize(100, 100));
    CHECK(screenshot.pixelColor(0, 0) == coordinateImage.pixelColor(100, 80));
    CHECK(screenshot.pixelColor(99, 99) == coordinateImage.pixelColor(199, 179));
    CHECK(cancelled == cancelBeforeScreenshots);
    CHECK(done->text() == QStringLiteral("Done"));

    // A zero-size selection is rejected without discarding it; Escape exits
    // the temporary selection state, so later Done is ordinary measurement.
    canvas.setScreenshotSelection(true);
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(120, 90));
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(120, 90));
    QTest::mouseClick(done, Qt::LeftButton);
    CHECK(errors == errorBeforeScreenshots+2 && screenshots == 1);
    CHECK(done->text() == QStringLiteral("Save region") && undo->isEnabled());
    QTest::keyClick(&canvas, Qt::Key_Escape);
    CHECK(cancelled == cancelBeforeScreenshots+1);
    CHECK(done->text() == QStringLiteral("Done"));
    QTest::mouseClick(done, Qt::LeftButton);
    CHECK(screenshots == 1 && errors == errorBeforeScreenshots+3);

    // Reverse-direction drag uses the same normalized half-open crop bounds.
    canvas.setLanguage(false);
    canvas.setScreenshotSelection(true);
    CHECK(done->text() == QStringLiteral("Сохранить область"));
    QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(300, 220));
    sendHeldMove(&canvas, QPoint(250, 190));
    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(250, 190));
    QTest::mouseClick(&canvas, Qt::RightButton, Qt::NoModifier, QPoint(250, 190));
    CHECK(screenshots == 2 && screenshot.size() == QSize(50, 30));
    CHECK(screenshot.pixelColor(0, 0) == coordinateImage.pixelColor(250, 190));
    CHECK(screenshot.pixelColor(49, 29) == coordinateImage.pixelColor(299, 219));
    CHECK(done->text() == QStringLiteral("Готово"));

    // Tab reveals image pixels hidden by the toolbar. A click at the old
    // toolbar center must survive reopening the controls and completing the
    // measurement; changing source or mode restores the toolbar.
    canvas.setLanguage(true);
    canvas.setMode(ruler::Mode::Path);
    CHECK(controls->isVisible());
    const QPoint formerlyCovered = controls->geometry().center();
    CHECK(canvas.rect().contains(formerlyCovered));
    canvas.setFocus();
    QTest::keyClick(&canvas, Qt::Key_Tab);
    CHECK(!controls->isVisible());
    completed = 0;
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, formerlyCovered);
    CHECK(completed == 0);
    QTest::keyClick(&canvas, Qt::Key_Tab);
    CHECK(controls->isVisible());
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(40, 100));
    QTest::mouseClick(done, Qt::LeftButton);
    CHECK(completed == 1 && completedPoints.size() == 2);
    CHECK(std::abs(completedPoints.first().x()-formerlyCovered.x()) < 0.01);
    CHECK(std::abs(completedPoints.first().y()-formerlyCovered.y()) < 0.01);

    QTest::keyClick(&canvas, Qt::Key_Tab);
    CHECK(!controls->isVisible());
    canvas.setMode(ruler::Mode::Count);
    CHECK(controls->isVisible());
    QTest::keyClick(&canvas, Qt::Key_Tab);
    CHECK(!controls->isVisible());
    canvas.setImage(replacement);
    CHECK(controls->isVisible());

    return 0;
}
