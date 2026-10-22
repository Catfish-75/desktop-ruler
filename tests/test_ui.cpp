#include "../canvas.h"
#include "../window.h"
#include <QApplication>
#include <QClipboard>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QPainter>
#include <QPushButton>
#include <QTest>
#include <QTemporaryDir>
#include <QTableWidget>
#include <QTimer>
#include <cstdio>
#include <optional>
#define CHECK(condition) do { if (!(condition)) { std::fprintf(stderr, "test_ui failed at line %d: %s\n", __LINE__, #condition); return 1; } } while (false)
#include <cmath>

static QPoint widgetPoint(Canvas* canvas, QPointF imagePoint, QSize imageSize) {
    const QSizeF drawn = imageSize.scaled(canvas->size(), Qt::KeepAspectRatio);
    const QPointF origin((canvas->width()-drawn.width())/2, (canvas->height()-drawn.height())/2);
    return (origin+QPointF(imagePoint.x()*drawn.width()/imageSize.width(),
                            imagePoint.y()*drawn.height()/imageSize.height())).toPoint();
}
static void click(Canvas* canvas, QPointF imagePoint, QSize imageSize) {
    QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, widgetPoint(canvas,imagePoint,imageSize));
}

static bool finishCalibration(Canvas* canvas, std::optional<QPoint> rightClick = std::nullopt) {
    bool sawDialog = false;
    QTimer responder;
    responder.setInterval(10);
    QObject::connect(&responder, &QTimer::timeout, [&] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        if (!dialog || dialog->objectName() != QStringLiteral("calibrationDialog") || sawDialog) return;
        sawDialog = true;
        if (auto* buttons = dialog->findChild<QDialogButtonBox*>())
            buttons->button(QDialogButtonBox::Ok)->click();
    });
    QTimer bailout;
    bailout.setSingleShot(true);
    QObject::connect(&bailout, &QTimer::timeout, [] {
        if (auto* modal = QApplication::activeModalWidget()) modal->close();
    });
    bailout.start(4000);
    responder.start();
    if (rightClick) QTest::mouseClick(canvas, Qt::RightButton, Qt::NoModifier, *rightClick);
    else QTest::keyClick(canvas, Qt::Key_Return);
    responder.stop();
    bailout.stop();
    return sawDialog;
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    Canvas canvas;
    canvas.resize(400, 300);
    QImage blank(400, 300, QImage::Format_ARGB32);
    blank.fill(Qt::white);
    canvas.setImage(blank);
    canvas.setMode(ruler::Mode::Path);
    int completed = 0;
    double length = 0;
    canvas.setOnComplete([&](ruler::Measurement value, QVector<QPointF>, QImage) {
        ++completed;
        length = value.length;
    });
    canvas.show();
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(10, 10));
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(13, 14));
    QTest::keyClick(&canvas, Qt::Key_Return);
    CHECK(completed == 1 && std::abs(length - 5) < 1e-9);
    QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(10, 10));
    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(13, 14));
    QTest::keyClick(&canvas, Qt::Key_Return);
    CHECK(completed == 2 && std::abs(length - 5) < 1e-9);

    RulerWindow window;
    window.setKnownLength(10, QStringLiteral("m"));
    window.selectMode(ruler::Mode::Calibrate);
    window.show();
    window.openImage(blank);
    auto* c = window.canvas();
    CHECK(QTest::qWaitForWindowExposed(c));
    click(c, {10,10}, blank.size());
    click(c, {13,14}, blank.size());
    CHECK(finishCalibration(c));
    CHECK(window.historyCount() == 0);

    window.selectMode(ruler::Mode::Path);
    window.nextMeasurement();
    click(c, {10,10}, blank.size());
    click(c, {13,14}, blank.size());
    QTest::keyClick(c, Qt::Key_Return);
    CHECK(window.historyCount() == 1);
    window.setLabel(QStringLiteral("=critical"));
    auto* history = window.findChild<QTableWidget*>();
    CHECK(history && history->item(0, 0) && history->item(0, 1));
    CHECK(history->item(0, 0)->text() == QStringLiteral("=critical"));
    CHECK(history->item(0, 0)->flags() & Qt::ItemIsEditable);
    CHECK(!(history->item(0, 1)->flags() & Qt::ItemIsEditable));
    history->item(0, 0)->setText(QStringLiteral("=edited"));
    window.copyResults();
    const QString copied = QGuiApplication::clipboard()->text();
    const QStringList firstRow = copied.split('\n').at(1).split('\t');
    CHECK(firstRow.size() == 10);
    CHECK(firstRow[0] == QStringLiteral("'=edited"));
    CHECK(std::abs(firstRow[2].toDouble()-5) < 1e-9);
    CHECK(std::abs(firstRow[6].toDouble()-2) < 1e-9);
    CHECK(std::abs(firstRow[8].toDouble()-10) < 1e-9);

    QTemporaryDir files;
    CHECK(files.isValid());
    const QString csv = files.filePath(QStringLiteral("results.csv"));
    const QString png = files.filePath(QStringLiteral("marked.png"));
    CHECK(window.exportCsv(csv));
    CHECK(window.saveSelectedImage(png));
    QFile exported(csv); CHECK(exported.open(QIODevice::ReadOnly));
    CHECK(exported.readAll().contains("'=edited"));
    QImage marked(png); CHECK(!marked.isNull() && marked.width() < blank.width());

    window.nextMeasurement();
    click(c, {20,20}, blank.size());
    QTest::keyClick(c, Qt::Key_Escape);
    CHECK(window.historyCount() == 1);
    CHECK(!c->isVisible());

    window.selectMode(ruler::Mode::Area);
    window.nextMeasurement();
    for (QPointF p : {QPointF(20,20), QPointF(120,20), QPointF(120,70), QPointF(20,70)})
        click(c,p,blank.size());
    QTest::keyClick(c, Qt::Key_Return);
    CHECK(window.historyCount() == 2);
    const QString areaCsv = files.filePath(QStringLiteral("area.csv"));
    CHECK(window.exportCsv(areaCsv, true));
    QFile areaFile(areaCsv); CHECK(areaFile.open(QIODevice::ReadOnly));
    CHECK(areaFile.readAll().contains("20000"));

    window.setKnownLength(5, QStringLiteral("cm"));
    window.selectMode(ruler::Mode::Calibrate);
    window.nextMeasurement();
    click(c,{10,10},blank.size()); click(c,{13,14},blank.size());
    CHECK(finishCalibration(c));
    CHECK(window.historyCount() == 2);
    window.copyResults();
    const QStringList afterScale = QGuiApplication::clipboard()->text().trimmed().split('\n');
    CHECK(afterScale.size() == 3);
    const QStringList preserved = afterScale[1].split('\t');
    CHECK(std::abs(preserved[6].toDouble()-2) < 1e-9);
    CHECK(std::abs(preserved[8].toDouble()-10) < 1e-9);

    window.selectMode(ruler::Mode::Perimeter);
    window.nextMeasurement();
    for (QPointF p : {QPointF(20,20), QPointF(120,20), QPointF(120,70), QPointF(20,70)})
        click(c,p,blank.size());
    QTest::mouseClick(c, Qt::RightButton, Qt::NoModifier, widgetPoint(c,{20,70},blank.size()));
    CHECK(window.historyCount() == 3);

    window.selectMode(ruler::Mode::Angle);
    window.nextMeasurement();
    click(c,{50,50},blank.size()); click(c,{80,50},blank.size()); click(c,{50,80},blank.size());
    CHECK(window.historyCount() == 3);
    QTest::mouseClick(c, Qt::RightButton, Qt::NoModifier, widgetPoint(c,{50,80},blank.size()));
    CHECK(window.historyCount() == 4);

    window.selectMode(ruler::Mode::Count);
    window.nextMeasurement();
    click(c,{40,40},blank.size()); click(c,{60,60},blank.size());
    QTest::keyClick(c, Qt::Key_Backspace);
    QTest::keyClick(c, Qt::Key_Return);
    CHECK(window.historyCount() == 5);

    QPainter painter(&blank); painter.fillRect(200,100,20,20,Qt::black); painter.end();
    window.selectMode(ruler::Mode::AutoArea);
    window.openImage(blank);
    click(c,{205,105},blank.size());
    CHECK(window.historyCount() == 5);
    QTest::keyClick(c, Qt::Key_Return);
    CHECK(window.historyCount() == 6);

    window.selectMode(ruler::Mode::Magnifier);
    window.nextMeasurement();
    QTest::mouseMove(c, widgetPoint(c,{300,200},blank.size()));
    QTest::keyClick(c, Qt::Key_Escape);
    CHECK(window.historyCount() == 6 && !c->isVisible());

    Canvas magnifier;
    magnifier.resize(400, 300);
    QImage scaledPixels(500, 375, QImage::Format_ARGB32);
    scaledPixels.fill(Qt::black);
    scaledPixels.setPixelColor(102, 102, Qt::red);
    scaledPixels.setPixelColor(103, 103, Qt::green);
    magnifier.setImage(scaledPixels);
    magnifier.setMode(ruler::Mode::Magnifier);
    magnifier.show();
    CHECK(QTest::qWaitForWindowExposed(&magnifier));
    QTest::mouseClick(&magnifier, Qt::LeftButton, Qt::NoModifier, QPoint(82, 82));
    const QImage magnified = magnifier.grab().toImage();
    // 82/400 maps to image coordinate 102.5: the cursor is inside source pixel 102.
    // Sample inside that enlarged pixel, clear of the center crosshair.
    const QColor centerPixel = magnified.pixelColor(298, 198);
    CHECK(centerPixel.red() > 200 && centerPixel.green() < 40);
    magnifier.hide();

    // One left point, a visible preview segment, and right click must reach
    // the calibration length/unit dialog through the real window callback.
    window.selectMode(ruler::Mode::Calibrate);
    window.nextMeasurement();
    click(c, {20, 20}, blank.size());
    QTest::mouseMove(c, widgetPoint(c, {120, 20}, blank.size()));
    CHECK(finishCalibration(c, widgetPoint(c, {120, 20}, blank.size())));
    CHECK(window.historyCount() == 6);

    window.clearHistory();
    CHECK(window.historyCount() == 0);
    return 0;
}
