// Optional native Wayland acceptance: real grim capture of a synthetic window.
// Run only in an interactive grim-compatible session; temporarily uses the screen.
#include "../window.h"
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QTest>
#include <cmath>
#include <iostream>
#include <stdexcept>

static void require(bool value, const char* reason) {
    if (!value) throw std::runtime_error(reason);
}

class Pattern final : public QWidget {
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.fillRect(rect(), QColor(245, 247, 250));
        p.fillRect(QRect(500, 200, 200, 200), QColor(195, 35, 160));
        p.setPen(QPen(QColor(40, 85, 130), 3));
        p.drawLine(QPoint(100, 100), QPoint(400, 500));
        p.setFont(QFont(QStringLiteral("Sans"), 20));
        p.drawText(QPoint(80, 70), QStringLiteral("Desktop Ruler — native capture fixture"));
        p.drawText(QPoint(80, 560), QStringLiteral("300 × 400 → 500 logical pixels"));
    }
};

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("desktop-ruler-capture-test"));
    app.setQuitOnLastWindowClosed(false);
    if (QGuiApplication::platformName() != QStringLiteral("wayland")) {
        std::cerr << "Native capture test requires QT_QPA_PLATFORM=wayland\n";
        return 2;
    }
    try {
        const QString out = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QStringLiteral("artifacts");
        QDir().mkpath(out);
        Pattern pattern;
        pattern.setWindowTitle(QStringLiteral("Desktop Ruler acceptance pattern"));
        pattern.showFullScreen();
        require(QTest::qWaitForWindowExposed(&pattern), "pattern not exposed");
        QTest::qWait(700);
        RulerWindow window;
        window.show();
        require(QTest::qWaitForWindowExposed(&window), "controls not exposed");
        QTest::qWait(400);
        // Mapping controls can demote the previous fullscreen client in a tiling WM.
        // Re-establish the synthetic background before invoking the capture action.
        pattern.setWindowState(Qt::WindowNoState);
        pattern.showFullScreen();
        QTest::qWait(500);
        require(pattern.size() == pattern.screen()->size(), "fixture is not fullscreen");
        QPushButton* capture = nullptr;
        for (auto* button : window.findChildren<QPushButton*>())
            if (button->text() == QStringLiteral("Захват экрана") ||
                button->text() == QStringLiteral("Capture screen")) capture = button;
        require(capture, "capture button missing");
        QTest::mouseClick(capture, Qt::LeftButton);
        Canvas* canvas = window.canvas();
        for (int i = 0; i < 100 && !canvas->isVisible(); ++i) QTest::qWait(100);
        require(canvas->isVisible(), "real capture did not open canvas");
        require(QTest::qWaitForWindowExposed(canvas), "canvas not exposed");
        QTest::qWait(500);
        require(canvas->size() == canvas->screen()->size(), "canvas is not full output size");
        QImage view = canvas->grab().toImage();
        const qreal dpr = view.devicePixelRatio();
        const QColor actual = view.pixelColor(qRound(600*dpr), qRound(300*dpr));
        view.save(out+QStringLiteral("/capture-source.png"));
        std::cout << "Canvas " << canvas->width() << 'x' << canvas->height()
                  << " DPR " << dpr << " source color " << actual.name().toStdString() << '\n';
        require(std::abs(actual.red()-195) < 4 && std::abs(actual.blue()-160) < 4,
                "grim did not capture the known underlying pattern");
        QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(100,100));
        QTest::mouseMove(canvas, QPoint(400,500));
        QTest::qWait(150);
        require(canvas->grab().save(out+QStringLiteral("/native-overlay.png")), "overlay save failed");
        QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(400,500));
        QTest::keyClick(canvas, Qt::Key_Return);
        require(window.historyCount() == 1, "measurement not in history");
        window.copyResults();
        const QStringList lines = QGuiApplication::clipboard()->text().split('\n');
        require(lines.size() >= 2, "clipboard missing result");
        const QStringList cells = lines[1].split('\t');
        require(cells.size() == 10, "clipboard schema changed");
        const double expected = 500 * canvas->screen()->devicePixelRatio();
        require(std::abs(cells[2].toDouble()-expected) < 0.01, "native distance is not 500 physical pixels at scale 1");
        require(window.saveSelectedImage(out+QStringLiteral("/native-measurement.png")), "annotated image save failed");
        require(window.exportCsv(out+QStringLiteral("/native-results.csv")), "native CSV save failed");
        QTest::qWait(250);
        require(window.grab().save(out+QStringLiteral("/native-controls.png")), "controls save failed");
        window.nextMeasurement();
        QTest::qWait(100);
        QTest::keyClick(canvas, Qt::Key_Escape);
        require(!canvas->isVisible() && window.isVisible(), "Escape did not restore controls");
        std::cout << "Native Wayland PASS: actual grim capture, known source pixels, fullscreen, "
                  << expected << " px path, history, clipboard, PNG, CSV, repeat, Escape\n";
        window.close();
        pattern.close();
    } catch (const std::exception& error) {
        std::cerr << "Native capture FAIL: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
