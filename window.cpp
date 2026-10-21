#include "window.h"

#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGuiApplication>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QPointer>
#include <QPushButton>
#include <QSaveFile>
#include <QScreen>
#include <QScrollArea>
#include <QSpinBox>
#include <QSystemTrayIcon>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>
#include <QBuffer>
#include <QDialog>
#include <QAction>

#include <algorithm>
#include <cmath>

namespace {
QString modeName(ruler::Mode mode, bool en) {
    switch (mode) {
    case ruler::Mode::Path: return en ? "Path / freehand" : "Линия / путь";
    case ruler::Mode::Area: return en ? "Area and perimeter" : "Площадь и периметр";
    case ruler::Mode::Perimeter: return en ? "Perimeter" : "Периметр";
    case ruler::Mode::AutoArea: return en ? "Auto color area" : "Область по цвету";
    case ruler::Mode::Angle: return en ? "Angle" : "Угол";
    case ruler::Mode::Count: return en ? "Count" : "Подсчёт";
    case ruler::Mode::Calibrate: return en ? "Calibrate" : "Калибровка";
    case ruler::Mode::Magnifier: return en ? "Magnifier" : "Лупа";
    }
    return {};
}
QString safeCell(QString value) {
    value.replace('\t', ' '); value.replace('\n', ' '); value.replace('\r', ' ');
    const QString trimmed = value.trimmed();
    if (!trimmed.isEmpty() && QStringLiteral("=+-@").contains(trimmed.front())) value.prepend('\'');
    return value;
}
QString csvCell(const QString& value) {
    QString escaped = safeCell(value);
    escaped.replace('"', QStringLiteral("\"\""));
    return QStringLiteral("\"")+escaped+QStringLiteral("\"");
}
}

RulerWindow::RulerWindow(QWidget* parent)
    : QMainWindow(parent), canvas_(new Canvas),
      shortcuts_(this, [this](int action) {
          if (capturePending_ || canvas_->isVisible() || QApplication::activeModalWidget()) return;
          if (action == ruler::CaptureShortcutAction) { capture(); return; }
          if (action == ruler::HistoryShortcutAction) {
              showControls();
              if (!entries_.isEmpty()) history_->selectRow(entries_.size()-1);
              history_->setFocus();
              return;
          }
          if (action >= 0 && action <= static_cast<int>(ruler::Mode::Magnifier)) {
              selectMode(static_cast<ruler::Mode>(action));
              if (image_.isNull()) capture(); else nextMeasurement();
          }
      }) {
    setWindowTitle(QStringLiteral("Desktop Ruler"));
    const QIcon icon = QIcon::fromTheme(QStringLiteral("desktop-ruler"),
                                      QIcon::fromTheme(QStringLiteral("accessories-calculator")));
    setWindowIcon(icon);
    resize(900, 560);
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* central = new QWidget(scroll);
    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(12, 5, 12, 5);
    root->setSpacing(3);
    scroll->setWidget(central);
    setCentralWidget(scroll);

    auto* top = new QHBoxLayout;
    screenBox_ = new QComboBox; screenBox_->setMinimumWidth(150);
    captureButton_ = new QPushButton; openButton_ = new QPushButton;
    languageBox_ = new QComboBox;
    languageBox_->addItems({QStringLiteral("Русский"), QStringLiteral("English")});
    top->addWidget(screenBox_, 1); top->addWidget(captureButton_);
    top->addWidget(openButton_); top->addWidget(languageBox_);
    root->addLayout(top);
    sourceLabel_ = new QLabel;
    root->addWidget(sourceLabel_);

    auto* modeRow = new QHBoxLayout;
    modeBox_ = new QComboBox;
    for (int i=0; i<=static_cast<int>(ruler::Mode::Magnifier); ++i)
        modeBox_->addItem(modeName(static_cast<ruler::Mode>(i), false), i);
    label_ = new QLineEdit; label_->setMaxLength(200);
    modeLabel_ = new QLabel; nameLabel_ = new QLabel;
    optionsButton_ = new QPushButton;
    modeRow->addWidget(modeLabel_); modeRow->addWidget(modeBox_, 2);
    modeRow->addWidget(nameLabel_); modeRow->addWidget(label_, 2);
    modeRow->addWidget(optionsButton_);
    root->addLayout(modeRow);

    knownLength_ = new QDoubleSpinBox; knownLength_->setRange(0.000001, 1e12);
    knownLength_->setDecimals(6); knownLength_->setValue(1);
    unit_ = new QLineEdit(QStringLiteral("mm")); unit_->setMaxLength(32);
    tolerance_ = new QSpinBox; tolerance_->setRange(0,255);
    zoom_ = new QSpinBox; zoom_->setRange(2,24); zoom_->setValue(8);
    knownLabel_ = new QLabel;
    toleranceLabel_ = new QLabel; zoomLabel_ = new QLabel;
    auto* settings = new QHBoxLayout;
    knownWidget_ = new QWidget;
    auto* knownRow = new QHBoxLayout(knownWidget_); knownRow->setContentsMargins(0, 0, 0, 0);
    knownRow->addWidget(knownLabel_); knownRow->addWidget(knownLength_); knownRow->addWidget(unit_);
    toleranceWidget_ = new QWidget;
    auto* toleranceRow = new QHBoxLayout(toleranceWidget_); toleranceRow->setContentsMargins(0, 0, 0, 0);
    toleranceRow->addWidget(toleranceLabel_); toleranceRow->addWidget(tolerance_);
    zoomWidget_ = new QWidget;
    auto* zoomRow = new QHBoxLayout(zoomWidget_); zoomRow->setContentsMargins(0, 0, 0, 0);
    zoomRow->addWidget(zoomLabel_); zoomRow->addWidget(zoom_);
    settings->addWidget(knownWidget_); settings->addWidget(toleranceWidget_);
    settings->addWidget(zoomWidget_); settings->addStretch(1);
    root->addLayout(settings);
    hintLabel_ = new QLabel;
    hintLabel_->setWordWrap(true);
    root->addWidget(hintLabel_);
    scaleLabel_ = new QLabel;
    resetButton_ = new QPushButton;
    auto* scaleLine = new QHBoxLayout;
    scaleLine->addWidget(scaleLabel_, 1); scaleLine->addWidget(resetButton_);
    root->addLayout(scaleLine);

    auto* actions = new QHBoxLayout;
    measureButton_ = new QPushButton;
    actions->addWidget(measureButton_);
    root->addLayout(actions);
    resultLabel_ = new QLabel;
    resultLabel_->setFrameShape(QFrame::StyledPanel);
    resultLabel_->setMargin(5);
    QFont resultFont = resultLabel_->font(); resultFont.setBold(true); resultLabel_->setFont(resultFont);
    resultLabel_->setWordWrap(true);
    root->addWidget(resultLabel_);
    status_ = new QLabel;
    root->addWidget(status_);
    history_ = new QTableWidget(0, 5);
    history_->setSelectionBehavior(QAbstractItemView::SelectRows);
    history_->setSelectionMode(QAbstractItemView::SingleSelection);
    history_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    history_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    history_->setMinimumHeight(60);
    connect(history_, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
        if (item && item->column() == 0 && item->row() >= 0 && item->row() < entries_.size())
            entries_[item->row()].name = item->text().trimmed();
    });
    connect(history_, &QTableWidget::itemSelectionChanged, this, [this] { updateAvailability(); });
    root->addWidget(history_, 1);
    auto* exports = new QGridLayout;
    showButton_ = new QPushButton; saveButton_ = new QPushButton; saveSourceButton_ = new QPushButton;
    regionButton_ = new QPushButton;
    copyButton_ = new QPushButton; copyAllButton_ = new QPushButton;
    csvButton_ = new QPushButton; clearButton_ = new QPushButton;
    exports->addWidget(showButton_, 0, 0);
    exports->addWidget(saveButton_, 0, 1);
    exports->addWidget(saveSourceButton_, 0, 2);
    exports->addWidget(regionButton_, 0, 3);
    exports->addWidget(copyButton_, 1, 0);
    exports->addWidget(copyAllButton_, 1, 1);
    exports->addWidget(csvButton_, 1, 2);
    exports->addWidget(clearButton_, 1, 3);
    for (int column = 0; column < 4; ++column) exports->setColumnStretch(column, 1);
    root->addLayout(exports);
    auto* footer = new QHBoxLayout;
    helpButton_ = new QPushButton; shortcutsButton_ = new QPushButton;
    hideButton_ = new QPushButton; quitButton_ = new QPushButton;
    footer->addWidget(helpButton_); footer->addWidget(shortcutsButton_); footer->addStretch(1);
    footer->addWidget(hideButton_); footer->addWidget(quitButton_);
    root->addLayout(footer);

    auto identify = [](QWidget* widget, const char* name) {
        widget->setObjectName(QString::fromLatin1(name));
    };
    identify(screenBox_, "screenSelector"); identify(captureButton_, "captureButton");
    identify(openButton_, "openImageButton"); identify(languageBox_, "languageSelector");
    identify(modeBox_, "modeSelector"); identify(label_, "measurementLabel");
    identify(optionsButton_, "modeOptionsButton"); identify(knownLength_, "knownLength");
    identify(unit_, "knownUnit"); identify(tolerance_, "colorTolerance");
    identify(zoom_, "magnifierZoom"); identify(resetButton_, "resetScaleButton");
    identify(measureButton_, "measureButton"); identify(resultLabel_, "latestResult");
    identify(sourceLabel_, "sourceStatus"); identify(hintLabel_, "modeHint");
    identify(status_, "statusMessage"); identify(history_, "historyTable");
    identify(showButton_, "showAnnotatedButton"); identify(saveButton_, "saveAnnotatedButton");
    identify(regionButton_, "screenshotRegionButton");
    identify(saveSourceButton_, "saveSourceButton"); identify(copyButton_, "copySelectedButton");
    identify(copyAllButton_, "copyAllButton"); identify(csvButton_, "exportCsvButton");
    identify(clearButton_, "clearHistoryButton"); identify(helpButton_, "helpButton");
    identify(shortcutsButton_, "shortcutsButton");
    identify(hideButton_, "hideToTrayButton"); identify(quitButton_, "quitButton");

    connect(regionButton_, &QPushButton::clicked, this, [this] { selectScreenshotRegion(); });
    canvas_->setOnScreenshot([this](const QImage& crop) {
        showControls();
        const QString path = QFileDialog::getSaveFileName(this,
            trText("Сохранить область", "Save region"), {}, QStringLiteral("PNG (*.png)"));
        if (path.isEmpty()) return;
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly) || !crop.save(&file, "PNG") || !file.commit()) {
            status_->setText(file.errorString());
            QMessageBox::warning(this, QStringLiteral("Desktop Ruler"),
                trText("Не удалось сохранить область: ", "Could not save region: ")+file.errorString());
        } else status_->setText(trText("Область сохранена", "Region saved"));
    });
    canvas_->setWindowTitle(QStringLiteral("Desktop Ruler · canvas"));
    canvas_->setOnComplete([this](auto m, auto points, auto mask) { onComplete(m, points, mask); });
    canvas_->setOnError([this](const QString& error) {
        const QString localized = displayError(error);
        status_->setText(localized);
        QMessageBox::warning(canvas_, QStringLiteral("Desktop Ruler"), localized);
    });
    canvas_->setOnCancel([this] { showControls();
        status_->setText(trText("Измерение отменено", "Measurement cancelled")); });
    connect(captureButton_, &QPushButton::clicked, this, [this] { capture(); });
    connect(openButton_, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, trText("Открыть изображение", "Open image"), {},
                                                          QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp *.webp)"));
        if (!path.isEmpty()) openImageFile(path);
    });
    connect(measureButton_, &QPushButton::clicked, this, [this] {
        if (image_.isNull()) capture(); else nextMeasurement();
    });
    connect(modeBox_, &QComboBox::currentIndexChanged, this, [this] {
        updateModeControls(); updateHints(); updateAvailability();
    });
    connect(optionsButton_, &QPushButton::clicked, this, [this] {
        optionsExpanded_ = !optionsExpanded_; updateModeControls();
    });
    connect(showButton_, &QPushButton::clicked, this, [this] { showImage(); });
    connect(saveButton_, &QPushButton::clicked, this, [this] { saveImage(); });
    connect(saveSourceButton_, &QPushButton::clicked, this, [this] { saveSourceDialog(); });
    connect(copyButton_, &QPushButton::clicked, this, [this] { copyResults(true); });
    connect(copyAllButton_, &QPushButton::clicked, this, [this] { copyResults(false); });
    connect(csvButton_, &QPushButton::clicked, this, [this] { exportDialog(); });
    connect(clearButton_, &QPushButton::clicked, this, [this] {
        if (entries_.isEmpty()) return;
        if (QMessageBox::question(this, trText("Очистить историю", "Clear history"),
                                  trText("Удалить все результаты этой сессии?", "Delete all results from this session?"))
            == QMessageBox::Yes) clearHistory();
    });
    connect(resetButton_, &QPushButton::clicked, this, [this] {
        scale_ = 1; scaleUnit_ = QStringLiteral("px");
        scaleLabel_->setText(QStringLiteral("1 px = 1 px"));
        canvas_->setScale(scale_, scaleUnit_);
        updateAvailability();
    });
    connect(helpButton_, &QPushButton::clicked, this, [this] { showHelp(); });
    connect(shortcutsButton_, &QPushButton::clicked, this, [this] { shortcuts_.showSettings(english_); });
    connect(hideButton_, &QPushButton::clicked, this, [this] {
        if (QSystemTrayIcon::isSystemTrayAvailable()) hide();
        else { hideButton_->hide(); status_->setText(trText("Системный трей недоступен", "System tray is unavailable")); }
    });
    connect(quitButton_, &QPushButton::clicked, this, [this] { quitApplication(); });
    connect(languageBox_, &QComboBox::currentIndexChanged, this, [this](int index) { setEnglish(index == 1); });
    connect(zoom_, &QSpinBox::valueChanged, canvas_, &Canvas::setMagnifierZoom);
    connect(tolerance_, &QSpinBox::valueChanged, canvas_, &Canvas::setTolerance);
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        tray_ = new QSystemTrayIcon(icon, this);
        auto* menu = new QMenu(this);
        trayShow_ = menu->addAction(QString(), this, [this] {
            if (canvas_->isVisible()) canvas_->cancel(); else showControls();
        });
        trayCapture_ = menu->addAction(QString(), this, [this] { capture(); });
        trayHide_ = menu->addAction(QString(), this, [this] { hide(); });
        menu->addSeparator();
        trayQuit_ = menu->addAction(QString(), this, [this] { quitApplication(); });
        tray_->setContextMenu(menu);
        tray_->show();
        connect(tray_, &QSystemTrayIcon::activated, this, [this](auto reason) {
            if (reason == QSystemTrayIcon::Trigger) {
                if (canvas_->isVisible()) canvas_->cancel(); else showControls();
            }
        });
    }
    hideButton_->setVisible(tray_ && tray_->isVisible());
    populateScreens(); setEnglish(false);
    status_->setText(trText("Готово к захвату экрана или открытию файла", "Ready to capture a screen or open an image"));
    scaleLabel_->setText(QStringLiteral("1 px = 1 px"));
}
RulerWindow::~RulerWindow() { delete canvas_; }

void RulerWindow::closeEvent(QCloseEvent* event) {
    if (!quitRequested_ && tray_ && tray_->isVisible() && QSystemTrayIcon::isSystemTrayAvailable()) {
        hide();
        event->ignore();
        return;
    }
    QMainWindow::closeEvent(event);
}

void RulerWindow::quitApplication() {
    quitRequested_ = true;
    canvas_->hide();
    if (tray_) tray_->hide();
    close();
    qApp->quit();
}

QString RulerWindow::trText(const char* ru, const char* en) const {
    return QString::fromUtf8(english_ ? en : ru);
}
void RulerWindow::setEnglish(bool english) {
    english_ = english;
    const int current = modeBox_->currentIndex();
    for (int i=0; i<modeBox_->count(); ++i)
        modeBox_->setItemText(i, modeName(static_cast<ruler::Mode>(i), english));
    modeBox_->setCurrentIndex(current);
    openButton_->setText(trText("Открыть файл", "Open image"));
    showButton_->setText(trText("Показать фрагмент", "Show marked crop"));
    saveButton_->setText(trText("Разметка PNG", "Marked crop PNG"));
    saveSourceButton_->setText(trText("Снимок PNG", "Full image PNG"));
    regionButton_->setText(trText("Область PNG", "Region PNG"));
    copyButton_->setText(trText("Копия строки", "Copy row"));
    copyAllButton_->setText(trText("Копия всех", "Copy all"));
    csvButton_->setText(trText("Экспорт CSV", "Export CSV"));
    clearButton_->setText(trText("Очистить историю", "Clear history"));
    helpButton_->setText(trText("Справка", "Help"));
    shortcutsButton_->setText(trText("Сочетания", "Shortcuts"));
    hideButton_->setText(trText("Свернуть в трей", "Hide to tray"));
    quitButton_->setText(trText("Выход", "Quit"));
    resetButton_->setText(trText("Сбросить в пиксели", "Reset to pixels"));
    modeLabel_->setText(trText("Режим", "Mode"));
    nameLabel_->setText(trText("Метка", "Label"));
    knownLabel_->setText(trText("Известная длина", "Known length"));
    toleranceLabel_->setText(trText("Допуск RGB", "RGB tolerance"));
    zoomLabel_->setText(trText("Увеличение лупы", "Magnifier zoom"));
    if (trayShow_) trayShow_->setText(trText("Показать линейку", "Show ruler"));
    if (trayCapture_) trayCapture_->setText(trText("Новый снимок", "New capture"));
    if (trayHide_) trayHide_->setText(trText("Скрыть", "Hide"));
    if (trayQuit_) trayQuit_->setText(trText("Выход", "Quit"));
    history_->setHorizontalHeaderLabels(english ?
        QStringList{"Label","Mode","Raw pixels","Calibrated","Unit"} :
        QStringList{"Метка","Режим","Исходные пиксели","После калибровки","Единица"});
    refreshHistory();
    canvas_->setLanguage(english_);
    updateModeControls();
    updateHints();
    updateAvailability();
    status_->setText(image_.isNull() ?
        trText("Готово к захвату экрана или открытию файла", "Ready to capture a screen or open an image") :
        trText("Источник готов. Выберите режим и нажмите «Измерить»", "Source ready. Choose a mode and Measure"));
}

QString RulerWindow::modeHint(ruler::Mode mode) const {
    switch (mode) {
    case ruler::Mode::Path:
        return trText("Нажмите две точки, затем «Готово» или Enter. Перетаскивайте для кривой; Backspace отменяет штрих.",
                      "Click two points, then Done or Enter. Drag for a curve; Backspace undoes a stroke.");
    case ruler::Mode::Area:
    case ruler::Mode::Perimeter:
        return trText("Обведите контур минимум тремя точками, затем «Готово» или Enter. Backspace отменяет штрих.",
                      "Trace at least three points, then Done or Enter. Backspace undoes a stroke.");
    case ruler::Mode::AutoArea:
        return trText("Нажмите пиксель области, проверьте подсветку и нажмите «Готово» или Enter.",
                      "Click a region pixel, check the highlight, then Done or Enter to finish.");
    case ruler::Mode::Angle:
        return trText("Вершина, затем две точки лучей. Третья точка завершит измерение.",
                      "Click the vertex, then two ray points. The third point completes the angle.");
    case ruler::Mode::Count:
        return trText("Нажмите каждый объект, затем «Готово» или Enter. Backspace убирает последнюю метку.",
                      "Click each object, then Done or Enter. Backspace removes the last mark.");
    case ruler::Mode::Calibrate:
        return trText("Укажите известную длину и единицу; отметьте две точки и нажмите «Готово» или Enter.",
                      "Enter a known length and unit; mark two points and finish with Done or Enter.");
    case ruler::Mode::Magnifier:
        return trText("Двигайте указатель для увеличения пикселей. Esc возвращает к настройкам.",
                      "Move the pointer to inspect pixels. Esc returns to the controls.");
    }
    return {};
}

void RulerWindow::updateModeControls() {
    const auto mode = static_cast<ruler::Mode>(modeBox_->currentData().toInt());
    const bool calibrate = mode == ruler::Mode::Calibrate;
    const bool magnifier = mode == ruler::Mode::Magnifier;
    knownWidget_->setVisible(calibrate);
    toleranceWidget_->setVisible(mode == ruler::Mode::AutoArea);
    zoomWidget_->setVisible(magnifier && optionsExpanded_);
    optionsButton_->setVisible(magnifier);
    optionsButton_->setText(optionsExpanded_ ? trText("Скрыть настройки", "Hide options") :
                                             trText("Настройки лупы", "Magnifier options"));
    nameLabel_->setVisible(!calibrate && !magnifier);
    label_->setVisible(!calibrate && !magnifier);
}

void RulerWindow::updateHints() {
    hintLabel_->setText(modeHint(static_cast<ruler::Mode>(modeBox_->currentData().toInt())));
}

void RulerWindow::updateAvailability() {
    const bool hasImage = !image_.isNull();
    const bool hasRows = !entries_.isEmpty();
    const int selected = history_->currentRow();
    const bool hasSelection = selected >= 0 && selected < entries_.size();
    captureButton_->setText(hasImage ? trText("Новый снимок", "New capture") :
                                       trText("Захват экрана", "Capture screen"));
    measureButton_->setText(hasImage ? trText("Измерить по снимку", "Measure this image") :
                                      trText("Снять и измерить", "Capture and measure"));
    captureButton_->setEnabled(screenBox_->count() > 0);
    measureButton_->setEnabled(hasImage || screenBox_->count() > 0);
    saveSourceButton_->setEnabled(hasImage);
    regionButton_->setEnabled(hasImage);
    for (auto* button : {showButton_, saveButton_, copyButton_}) button->setEnabled(hasSelection);
    for (auto* button : {copyAllButton_, csvButton_, clearButton_}) button->setEnabled(hasRows);
    resetButton_->setEnabled(scale_ != 1.0 || scaleUnit_ != QStringLiteral("px"));
    const QString shortSource = sourceName_.size() > 80 ? sourceName_.left(77)+QStringLiteral("…") : sourceName_;
    sourceLabel_->setText(hasImage ?
        trText("Источник: ", "Source: ")+shortSource :
        trText("Источник: не выбран", "Source: none"));
    sourceLabel_->setToolTip(hasImage ? sourceName_ : QString());
    resultLabel_->setText(hasRows ?
        trText("Последний результат: ", "Latest result: ")+resultText(entries_.last()) :
        trText("Последний результат: пока нет", "Latest result: none yet"));
    const QString chooseRow = trText("Сначала выберите результат в истории", "Select a result in history first");
    for (auto* button : {showButton_, saveButton_, copyButton_}) button->setToolTip(hasSelection ? QString() : chooseRow);
    const QString needHistory = trText("Сначала завершите измерение", "Complete a measurement first");
    for (auto* button : {copyAllButton_, csvButton_, clearButton_}) button->setToolTip(hasRows ? QString() : needHistory);
    saveSourceButton_->setToolTip(hasImage ?
        trText("Сохранить весь снимок без разметки", "Save the full image without marks") :
        trText("Сначала сделайте снимок или откройте файл", "Capture or open an image first"));
    const QString noScreen = trText("Нет доступного экрана", "No screen available");
    captureButton_->setToolTip(captureButton_->isEnabled() ? QString() : noScreen);
    measureButton_->setToolTip(measureButton_->isEnabled() ? modeHint(static_cast<ruler::Mode>(modeBox_->currentData().toInt())) : noScreen);
    for (auto* button : {captureButton_, openButton_, measureButton_, optionsButton_, resetButton_,
                         showButton_, saveButton_, saveSourceButton_, regionButton_, copyButton_, copyAllButton_,
                         csvButton_, clearButton_, helpButton_, shortcutsButton_, hideButton_, quitButton_})
        button->setAccessibleName(button->text());
    screenBox_->setAccessibleName(trText("Выбор экрана", "Screen selection"));
    languageBox_->setAccessibleName(trText("Язык", "Language"));
    modeBox_->setAccessibleName(trText("Режим измерения", "Measurement mode"));
    label_->setAccessibleName(nameLabel_->text());
    knownLength_->setAccessibleName(knownLabel_->text());
    unit_->setAccessibleName(trText("Единица измерения", "Unit"));
    tolerance_->setAccessibleName(toleranceLabel_->text());
    zoom_->setAccessibleName(zoomLabel_->text());
    history_->setAccessibleName(trText("История измерений", "Measurement history"));
    resultLabel_->setAccessibleName(resultLabel_->text());
    sourceLabel_->setAccessibleName(sourceLabel_->text());
    hintLabel_->setAccessibleName(hintLabel_->text());
}

void RulerWindow::showControls() {
    canvas_->hide();
    showNormal();
    raise(); activateWindow();
    updateAvailability();
}

bool RulerWindow::prepareCanvas() {
    activeMode_ = static_cast<ruler::Mode>(modeBox_->currentData().toInt());
    if (activeMode_ == ruler::Mode::Calibrate && unit_->text().trimmed().isEmpty()) {
        showControls();
        QMessageBox::warning(this, QStringLiteral("Desktop Ruler"),
            trText("Укажите единицу измерения перед калибровкой", "Enter a unit before calibration"));
        unit_->setFocus();
        return false;
    }
    canvas_->setMode(activeMode_);
    canvas_->setLanguage(english_);
    canvas_->setHudText(modeName(activeMode_, english_));
    canvas_->setScale(scale_, scaleUnit_);
    canvas_->setTolerance(tolerance_->value());
    canvas_->setMagnifierZoom(zoom_->value());
    return true;
}
void RulerWindow::populateScreens() {
    const QString current = screenBox_->currentData().toString();
    screenBox_->clear();
    for (QScreen* screen : QGuiApplication::screens()) {
        screenBox_->addItem(QStringLiteral("%1 · %2×%3 · %4×")
            .arg(screen->name()).arg(screen->size().width()).arg(screen->size().height())
            .arg(screen->devicePixelRatio(), 0, 'g', 4), screen->name());
    }
    const int selected = screenBox_->findData(current);
    if (selected >= 0) screenBox_->setCurrentIndex(selected);
}
void RulerWindow::capture() {
    if (capturePending_ || QApplication::activeModalWidget()) return;
    populateScreens();
    updateAvailability();
    const auto screens = QGuiApplication::screens();
    if (screenBox_->currentIndex() < 0 || screenBox_->currentIndex() >= screens.size()) {
        status_->setText(trText("Нет доступного экрана для захвата", "No screen is available to capture"));
        showControls();
        return;
    }
    QPointer<QScreen> screen = screens[screenBox_->currentIndex()];
    const QString output = screen->name();
    const QString scale = QString::number(screen->devicePixelRatio(), 'g', 8);
    capturePending_ = true;
    hide(); canvas_->hide();
    QTimer::singleShot(220, this, [this, screen, output, scale] {
        capturePending_ = false;
        if (!screen) {
            showControls(); status_->setText(trText("Экран отключён", "Screen disconnected")); return;
        }
        QProcess grim;
        grim.start(QStringLiteral("grim"), {QStringLiteral("-o"), output,
                    QStringLiteral("-s"), scale, QStringLiteral("-t"), QStringLiteral("png"),
                    QStringLiteral("-")});
        if (!grim.waitForStarted(2000) || !grim.waitForFinished(8000)) {
            grim.kill(); grim.waitForFinished();
            showControls();
            status_->setText(trText("Не удалось запустить или завершить grim", "Could not start or finish grim"));
            QMessageBox::warning(this, QStringLiteral("Desktop Ruler"),
                status_->text());
            return;
        }
        QImage capture;
        capture.loadFromData(grim.readAllStandardOutput(), "PNG");
        if (grim.exitStatus() != QProcess::NormalExit || grim.exitCode() != 0 || capture.isNull()) {
            const QString error = QString::fromLocal8Bit(grim.readAllStandardError()).left(1000);
            showControls();
            status_->setText(trText("Захват экрана не удался: ", "Capture failed: ")+error);
            QMessageBox::warning(this, QStringLiteral("Desktop Ruler"),
                status_->text());
            return;
        }
        image_ = capture;
        sourceName_ = QStringLiteral("%1 · %2×%3").arg(output)
            .arg(capture.width()).arg(capture.height());
        canvas_->setImage(image_);
        if (!prepareCanvas()) return;
        canvas_->createWinId();
        if (canvas_->windowHandle()) canvas_->windowHandle()->setScreen(screen);
        canvas_->setGeometry(screen->geometry());
        canvas_->showFullScreen(); canvas_->raise(); canvas_->activateWindow(); canvas_->setFocus();
        status_->setText(trText("Снимок готов", "Capture ready"));
        updateAvailability();
    });
}
void RulerWindow::openImage(const QImage& image) {
    if (image.isNull()) return;
    image_ = image;
    sourceName_ = QStringLiteral("%1 × %2").arg(image.width()).arg(image.height());
    canvas_->setImage(image_);
    if (!prepareCanvas()) return;
    hide(); canvas_->showFullScreen(); canvas_->raise(); canvas_->activateWindow(); canvas_->setFocus();
    status_->setText(trText("Изображение готово", "Image ready"));
    updateAvailability();
}
bool RulerWindow::openImageFile(const QString& path) {
    QImage image(path);
    if (image.isNull()) {
        status_->setText(trText("Невозможно открыть изображение", "Cannot open image"));
        QMessageBox::warning(this, QStringLiteral("Desktop Ruler"),
                             status_->text());
        return false;
    }
    openImage(image);
    sourceName_ = QFileInfo(path).fileName()+QStringLiteral(" · %1×%2")
        .arg(image.width()).arg(image.height());
    updateAvailability();
    return true;
}
void RulerWindow::selectMode(ruler::Mode mode) {
    if (canvas_->isVisible()) canvas_->cancel();
    modeBox_->setCurrentIndex(static_cast<int>(mode));
    updateModeControls(); updateHints();
}
void RulerWindow::setLabel(const QString& label) { label_->setText(label); }
void RulerWindow::nextMeasurement() {
    if (image_.isNull()) {
        status_->setText(trText("Сначала сделайте снимок или откройте файл", "Capture or open an image first"));
        return;
    }
    if (!prepareCanvas()) return;
    hide(); canvas_->showFullScreen(); canvas_->raise(); canvas_->activateWindow(); canvas_->setFocus();
}
void RulerWindow::setKnownLength(double length, const QString& unit) {
    knownLength_->setValue(length); unit_->setText(unit);
}
void RulerWindow::onComplete(ruler::Measurement measurement, QVector<QPointF> points, QImage mask) {
    const auto mode = activeMode_;
    if (mode == ruler::Mode::Calibrate) {
        const double next = ruler::unitsPerPixel(knownLength_->value(), measurement.length);
        if (next <= 0 || unit_->text().trimmed().isEmpty()) {
            QMessageBox::warning(canvas_, QStringLiteral("Desktop Ruler"),
                trText("Укажите положительную длину и единицу", "Enter a positive length and unit"));
            return;
        }
        scale_ = next; scaleUnit_ = unit_->text().trimmed();
        scaleLabel_->setText(QStringLiteral("1 px = %1 %2").arg(scale_, 0, 'g', 9).arg(scaleUnit_));
        canvas_->setScale(scale_, scaleUnit_);
        showControls();
        status_->setText(trText("Масштаб установлен", "Scale calibrated"));
        return;
    }
    Entry entry;
    entry.name = label_->text().trimmed();
    if (entry.name.isEmpty()) entry.name = QString::number(entries_.size()+1);
    entry.kind = mode;
    entry.raw = measurement; entry.scale = scale_; entry.unit = scaleUnit_;
    entry.screenshot = canvas_->annotatedImage(points, mode, mask);
    entries_.append(entry);
    refreshHistory();
    history_->selectRow(entries_.size()-1);
    status_->setText(resultText(entry));
    showControls();
}
QString RulerWindow::resultText(const Entry& entry) const {
    const auto& m = entry.raw;
    if (entry.kind == ruler::Mode::Angle) return QString::number(m.angle, 'f', 2)+QStringLiteral("°");
    if (entry.kind == ruler::Mode::Count) return QString::number(m.count);
    if (entry.kind == ruler::Mode::Area || entry.kind == ruler::Mode::AutoArea)
        return QStringLiteral("%1 px² · %2 %3² · %4 px / %5 %3")
            .arg(m.area,0,'f',2).arg(m.area*entry.scale*entry.scale,0,'f',2)
            .arg(entry.unit).arg(m.length,0,'f',2).arg(m.length*entry.scale,0,'f',2);
    return QStringLiteral("%1 px · %2 %3").arg(m.length,0,'f',2)
        .arg(m.length*entry.scale,0,'f',2).arg(entry.unit);
}
void RulerWindow::refreshHistory() {
    const int selected = history_->currentRow();
    history_->setRowCount(entries_.size());
    for (int i=0; i<entries_.size(); ++i) {
        const Entry& entry = entries_[i];
        const auto& m = entry.raw;
        QString raw, calibrated, unit = entry.unit;
        if (entry.kind == ruler::Mode::Angle) {
            raw = QString::number(m.angle,'f',2); calibrated = raw; unit = QStringLiteral("°");
        } else if (entry.kind == ruler::Mode::Count) {
            raw = QString::number(m.count); calibrated = raw; unit.clear();
        } else if (entry.kind == ruler::Mode::Area || entry.kind == ruler::Mode::AutoArea) {
            raw = QStringLiteral("%1 px²; %2 px").arg(m.area,0,'f',2).arg(m.length,0,'f',2);
            calibrated = QStringLiteral("%1 %2²; %3 %2")
                .arg(m.area*entry.scale*entry.scale,0,'f',2).arg(unit)
                .arg(m.length*entry.scale,0,'f',2);
        } else {
            raw = QString::number(m.length,'f',2);
            calibrated = QString::number(m.length*entry.scale,'f',2);
        }
        const QStringList cells{entry.name, modeName(entry.kind, english_), raw, calibrated, unit};
        for (int j=0; j<cells.size(); ++j) {
            auto* item = new QTableWidgetItem(cells[j]);
            if (j != 0) item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            history_->setItem(i,j,item);
        }
    }
    if (selected >= 0 && selected < entries_.size()) history_->selectRow(selected);
    updateAvailability();
}
int RulerWindow::historyCount() const { return entries_.size(); }
QString RulerWindow::tableText(bool selectedOnly, bool csv) const {
    QStringList lines;
    auto format = [csv](const QStringList& fields) {
        QStringList escaped;
        for (const auto& field : fields) escaped << (csv ? csvCell(field) : safeCell(field));
        return escaped.join(csv ? ',' : '\t');
    };
    lines << format({"Label","Mode","Length px","Area px2","Angle deg","Count",
                     "Scale units/px","Unit","Length calibrated","Area calibrated"});
    for (int i=0; i<entries_.size(); ++i) {
        if (selectedOnly && i != history_->currentRow()) continue;
        const Entry& e = entries_[i];
        const auto& m = e.raw;
        const bool dimension = e.kind == ruler::Mode::Path || e.kind == ruler::Mode::Area ||
                               e.kind == ruler::Mode::Perimeter || e.kind == ruler::Mode::AutoArea;
        const bool area = e.kind == ruler::Mode::Area || e.kind == ruler::Mode::AutoArea;
        lines << format({e.name, modeName(e.kind, english_),
                         dimension ? QString::number(m.length,'g',15) : QString(),
                         area ? QString::number(m.area,'g',15) : QString(),
                         e.kind == ruler::Mode::Angle ? QString::number(m.angle,'g',15) : QString(),
                         e.kind == ruler::Mode::Count ? QString::number(m.count) : QString(),
                         dimension ? QString::number(e.scale,'g',15) : QString(),
                         dimension ? e.unit : QString(),
                         dimension ? QString::number(m.length*e.scale,'g',15) : QString(),
                         area ? QString::number(m.area*e.scale*e.scale,'g',15) : QString()});
    }
    return lines.join('\n')+'\n';
}
void RulerWindow::copyResults(bool selectedOnly) {
    if (entries_.isEmpty() || (selectedOnly && history_->currentRow() < 0)) return;
    QGuiApplication::clipboard()->setText(tableText(selectedOnly, false));
    status_->setText(trText("Таблица скопирована", "Table copied"));
}
bool RulerWindow::exportCsv(const QString& path, bool selectedOnly) {
    if (entries_.isEmpty() || (selectedOnly && history_->currentRow() < 0)) return false;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) { status_->setText(file.errorString()); return false; }
    const QByteArray bytes = QByteArray("\xEF\xBB\xBF")+tableText(selectedOnly,true).toUtf8();
    if (file.write(bytes) != bytes.size() || !file.commit()) {
        status_->setText(file.errorString()); return false;
    }
    status_->setText(trText("CSV сохранён", "CSV saved")); return true;
}
void RulerWindow::exportDialog() {
    if (entries_.isEmpty()) return;
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("CSV"), {}, QStringLiteral("CSV (*.csv)"));
    if (!path.isEmpty() && !exportCsv(path)) QMessageBox::warning(this, QStringLiteral("Desktop Ruler"), status_->text());
}
void RulerWindow::showImage() {
    const int row = history_->currentRow(); if (row < 0 || row >= entries_.size()) return;
    auto* dialog = new QDialog(this); dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(entries_[row].name);
    auto* layout = new QVBoxLayout(dialog);
    auto* image = new QLabel;
    image->setPixmap(QPixmap::fromImage(entries_[row].screenshot).scaled(1000,700,Qt::KeepAspectRatio,
                                                                        Qt::SmoothTransformation));
    layout->addWidget(image); dialog->show();
}
void RulerWindow::saveImage() {
    const int row = history_->currentRow(); if (row < 0 || row >= entries_.size()) return;
    const QString path = QFileDialog::getSaveFileName(this, trText("Сохранить изображение", "Save image"), {},
                                                      QStringLiteral("PNG (*.png)"));
    if (path.isEmpty()) return;
    if (!saveSelectedImage(path))
        QMessageBox::warning(this, QStringLiteral("Desktop Ruler"), status_->text());
}
void RulerWindow::selectScreenshotRegion() {
    if (image_.isNull()) return;
    canvas_->setMode(ruler::Mode::Path);
    canvas_->setLanguage(english_);
    canvas_->setHudText(trText("Область снимка", "Image region"));
    canvas_->setScreenshotSelection(true);
    hide(); canvas_->showFullScreen(); canvas_->raise(); canvas_->activateWindow(); canvas_->setFocus();
}
void RulerWindow::saveSourceDialog() {
    if (image_.isNull()) return;
    const QString path = QFileDialog::getSaveFileName(this,
        trText("Сохранить весь снимок", "Save full image"), {}, QStringLiteral("PNG (*.png)"));
    if (!path.isEmpty() && !saveSourceImage(path))
        QMessageBox::warning(this, QStringLiteral("Desktop Ruler"), status_->text());
}
bool RulerWindow::saveSourceImage(const QString& path) {
    if (image_.isNull()) return false;
    QByteArray bytes; QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly);
    if (!image_.save(&buffer, "PNG")) {
        status_->setText(trText("Ошибка PNG", "PNG error")); return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        status_->setText(file.errorString()); return false;
    }
    status_->setText(trText("Полный снимок сохранён", "Full image saved")); return true;
}
bool RulerWindow::saveSelectedImage(const QString& path) {
    const int row = history_->currentRow(); if (row < 0 || row >= entries_.size()) return false;
    QByteArray bytes; QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly);
    if (!entries_[row].screenshot.save(&buffer,"PNG")) {
        status_->setText(trText("Ошибка PNG", "PNG error")); return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        status_->setText(file.errorString()); return false;
    }
    status_->setText(trText("Фрагмент с разметкой сохранён", "Marked crop saved")); return true;
}
void RulerWindow::clearHistory() {
    entries_.clear(); refreshHistory();
    status_->setText(trText("История очищена", "History cleared"));
}
QString RulerWindow::displayError(const QString& error) const {
    if (english_) return error;
    static const QHash<QString, QString> translations = {
        {"At least two points are required", "Нужны минимум две точки"},
        {"At least three points are required", "Нужны минимум три точки"},
        {"At least one point is required", "Нужна минимум одна точка"},
        {"Coordinates must be finite", "Координаты должны быть конечными числами"},
        {"Path length must be positive", "Длина пути должна быть больше нуля"},
        {"Path length is out of range", "Длина пути выходит за допустимый диапазон"},
        {"Perimeter is out of range", "Периметр выходит за допустимый диапазон"},
        {"Adjacent polygon edges must not overlap", "Соседние рёбра контура не должны накладываться"},
        {"Polygon edges must not cross or touch", "Контур не должен пересекаться или касаться себя"},
        {"Polygon edges must have positive finite length", "У каждого ребра контура должна быть положительная длина"},
        {"Polygon area must be positive and finite", "Площадь контура должна быть больше нуля"},
        {"Angle requires a vertex and two ray points", "Для угла нужны вершина и две точки лучей"},
        {"Angle rays must have positive length", "Луч угла должен иметь положительную длину"},
        {"Angle is out of range", "Угол выходит за допустимый диапазон"},
        {"Unsupported geometry mode", "Этот режим геометрии не поддерживается"},
        {"Image is empty", "Изображение пусто"},
        {"Tolerance must be between 0 and 255", "Допуск должен быть от 0 до 255"},
        {"Seed is outside the image", "Начальная точка вне изображения"},
        {"Could not allocate region mask", "Не удалось выделить память для области"},
        {"Could not convert image for region selection", "Не удалось подготовить изображение для области"}
    };
    return translations.value(error, QStringLiteral("Ошибка измерения: ")+error);
}
void RulerWindow::showHelp() {
    QMessageBox::information(this, trText("Справка", "Help"), english_ ?
        QStringLiteral("Select a screen and Capture, or Open image. The screen is frozen. Choose a mode, then Measure. "
        "With no image, Measure captures the selected screen first. Click at least two points for a distance; "
        "drag to trace freehand. Angle: vertex first, then two rays. Auto color: click a seed; "
        "the RGB tolerance is the maximum channel difference from its seed. The region uses 4-connected pixels; "
        "area counts occupied pixels and perimeter counts exposed pixel edges, including holes. "
        "Use Done, Enter or right click to finish; Cancel or Esc returns; Undo, Backspace or Ctrl+Z removes a point or stroke. "
        "Calibration measures a known line or path and sets units per image pixel. Area scales by its square. "
        "Measure again reuses the frozen image; New capture updates it. History lasts until Clear or quit. "
        "Full image PNG saves the whole source; Marked crop PNG saves the annotated result. Copy rows as TSV or export CSV. "
        "Magnifier works without measurement. Shortcuts edits local key bindings. Closing hides to tray when available; Quit exits.") :
        QStringLiteral("Выберите экран и сделайте снимок либо откройте файл. Изображение неподвижно. Выберите режим и нажмите «Измерить». "
        "Если снимка нет, «Измерить» сначала захватит выбранный экран. Для расстояния нажмите минимум две точки; "
        "перетаскивайте для свободной линии. Угол: сначала вершина, затем два луча. "
        "Область по цвету: нажмите на начальный пиксель. Допуск RGB — максимальное отклонение канала от него. "
        "Связность по 4 сторонам; площадь — число пикселей, периметр — открытые стороны пикселей, включая отверстия. "
        "«Готово», Enter или правая кнопка завершают; «Назад» или Esc возвращают; «Отменить», Backspace или Ctrl+Z убирают точку/штрих. "
        "Калибровка задаёт единицы на пиксель по известному отрезку или пути; площадь меняется по квадрату масштаба. "
        "Повторное измерение использует тот же снимок; «Новый снимок» обновляет его. История хранится до очистки или выхода. "
        "«Снимок PNG» сохраняет весь источник; «Разметка PNG» — фрагмент с отметками. Таблица копируется как TSV или сохраняется CSV. "
        "Лупа работает отдельно. «Сочетания» настраивает клавиши внутри приложения. "
        "Закрытие скрывает окно в трей, если он доступен; «Выход» завершает программу."));
}
