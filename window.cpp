#include "window.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGuiApplication>
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

RulerWindow::RulerWindow(QWidget* parent) : QMainWindow(parent), canvas_(new Canvas) {
    setWindowTitle(QStringLiteral("Desktop Ruler"));
    const QIcon icon = QIcon::fromTheme(QStringLiteral("desktop-ruler"),
                                      QIcon::fromTheme(QStringLiteral("accessories-calculator")));
    setWindowIcon(icon);
    resize(920, 690);
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* central = new QWidget(scroll);
    central->setMinimumSize(860, 620);
    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(10);
    scroll->setWidget(central);
    setCentralWidget(scroll);

    auto* title = new QLabel(QStringLiteral("Desktop Ruler"));
    QFont font = title->font(); font.setPointSize(18); font.setBold(true); title->setFont(font);
    root->addWidget(title);
    auto* top = new QHBoxLayout;
    screenBox_ = new QComboBox; screenBox_->setMinimumWidth(140);
    captureButton_ = new QPushButton; openButton_ = new QPushButton;
    languageBox_ = new QComboBox;
    languageBox_->addItems({QStringLiteral("Русский"), QStringLiteral("English")});
    top->addWidget(screenBox_, 2); top->addWidget(captureButton_);
    top->addWidget(openButton_); top->addWidget(languageBox_);
    root->addLayout(top);
    auto* form = new QFormLayout;
    modeBox_ = new QComboBox;
    for (int i=0; i<=static_cast<int>(ruler::Mode::Magnifier); ++i)
        modeBox_->addItem(modeName(static_cast<ruler::Mode>(i), false), i);
    label_ = new QLineEdit; label_->setMaxLength(200);
    knownLength_ = new QDoubleSpinBox; knownLength_->setRange(0.000001, 1e12);
    knownLength_->setDecimals(6); knownLength_->setValue(1);
    unit_ = new QLineEdit(QStringLiteral("mm")); unit_->setMaxLength(32);
    tolerance_ = new QSpinBox; tolerance_->setRange(0,255);
    zoom_ = new QSpinBox; zoom_->setRange(2,24); zoom_->setValue(8);
    auto* scaleRow = new QHBoxLayout;
    scaleRow->setContentsMargins(0,0,0,0);
    scaleRow->addWidget(knownLength_); scaleRow->addWidget(unit_);
    auto* scaleWidget = new QWidget; scaleWidget->setLayout(scaleRow);
    scaleWidget->setMinimumHeight(30);
    modeLabel_ = new QLabel; nameLabel_ = new QLabel; knownLabel_ = new QLabel;
    toleranceLabel_ = new QLabel; zoomLabel_ = new QLabel;
    form->addRow(modeLabel_, modeBox_);
    form->addRow(nameLabel_, label_);
    form->addRow(knownLabel_, scaleWidget);
    form->addRow(toleranceLabel_, tolerance_);
    form->addRow(zoomLabel_, zoom_);
    root->addLayout(form);
    scaleLabel_ = new QLabel;
    resetButton_ = new QPushButton;
    auto* scaleLine = new QHBoxLayout;
    scaleLine->addWidget(scaleLabel_, 1); scaleLine->addWidget(resetButton_);
    root->addLayout(scaleLine);

    auto* actions = new QHBoxLayout;
    startButton_ = new QPushButton; nextButton_ = new QPushButton; freshButton_ = new QPushButton;
    actions->addWidget(startButton_); actions->addWidget(nextButton_); actions->addWidget(freshButton_);
    root->addLayout(actions);
    preview_ = new QLabel; status_ = new QLabel;
    root->addWidget(preview_); root->addWidget(status_);
    history_ = new QTableWidget(0, 5);
    history_->setSelectionBehavior(QAbstractItemView::SelectRows);
    history_->setSelectionMode(QAbstractItemView::SingleSelection);
    history_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    history_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    history_->setMinimumHeight(115);
    connect(history_, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
        if (item && item->column() == 0 && item->row() >= 0 && item->row() < entries_.size())
            entries_[item->row()].name = item->text().trimmed();
    });
    root->addWidget(history_, 1);
    auto* exports = new QHBoxLayout;
    showButton_ = new QPushButton; saveButton_ = new QPushButton;
    copyButton_ = new QPushButton; copyAllButton_ = new QPushButton;
    csvButton_ = new QPushButton; clearButton_ = new QPushButton; helpButton_ = new QPushButton;
    for (auto* b : {showButton_, saveButton_, copyButton_, copyAllButton_, csvButton_, clearButton_, helpButton_})
        exports->addWidget(b);
    root->addLayout(exports);

    canvas_->setWindowTitle(QStringLiteral("Desktop Ruler · canvas"));
    canvas_->setOnComplete([this](auto m, auto points, auto mask) { onComplete(m, points, mask); });
    canvas_->setOnError([this](const QString& error) {
        const QString localized = displayError(error);
        status_->setText(localized);
        QMessageBox::warning(canvas_, QStringLiteral("Desktop Ruler"), localized);
    });
    canvas_->setOnPreview([this](const QString& value) { preview_->setText(value); });
    canvas_->setOnCancel([this] { canvas_->hide(); show(); raise(); activateWindow(); });
    connect(captureButton_, &QPushButton::clicked, this, [this] { capture(); });
    connect(freshButton_, &QPushButton::clicked, this, [this] { capture(); });
    connect(openButton_, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, trText("Открыть изображение", "Open image"), {},
                                                          QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp *.webp)"));
        if (!path.isEmpty()) openImageFile(path);
    });
    connect(startButton_, &QPushButton::clicked, this, [this] { nextMeasurement(); });
    connect(nextButton_, &QPushButton::clicked, this, [this] { nextMeasurement(); });
    connect(showButton_, &QPushButton::clicked, this, [this] { showImage(); });
    connect(saveButton_, &QPushButton::clicked, this, [this] { saveImage(); });
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
    });
    connect(helpButton_, &QPushButton::clicked, this, [this] { showHelp(); });
    connect(languageBox_, &QComboBox::currentIndexChanged, this, [this](int index) { setEnglish(index == 1); });
    connect(zoom_, &QSpinBox::valueChanged, canvas_, &Canvas::setMagnifierZoom);
    connect(tolerance_, &QSpinBox::valueChanged, canvas_, &Canvas::setTolerance);
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        tray_ = new QSystemTrayIcon(icon, this);
        auto* menu = new QMenu(this);
        menu->addAction(QStringLiteral("Desktop Ruler"), this, [this] { show(); raise(); activateWindow(); });
        menu->addAction(QStringLiteral("Capture / Захват"), this, [this] { capture(); });
        auto* modes = menu->addMenu(QStringLiteral("Mode / Режим"));
        for (int i=0; i<=static_cast<int>(ruler::Mode::Magnifier); ++i) {
            const auto mode = static_cast<ruler::Mode>(i);
            modes->addAction(modeName(mode, false), this, [this,mode] {
                selectMode(mode); show(); raise(); activateWindow();
            });
        }
        menu->addAction(QStringLiteral("History / История"), this, [this] { show(); raise(); activateWindow(); });
        menu->addAction(QStringLiteral("Quit / Выход"), qApp, &QApplication::quit);
        tray_->setContextMenu(menu);
        tray_->show();
        connect(tray_, &QSystemTrayIcon::activated, this, [this](auto reason) {
            if (reason == QSystemTrayIcon::Trigger) { show(); raise(); activateWindow(); }
        });
    }
    populateScreens(); setEnglish(false);
    status_->setText(trText("Выберите экран и захватите изображение", "Choose a screen and capture"));
    scaleLabel_->setText(QStringLiteral("1 px = 1 px"));
}
RulerWindow::~RulerWindow() { delete canvas_; }

QString RulerWindow::trText(const char* ru, const char* en) const {
    return QString::fromUtf8(english_ ? en : ru);
}
void RulerWindow::setEnglish(bool english) {
    english_ = english;
    const int current = modeBox_->currentIndex();
    for (int i=0; i<modeBox_->count(); ++i)
        modeBox_->setItemText(i, modeName(static_cast<ruler::Mode>(i), english));
    modeBox_->setCurrentIndex(current);
    captureButton_->setText(trText("Захват экрана", "Capture screen"));
    openButton_->setText(trText("Открыть файл", "Open image"));
    startButton_->setText(trText("Начать", "Start"));
    nextButton_->setText(trText("Следующее", "Next measurement"));
    freshButton_->setText(trText("Новый снимок", "Fresh capture"));
    showButton_->setText(trText("Показать", "Show image"));
    saveButton_->setText(trText("Сохранить PNG", "Save PNG"));
    copyButton_->setText(trText("Копировать", "Copy selected"));
    copyAllButton_->setText(trText("Копировать всё", "Copy all"));
    csvButton_->setText(trText("CSV", "CSV"));
    clearButton_->setText(trText("Очистить", "Clear"));
    helpButton_->setText(trText("Справка", "Help"));
    resetButton_->setText(trText("Сбросить в пиксели", "Reset to pixels"));
    modeLabel_->setText(trText("Режим", "Mode"));
    nameLabel_->setText(trText("Метка", "Label"));
    knownLabel_->setText(trText("Известная длина и единица", "Known length and unit"));
    toleranceLabel_->setText(trText("Допуск RGB", "RGB tolerance"));
    zoomLabel_->setText(trText("Увеличение лупы", "Magnifier zoom"));
    history_->setHorizontalHeaderLabels(english ?
        QStringList{"Label","Mode","Raw pixels","Calibrated","Unit"} :
        QStringList{"Метка","Режим","Исходные пиксели","После калибровки","Единица"});
    refreshHistory();
}
void RulerWindow::populateScreens() {
    const int current = screenBox_->currentIndex();
    screenBox_->clear();
    for (QScreen* screen : QGuiApplication::screens()) {
        screenBox_->addItem(QStringLiteral("%1 · %2×%3 · %4×")
            .arg(screen->name()).arg(screen->size().width()).arg(screen->size().height())
            .arg(screen->devicePixelRatio(), 0, 'g', 4));
    }
    if (current >= 0 && current < screenBox_->count()) screenBox_->setCurrentIndex(current);
}
void RulerWindow::capture() {
    populateScreens();
    const auto screens = QGuiApplication::screens();
    if (screenBox_->currentIndex() < 0 || screenBox_->currentIndex() >= screens.size()) {
        status_->setText(trText("Нет доступного экрана для захвата", "No screen is available to capture"));
        show(); raise(); activateWindow();
        return;
    }
    QPointer<QScreen> screen = screens[screenBox_->currentIndex()];
    const QString output = screen->name();
    const QString scale = QString::number(screen->devicePixelRatio(), 'g', 8);
    hide(); canvas_->hide();
    QTimer::singleShot(220, this, [this, screen, output, scale] {
        if (!screen) {
            show(); status_->setText(trText("Экран отключён", "Screen disconnected")); return;
        }
        QProcess grim;
        grim.start(QStringLiteral("grim"), {QStringLiteral("-o"), output,
                    QStringLiteral("-s"), scale, QStringLiteral("-t"), QStringLiteral("png"),
                    QStringLiteral("-")});
        if (!grim.waitForStarted(2000) || !grim.waitForFinished(8000)) {
            grim.kill(); grim.waitForFinished();
            show();
            QMessageBox::warning(this, QStringLiteral("Desktop Ruler"),
                trText("Не удалось запустить или завершить grim", "Could not start or finish grim"));
            return;
        }
        QImage capture;
        capture.loadFromData(grim.readAllStandardOutput(), "PNG");
        if (grim.exitStatus() != QProcess::NormalExit || grim.exitCode() != 0 || capture.isNull()) {
            const QString error = QString::fromLocal8Bit(grim.readAllStandardError()).left(1000);
            show();
            QMessageBox::warning(this, QStringLiteral("Desktop Ruler"),
                trText("Захват экрана не удался: ", "Capture failed: ")+error);
            return;
        }
        image_ = capture;
        canvas_->setImage(image_);
        canvas_->setMode(static_cast<ruler::Mode>(modeBox_->currentData().toInt()));
        canvas_->setHudText(modeName(static_cast<ruler::Mode>(modeBox_->currentData().toInt()), english_)
            +trText(" · Enter/ПКМ: готово · Esc: назад · Backspace: отменить точку",
                    " · Enter/right click: finish · Esc: back · Backspace: undo"));
        canvas_->setScale(scale_, scaleUnit_);
        canvas_->setTolerance(tolerance_->value());
        canvas_->setMagnifierZoom(zoom_->value());
        canvas_->createWinId();
        if (canvas_->windowHandle()) canvas_->windowHandle()->setScreen(screen);
        canvas_->setGeometry(screen->geometry());
        canvas_->showFullScreen(); canvas_->raise(); canvas_->activateWindow(); canvas_->setFocus();
        status_->setText(trText("Снимок готов", "Capture ready"));
    });
}
void RulerWindow::openImage(const QImage& image) {
    if (image.isNull()) return;
    image_ = image;
    canvas_->setImage(image_);
    canvas_->setMode(static_cast<ruler::Mode>(modeBox_->currentData().toInt()));
    canvas_->setHudText(modeName(static_cast<ruler::Mode>(modeBox_->currentData().toInt()), english_)
        +trText(" · Enter/ПКМ: готово · Esc: назад · Backspace: отменить точку",
                " · Enter/right click: finish · Esc: back · Backspace: undo"));
    canvas_->setScale(scale_, scaleUnit_);
    canvas_->setTolerance(tolerance_->value()); canvas_->setMagnifierZoom(zoom_->value());
    hide(); canvas_->showFullScreen(); canvas_->raise(); canvas_->activateWindow(); canvas_->setFocus();
    status_->setText(trText("Изображение готово", "Image ready"));
}
bool RulerWindow::openImageFile(const QString& path) {
    QImage image(path);
    if (image.isNull()) {
        QMessageBox::warning(this, QStringLiteral("Desktop Ruler"),
                             trText("Невозможно открыть изображение", "Cannot open image"));
        return false;
    }
    openImage(image); return true;
}
void RulerWindow::selectMode(ruler::Mode mode) {
    modeBox_->setCurrentIndex(static_cast<int>(mode));
    canvas_->setMode(mode);
}
void RulerWindow::setLabel(const QString& label) { label_->setText(label); }
void RulerWindow::nextMeasurement() {
    if (image_.isNull()) {
        status_->setText(trText("Сначала сделайте снимок или откройте файл", "Capture or open an image first"));
        return;
    }
    canvas_->setMode(static_cast<ruler::Mode>(modeBox_->currentData().toInt()));
    canvas_->setHudText(modeName(static_cast<ruler::Mode>(modeBox_->currentData().toInt()), english_)
        +trText(" · Enter/ПКМ: готово · Esc: назад · Backspace: отменить точку",
                " · Enter/right click: finish · Esc: back · Backspace: undo"));
    canvas_->setScale(scale_, scaleUnit_);
    canvas_->setTolerance(tolerance_->value()); canvas_->setMagnifierZoom(zoom_->value());
    hide(); canvas_->showFullScreen(); canvas_->raise(); canvas_->activateWindow(); canvas_->setFocus();
}
void RulerWindow::setKnownLength(double length, const QString& unit) {
    knownLength_->setValue(length); unit_->setText(unit);
}
void RulerWindow::onComplete(ruler::Measurement measurement, QVector<QPointF> points, QImage mask) {
    const auto mode = static_cast<ruler::Mode>(modeBox_->currentData().toInt());
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
        canvas_->hide(); show(); raise(); activateWindow();
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
    canvas_->hide(); show(); raise(); activateWindow();
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
    status_->setText(trText("PNG сохранён", "PNG saved")); return true;
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
        QStringLiteral("Select a screen and Capture, or Open image. The screen is frozen. Choose a mode, then Start. "
        "Left click points; drag to trace freehand. Angle: vertex first, then two rays. Auto color: click a seed; "
        "the RGB tolerance is the maximum channel difference from its seed. The region uses 4-connected pixels; "
        "area counts occupied pixels and perimeter counts exposed pixel edges, including holes. "
        "Enter or right click finishes; Esc cancels and returns; Backspace/Ctrl+Z undoes. "
        "Calibration measures a known line or path and sets units per image pixel. Area scales by its square. "
        "Next measurement reuses the frozen image; Fresh capture updates it. History lasts until Clear or quit. "
        "Show image/Save PNG display marks. Copy rows as TSV or export CSV. Magnifier works without measurement.") :
        QStringLiteral("Выберите экран и сделайте снимок или откройте файл. Изображение неподвижно. Выберите режим и нажмите «Начать». "
        "Левой кнопкой ставьте точки; перетаскивайте для свободной линии. Угол: сначала вершина, затем два луча. "
        "Область по цвету: нажмите на начальный пиксель. Допуск RGB — максимальное отклонение канала от него. "
        "Связность по 4 сторонам; площадь — число пикселей, периметр — открытые стороны пикселей, включая отверстия. "
        "Enter или правая кнопка завершает; Esc отменяет и возвращает; Backspace/Ctrl+Z убирает точку/штрих. "
        "Калибровка задаёт единицы на пиксель по известному отрезку или пути; площадь меняется по квадрату масштаба. "
        "«Следующее» использует тот же снимок; «Новый снимок» обновляет его. История хранится до очистки или выхода. "
        "Изображение с отметками можно показать или сохранить в PNG. Таблица копируется как TSV или сохраняется CSV. Лупа работает отдельно."));
}
