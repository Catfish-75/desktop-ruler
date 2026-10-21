#pragma once
#include "canvas.h"
#include "shortcuts.h"

#include <QMainWindow>
#include <QImage>
#include <QVector>

class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QLineEdit;
class QLabel;
class QPushButton;
class QTableWidget;
class QSystemTrayIcon;
class QAction;
class QCloseEvent;
class QWidget;
class QDialog;
class QDialogButtonBox;

class RulerWindow final : public QMainWindow {
public:
    explicit RulerWindow(QWidget* parent = nullptr);
    ~RulerWindow() override;
    Canvas* canvas() const { return canvas_; }
    void openImage(const QImage& image);
    bool openImageFile(const QString& path);
    bool exportCsv(const QString& path, bool selectedOnly = false);
    bool saveSelectedImage(const QString& path);
    bool saveSourceImage(const QString& path);
    void copyResults(bool selectedOnly = false);
    int historyCount() const;
    void setKnownLength(double length, const QString& unit);
    void setLabel(const QString& label);
    void selectMode(ruler::Mode mode);
    void nextMeasurement();
    void clearHistory();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    struct Entry {
        QString name, unit;
        ruler::Mode kind;
        ruler::Measurement raw;
        double scale;
        QImage screenshot;
    };
    void capture();
    void populateScreens();
    void onComplete(ruler::Measurement measurement, QVector<QPointF> points, QImage mask);
    QString resultText(const Entry& entry) const;
    QString tableText(bool selectedOnly, bool csv) const;
    void refreshHistory();
    void showImage();
    void saveImage();
    void exportDialog();
    void saveSourceDialog();
    void selectScreenshotRegion();
    void quitApplication();
    void updateModeControls();
    void updateAvailability();
    void updateHints();
    void showHistory();
    void prepareCanvas();
    void showControls();
    QString modeHint(ruler::Mode mode) const;
    void setEnglish(bool english);
    void showHelp();
    QString displayError(const QString& error) const;
    QString trText(const char* ru, const char* en) const;
    Canvas* canvas_;
    QComboBox *screenBox_, *modeBox_, *languageBox_;
    QDoubleSpinBox* knownLength_;
    QSpinBox *tolerance_, *zoom_;
    QLineEdit *unit_, *label_;
    QLabel *status_, *scaleLabel_, *sourceLabel_, *resultLabel_, *hintLabel_;
    QLabel *modeLabel_, *nameLabel_, *knownLabel_, *unitLabel_, *toleranceLabel_, *zoomLabel_;
    QLabel *calibrationPixels_, *calibrationError_;
    QWidget *resultWidget_, *historyPanel_, *toleranceWidget_, *zoomWidget_;
    QDialog* calibrationDialog_;
    QDialogButtonBox* calibrationButtons_;
    QTableWidget* history_;
    QPushButton *captureButton_, *openButton_, *measureButton_, *historyButton_, *optionsButton_, *resetButton_,
        *showButton_, *saveButton_, *saveSourceButton_, *regionButton_, *copyButton_, *copyAllButton_,
        *csvButton_, *clearButton_, *helpButton_, *shortcutsButton_, *hideButton_, *quitButton_;
    QSystemTrayIcon* tray_ = nullptr;
    QAction *trayShow_ = nullptr, *trayCapture_ = nullptr, *trayHide_ = nullptr, *trayQuit_ = nullptr;
    QVector<Entry> entries_;
    QImage image_;
    double scale_ = 1.0;
    QString scaleUnit_ = QStringLiteral("px");
    QString sourceName_;
    ruler::Mode activeMode_ = ruler::Mode::Path;
    bool optionsExpanded_ = false;
    bool quitRequested_ = false;
    bool capturePending_ = false;
    bool english_ = false;
    ruler::ShortcutController shortcuts_;
};
