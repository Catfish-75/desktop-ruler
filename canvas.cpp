#include "canvas.h"

#include <QKeyEvent>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>
#include <algorithm>
#include <cmath>

Canvas::Canvas(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::CrossCursor);
    setAutoFillBackground(false);

    controls_ = new QWidget(this);
    controls_->setObjectName(QStringLiteral("canvasControls"));
    controls_->setStyleSheet(QStringLiteral(
        "QWidget#canvasControls { background: rgba(9,20,24,225); border: 1px solid #647578; border-radius: 4px; }"
        "QToolButton { color: #fff4ce; background: #26383c; border: 1px solid #687d80;"
        " border-radius: 3px; padding: 4px 7px; font-weight: 600; }"
        "QToolButton:hover { background: #3a5257; }"
        "QToolButton:disabled { color: #91a0a2; background: #1d292c; }"));
    auto* layout = new QHBoxLayout(controls_);
    layout->setContentsMargins(5, 4, 5, 4);
    layout->setSpacing(4);
    auto makeButton = [this, layout](const QString& name) {
        auto* button = new QToolButton(controls_);
        button->setObjectName(name);
        button->setFocusPolicy(Qt::NoFocus);
        button->setCursor(Qt::PointingHandCursor);
        button->setMinimumHeight(28);
        layout->addWidget(button);
        return button;
    };
    doneButton_ = makeButton(QStringLiteral("canvasDoneButton"));
    undoButton_ = makeButton(QStringLiteral("canvasUndoButton"));
    cancelButton_ = makeButton(QStringLiteral("canvasCancelButton"));
    connect(doneButton_, &QToolButton::clicked, this, [this] { finish(); });
    connect(undoButton_, &QToolButton::clicked, this, [this] { undo(); });
    connect(cancelButton_, &QToolButton::clicked, this, [this] { cancel(); });
    setLanguage(false);
}

void Canvas::setImage(const QImage& image) {
    resetScreenshotSelection();
    controls_->show();
    image_ = image.convertToFormat(QImage::Format_ARGB32);
    points_.clear(); strokeStarts_.clear(); mask_ = {}; tintedMask_ = {}; maskBounds_ = {}; autoResult_ = {};
    cursor_ = {}; hasCursor_ = false; drawing_ = false;
    updateControls();
    preview();
    update();
}
void Canvas::setMode(ruler::Mode mode) {
    resetScreenshotSelection();
    screenshotSelection_ = false;
    controls_->show();
    doneButton_->setText(english_ ? QStringLiteral("Done") : QStringLiteral("Готово"));
    doneButton_->setAccessibleName(doneButton_->text());
    positionControls();
    mode_ = mode;
    points_.clear(); strokeStarts_.clear(); mask_ = {}; tintedMask_ = {}; maskBounds_ = {}; autoResult_ = {};
    cursor_ = {}; hasCursor_ = false; drawing_ = false;
    updateControls();
    update(); preview();
}
void Canvas::setTolerance(int tolerance) { tolerance_ = qBound(0, tolerance, 255); }
void Canvas::setMagnifierZoom(int zoom) { zoom_ = qBound(2, zoom, 24); update(); }
void Canvas::setLanguage(bool english) {
    english_ = english;
    doneButton_->setText(screenshotSelection_
        ? (english_ ? QStringLiteral("Save region") : QStringLiteral("Сохранить область"))
        : (english_ ? QStringLiteral("Done") : QStringLiteral("Готово")));
    doneButton_->setAccessibleName(doneButton_->text());
    undoButton_->setText(english_ ? QStringLiteral("Undo") : QStringLiteral("Отменить"));
    cancelButton_->setText(english_ ? QStringLiteral("Cancel") : QStringLiteral("Назад"));
    doneButton_->setAccessibleName(doneButton_->text());
    undoButton_->setAccessibleName(undoButton_->text());
    cancelButton_->setAccessibleName(cancelButton_->text());
    positionControls();
    updateControls();
    update();
}

void Canvas::setScreenshotSelection(bool enabled) {
    if (enabled == screenshotSelection_) return;
    resetScreenshotSelection();
    screenshotSelection_ = enabled;
    doneButton_->setText(screenshotSelection_
        ? (english_ ? QStringLiteral("Save region") : QStringLiteral("Сохранить область"))
        : (english_ ? QStringLiteral("Done") : QStringLiteral("Готово")));
    doneButton_->setAccessibleName(doneButton_->text());
    positionControls();
    updateControls();
    preview();
    update();
}

void Canvas::resetScreenshotSelection() {
    screenshotHasAnchor_ = false;
    screenshotHasRect_ = false;
    screenshotDragging_ = false;
    screenshotAnchor_ = {};
    screenshotCurrent_ = {};
}

QRectF Canvas::screenshotRect() const {
    return QRectF(screenshotAnchor_, screenshotCurrent_).normalized();
}

bool Canvas::event(QEvent* event) {
    if (event->type() == QEvent::KeyPress) {
        const auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Tab && key->modifiers() == Qt::NoModifier) {
            controls_->setVisible(!controls_->isVisible());
            event->accept();
            update();
            return true;
        }
    }
    return QWidget::event(event);
}

QString Canvas::usageText() const {
    if (screenshotSelection_) {
        if (english_)
            return QStringLiteral("Drag a rectangle or click two opposite corners, then Save region.");
        return QStringLiteral("Выделите область рамкой или двумя углами, затем нажмите «Сохранить область».");
    }
    if (english_) {
        switch (mode_) {
        case ruler::Mode::Path: return QStringLiteral("Click points or drag a curved path; Done finishes.");
        case ruler::Mode::Area: return QStringLiteral("Click boundary corners or drag; Done closes the outline.");
        case ruler::Mode::Perimeter: return QStringLiteral("Click boundary corners or drag; Done closes the outline.");
        case ruler::Mode::AutoArea: return QStringLiteral("Click inside a color region, then Done to save its area.");
        case ruler::Mode::Angle: return QStringLiteral("Click the vertex, then one point on each ray.");
        case ruler::Mode::Count: return QStringLiteral("Click each object to place a numbered mark; Done finishes.");
        case ruler::Mode::Calibrate: return QStringLiteral("Trace a known line or path; Done applies its length.");
        case ruler::Mode::Magnifier: return QStringLiteral("Move to inspect pixels; Done or Cancel returns to controls.");
        }
    }
    switch (mode_) {
    case ruler::Mode::Path: return QStringLiteral("Ставьте точки или ведите линию мышью; «Готово» завершает путь.");
    case ruler::Mode::Area: return QStringLiteral("Отмечайте углы контура или ведите мышью; «Готово» замыкает его.");
    case ruler::Mode::Perimeter: return QStringLiteral("Отмечайте углы контура или ведите мышью; «Готово» замыкает его.");
    case ruler::Mode::AutoArea: return QStringLiteral("Щёлкните внутри одноцветной области, затем нажмите «Готово».");
    case ruler::Mode::Angle: return QStringLiteral("Отметьте вершину, затем по одной точке на каждом луче.");
    case ruler::Mode::Count: return QStringLiteral("Щёлкайте по объектам для нумерации; «Готово» завершает подсчёт.");
    case ruler::Mode::Calibrate: return QStringLiteral("Отметьте известный отрезок или путь; «Готово» задаёт масштаб.");
    case ruler::Mode::Magnifier: return QStringLiteral("Перемещайте указатель для увеличения; «Готово» вернёт к настройкам.");
    }
    return {};
}

void Canvas::updateControls() {
    if (undoButton_) undoButton_->setEnabled(screenshotSelection_
        ? (screenshotHasAnchor_ || screenshotHasRect_) : !points_.isEmpty());
}

void Canvas::positionControls() {
    controls_->adjustSize();
    controls_->move(std::max(8, width()-controls_->width()-12), 10);
}

QRectF Canvas::imageRect() const {
    if (image_.isNull()) return {};
    const QSizeF size = image_.size().scaled(this->size(), Qt::KeepAspectRatio);
    return QRectF((width()-size.width())/2, (height()-size.height())/2,
                  size.width(), size.height());
}
QPointF Canvas::pointAt(QPointF widgetPoint) const {
    const QRectF r = imageRect();
    const QPointF local = widgetPoint-r.topLeft();
    return ruler::imagePoint(local, r.size(), image_.size());
}

void Canvas::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::RightButton) { finish(); return; }
    if (event->button() != Qt::LeftButton || image_.isNull() ||
        !imageRect().contains(event->position())) return;
    setFocus();
    const QPointF p = pointAt(event->position());
    cursor_ = p; hasCursor_ = true;
    if (screenshotSelection_) {
        if (screenshotHasRect_) resetScreenshotSelection();
        if (!screenshotHasAnchor_) {
            screenshotAnchor_ = p;
            screenshotCurrent_ = p;
            screenshotHasAnchor_ = true;
        } else {
            screenshotCurrent_ = p;
            screenshotHasRect_ = true;
        }
        updateControls(); update();
        return;
    }
    if (mode_ == ruler::Mode::Magnifier) { update(); return; }
    if (mode_ == ruler::Mode::AutoArea) {
        const QPoint seed(qBound(0, static_cast<int>(std::floor(p.x())), image_.width()-1),
                          qBound(0, static_cast<int>(std::floor(p.y())), image_.height()-1));
        auto region = ruler::floodRegion(image_, seed, tolerance_);
        if (!region.measurement.valid) {
            if (onError_) onError_(region.measurement.error);
            return;
        }
        mask_ = region.mask; maskBounds_ = region.bounds; autoResult_ = region.measurement;
        tintedMask_ = QImage(maskBounds_.size(), QImage::Format_ARGB32_Premultiplied);
        tintedMask_.fill(QColor(0, 235, 190, 90));
        QPainter tint(&tintedMask_);
        tint.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        tint.drawImage(0, 0, mask_.copy(maskBounds_));
        tint.end();
        points_ = {p};
        updateControls(); preview(); update();
        return;
    }
    if (mode_ == ruler::Mode::Count) {
        points_.append(p);
        updateControls(); preview(); update();
        return;
    }
    if (mode_ == ruler::Mode::Angle && points_.size() >= 3) points_.clear();
    points_.append(p);
    drawing_ = mode_ == ruler::Mode::Path || mode_ == ruler::Mode::Calibrate ||
               mode_ == ruler::Mode::Area || mode_ == ruler::Mode::Perimeter;
    if (drawing_) strokeStarts_.append(points_.size()-1);
    if (mode_ == ruler::Mode::Angle && points_.size() == 3) { finish(); return; }
    updateControls(); preview(); update();
}

void Canvas::mouseMoveEvent(QMouseEvent* event) {
    if (image_.isNull()) return;
    const QRectF r = imageRect();
    QPointF w = event->position();
    w.setX(qBound(r.left(), w.x(), r.right()));
    w.setY(qBound(r.top(), w.y(), r.bottom()));
    cursor_ = pointAt(w); hasCursor_ = true;
    if (screenshotSelection_) {
        if (screenshotHasAnchor_ && (event->buttons() & Qt::LeftButton)) {
            screenshotCurrent_ = cursor_;
            if (std::hypot(screenshotCurrent_.x()-screenshotAnchor_.x(),
                           screenshotCurrent_.y()-screenshotAnchor_.y()) > 0.1)
                screenshotDragging_ = true;
        }
        update();
        return;
    }
    if (drawing_ && (event->buttons() & Qt::LeftButton) && !points_.isEmpty() &&
        std::hypot(cursor_.x()-points_.last().x(), cursor_.y()-points_.last().y()) >= 1.0) {
        points_.append(cursor_);
    }
    preview(); update();
}

void Canvas::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && screenshotSelection_) {
        if (screenshotHasAnchor_ && screenshotDragging_) {
            const QRectF r = imageRect();
            QPointF w = event->position();
            w.setX(qBound(r.left(), w.x(), r.right()));
            w.setY(qBound(r.top(), w.y(), r.bottom()));
            screenshotCurrent_ = pointAt(w);
            screenshotHasRect_ = true;
        }
        screenshotDragging_ = false;
        updateControls(); update();
        return;
    }
    if (event->button() == Qt::LeftButton && drawing_ && !points_.isEmpty()) {
        const QRectF r = imageRect();
        QPointF w = event->position();
        w.setX(qBound(r.left(), w.x(), r.right()));
        w.setY(qBound(r.top(), w.y(), r.bottom()));
        const QPointF end = pointAt(w);
        if (std::hypot(end.x()-points_.last().x(), end.y()-points_.last().y()) >= 1.0)
            points_.append(end);
        preview(); update();
    }
    if (event->button() == Qt::LeftButton) drawing_ = false;
}

void Canvas::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) finish();
    else if (event->key() == Qt::Key_Escape) cancel();
    else if (event->key() == Qt::Key_Backspace ||
             (event->key() == Qt::Key_Z && event->modifiers() & Qt::ControlModifier)) undo();
    else QWidget::keyPressEvent(event);
}

void Canvas::finish() {
    if (screenshotSelection_) {
        if (image_.isNull() || !screenshotHasRect_) {
            if (onError_) onError_(english_
                ? QStringLiteral("Select a screen region first")
                : QStringLiteral("Сначала выделите область экрана"));
            return;
        }
        const QRectF selection = screenshotRect();
        const int left = qBound(0, qFloor(selection.left()), image_.width());
        const int top = qBound(0, qFloor(selection.top()), image_.height());
        const int right = qBound(0, qCeil(selection.right()), image_.width());
        const int bottom = qBound(0, qCeil(selection.bottom()), image_.height());
        if (right <= left || bottom <= top) {
            if (onError_) onError_(english_
                ? QStringLiteral("The selected region must have a non-zero size")
                : QStringLiteral("Размер области должен быть больше нуля"));
            return;
        }
        const QImage crop = image_.copy(left, top, right-left, bottom-top);
        resetScreenshotSelection();
        screenshotSelection_ = false;
        doneButton_->setText(english_ ? QStringLiteral("Done") : QStringLiteral("Готово"));
        doneButton_->setAccessibleName(doneButton_->text());
        positionControls();
        updateControls(); update();
        if (onScreenshot_) onScreenshot_(crop);
        return;
    }
    if (mode_ == ruler::Mode::Magnifier) { cancel(); return; }
    if (mode_ == ruler::Mode::AutoArea && !autoResult_.valid) {
        if (onError_) onError_(QStringLiteral("At least one point is required"));
        return;
    }
    const auto value = mode_ == ruler::Mode::AutoArea ? autoResult_ : ruler::measure(mode_, points_);
    if (!value.valid) {
        if (onError_) onError_(value.error);
        return;
    }
    if (onComplete_) onComplete_(value, points_, mask_);
    points_.clear(); strokeStarts_.clear(); mask_ = {}; tintedMask_ = {}; maskBounds_ = {}; autoResult_ = {};
    cursor_ = {}; hasCursor_ = false; drawing_ = false;
    updateControls(); preview(); update();
}
void Canvas::cancel() {
    screenshotSelection_ = false;
    resetScreenshotSelection();
    doneButton_->setText(english_ ? QStringLiteral("Done") : QStringLiteral("Готово"));
    doneButton_->setAccessibleName(doneButton_->text());
    positionControls();
    points_.clear(); strokeStarts_.clear(); mask_ = {}; tintedMask_ = {}; maskBounds_ = {}; autoResult_ = {};
    cursor_ = {}; hasCursor_ = false; drawing_ = false;
    updateControls();
    preview(); update();
    if (onCancel_) onCancel_();
}
void Canvas::undo() {
    if (screenshotSelection_) {
        if (!screenshotHasAnchor_ && !screenshotHasRect_) return;
        resetScreenshotSelection();
        updateControls(); update();
        return;
    }
    if (points_.isEmpty()) return;
    if (mode_ == ruler::Mode::AutoArea) {
        points_.clear(); mask_ = {}; tintedMask_ = {}; maskBounds_ = {}; autoResult_ = {};
        updateControls(); preview(); update(); return;
    }
    if ((mode_ == ruler::Mode::Path || mode_ == ruler::Mode::Calibrate ||
         mode_ == ruler::Mode::Area || mode_ == ruler::Mode::Perimeter) &&
        !strokeStarts_.isEmpty()) {
        const int start = strokeStarts_.takeLast();
        points_.resize(start);
    } else points_.removeLast();
    updateControls(); preview(); update();
}
void Canvas::preview() {
    previewText_.clear();
    if (mode_ == ruler::Mode::Count) {
        previewText_ = QString::number(points_.size());
    } else if (mode_ == ruler::Mode::AutoArea) {
        if (autoResult_.valid)
            previewText_ = QStringLiteral("%1 px² / %2 %3² · %4 px")
                .arg(autoResult_.area,0,'f',0)
                .arg(autoResult_.area*scale_*scale_,0,'f',2).arg(unit_)
                .arg(autoResult_.length,0,'f',0);
    } else if (mode_ == ruler::Mode::Magnifier) {
        previewText_.clear();
    } else if (mode_ == ruler::Mode::Angle) {
        QVector<QPointF> rays = points_;
        if (rays.size() == 2 && hasCursor_) rays.append(cursor_);
        if (rays.size() == 3) {
            const auto value = ruler::measure(mode_, rays);
            if (value.valid) previewText_ = QString::number(value.angle, 'f', 2)+QStringLiteral("°");
        }
    } else {
        QVector<QPointF> previewPoints = points_;
        if (!drawing_ && hasCursor_ && !points_.isEmpty() &&
            std::hypot(cursor_.x()-points_.last().x(), cursor_.y()-points_.last().y()) > 0.1)
            previewPoints.append(cursor_);
        if (previewPoints.size() >= 2) {
            double length = 0, twiceArea = 0;
            for (int i=1; i<previewPoints.size(); ++i)
                length += std::hypot(previewPoints[i].x()-previewPoints[i-1].x(),
                                     previewPoints[i].y()-previewPoints[i-1].y());
            if (mode_ == ruler::Mode::Area || mode_ == ruler::Mode::Perimeter) {
                length += std::hypot(previewPoints.first().x()-previewPoints.last().x(),
                                     previewPoints.first().y()-previewPoints.last().y());
                for (int i=0; i<previewPoints.size(); ++i) {
                    const auto a = previewPoints[i];
                    const auto b = previewPoints[(i+1)%previewPoints.size()];
                    twiceArea += a.x()*b.y()-b.x()*a.y();
                }
            }
            if (mode_ == ruler::Mode::Area && previewPoints.size() >= 3) {
                const double area = std::abs(twiceArea)/2;
                previewText_ = QStringLiteral("%1 px² / %2 %3² · %4 px")
                    .arg(area,0,'f',2).arg(area*scale_*scale_,0,'f',2).arg(unit_)
                    .arg(length,0,'f',2);
            } else {
                previewText_ = QStringLiteral("%1 px / %2 %3")
                    .arg(length,0,'f',2).arg(length*scale_,0,'f',2).arg(unit_);
            }
        }
    }
    if (onPreview_) onPreview_(previewText_);
    update();
}

void Canvas::drawMarks(QPainter& p, const QVector<QPointF>& points, ruler::Mode mode,
                       const QImage& mask) const {
    if (!mask.isNull()) {
        p.drawImage(maskBounds_.topLeft(), tintedMask_);
    }
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(10, 20, 23, 210), 5));
    QPainterPath path;
    if (!points.isEmpty()) {
        if (mode == ruler::Mode::Angle && points.size() >= 2) {
            path.moveTo(points[1]); path.lineTo(points[0]);
            if (points.size() >= 3) path.lineTo(points[2]);
        } else {
            path.moveTo(points[0]);
            for (int i=1; i<points.size(); ++i) path.lineTo(points[i]);
            if ((mode == ruler::Mode::Area || mode == ruler::Mode::Perimeter) && points.size() >= 3)
                path.closeSubpath();
        }
    }
    if (mode != ruler::Mode::Count && mode != ruler::Mode::AutoArea) {
        p.drawPath(path);
        p.setPen(QPen(QColor(255, 207, 74), 2.5)); p.drawPath(path);
    }
    for (int i=0; i<points.size(); ++i) {
        if (mode != ruler::Mode::Count && i != 0 && i != points.size()-1 &&
            mode != ruler::Mode::Angle &&
            (i >= strokeStarts_.size() || !strokeStarts_.contains(i))) continue;
        p.setPen(QPen(QColor(8, 24, 27), 2)); p.setBrush(QColor(255, 207, 74));
        p.drawEllipse(points[i], 5, 5);
        if (mode == ruler::Mode::Count) {
            p.setPen(Qt::white);
            p.setFont(QFont(QStringLiteral("Sans"), 11, QFont::Bold));
            p.drawText(QRectF(points[i].x()+7, points[i].y()-17, 36, 28), QString::number(i+1));
        }
    }
}

void Canvas::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), QColor(20, 25, 29));
    const QRectF r = imageRect();
    if (image_.isNull()) return;
    p.drawImage(r, image_);
    p.save();
    p.translate(r.topLeft());
    p.scale(r.width()/image_.width(), r.height()/image_.height());
    drawMarks(p, points_, mode_, mask_);
    p.restore();
    if (screenshotSelection_ && screenshotHasAnchor_) {
        if (screenshotHasRect_ || screenshotDragging_) {
            const QRectF selected = screenshotRect();
            const QRectF widgetRect(r.left()+selected.left()*r.width()/image_.width(),
                                    r.top()+selected.top()*r.height()/image_.height(),
                                    selected.width()*r.width()/image_.width(),
                                    selected.height()*r.height()/image_.height());
            p.fillRect(widgetRect, QColor(28, 177, 190, 45));
            p.setPen(QPen(QColor(95, 243, 229), 2, Qt::DashLine));
            p.drawRect(widgetRect);
            const int cropWidth = qMax(0, qCeil(selected.right())-qFloor(selected.left()));
            const int cropHeight = qMax(0, qCeil(selected.bottom())-qFloor(selected.top()));
            const QString dimensions = QStringLiteral("%1 × %2 px").arg(cropWidth).arg(cropHeight);
            QFont labelFont = font(); labelFont.setBold(true);
            p.setFont(labelFont);
            const QSize labelSize = p.fontMetrics().size(Qt::TextSingleLine, dimensions)+QSize(12, 6);
            QRect label(widgetRect.left(), widgetRect.top()-labelSize.height()-4,
                        labelSize.width(), labelSize.height());
            label.moveLeft(qBound(4, label.left(), qMax(4, width()-label.width()-4)));
            if (label.top() < 4) label.moveTop(qMin(height()-label.height()-4,
                                                    qMax(4, qCeil(widgetRect.bottom()+4))));
            p.fillRect(label, QColor(9, 20, 24, 230));
            p.setPen(QColor(205, 255, 250));
            p.drawText(label, Qt::AlignCenter, dimensions);
        } else {
            const QPointF anchor(r.left()+screenshotAnchor_.x()*r.width()/image_.width(),
                                 r.top()+screenshotAnchor_.y()*r.height()/image_.height());
            p.setPen(QPen(QColor(95, 243, 229), 2));
            p.setBrush(QColor(9, 20, 24, 180));
            p.drawEllipse(anchor, 5, 5);
        }
    }
    if (hasCursor_ && !screenshotSelection_) drawMagnifier(p);
    const QString guidance = usageText();
    QString hud = hudText_;
    if (!guidance.isEmpty()) hud += (hud.isEmpty() ? QString() : QStringLiteral("\n"))+guidance;
    hud += (hud.isEmpty() ? QString() : QStringLiteral("\n")) +
        (english_ ? QStringLiteral("Tab: show/hide controls")
                  : QStringLiteral("Tab: показать/скрыть панель"));
    if (!previewText_.isEmpty()) hud += QStringLiteral("  ·  ")+previewText_;
    if (!hud.isEmpty()) {
        QFont f = font(); f.setPointSize(11); f.setBold(true); p.setFont(f);
        const QRect bounds = p.fontMetrics().boundingRect(QRect(16, 0, width()-32, 90),
            Qt::TextWordWrap, hud).adjusted(-11,-7,11,7);
        QRect panel = bounds.translated(0, 17);
        p.fillRect(panel, QColor(9, 20, 24, 220));
        p.setPen(QColor(255, 236, 178));
        p.drawText(panel.adjusted(11,7,-11,-7), Qt::TextWordWrap, hud);
    }
}

void Canvas::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    positionControls();
}

void Canvas::drawMagnifier(QPainter& p) const {
    const int pixel = zoom_;
    constexpr int margin = 16;
    constexpr int frame = 3;
    const int available = std::min(width(), height())-2*(margin+frame);
    int maxSourceSide = available/pixel;
    if ((maxSourceSide % 2) == 0) --maxSourceSide;
    const int sourceSide = std::min(21, maxSourceSide);
    if (sourceSide < 1) return;
    const int half = sourceSide/2;
    const QSize boxSize(sourceSide*pixel, sourceSide*pixel);
    const QRectF r = imageRect();
    const QPointF cursorWidget(r.left()+cursor_.x()*r.width()/image_.width(),
                               r.top()+cursor_.y()*r.height()/image_.height());
    const bool atRight = cursorWidget.x() > width()/2;
    const bool atTop = cursorWidget.y() < height()/2;
    const int left = margin;
    const int right = width()-boxSize.width()-margin;
    const int top = margin;
    const int bottom = height()-boxSize.height()-margin;
    QVector<QPoint> candidates = {
        QPoint(atRight ? left : right, atTop ? bottom : top),
        QPoint(atRight ? left : right, atTop ? top : bottom),
        QPoint(atRight ? right : left, atTop ? bottom : top),
        QPoint(atRight ? right : left, atTop ? top : bottom)
    };
    QRect box;
    for (const QPoint& candidate : candidates) {
        const QRect proposed(candidate, boxSize);
        const QRect framed = proposed.adjusted(-frame, -frame, frame, frame);
        if (proposed.left() >= margin && proposed.top() >= margin &&
            proposed.right() < width()-margin && proposed.bottom() < height()-margin &&
            (!controls_ || !controls_->isVisible() ||
             !framed.intersects(controls_->geometry().adjusted(-4,-4,4,4)))) {
            box = proposed;
            break;
        }
    }
    if (box.isNull()) return;
    const QPoint center(qBound(0, static_cast<int>(std::floor(cursor_.x())), image_.width()-1),
                        qBound(0, static_cast<int>(std::floor(cursor_.y())), image_.height()-1));
    p.fillRect(box.adjusted(-frame,-frame,frame,frame), QColor(10,14,18,230));
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    p.drawImage(box, image_, QRect(center.x()-half, center.y()-half, sourceSide, sourceSide));
    p.setPen(QPen(QColor(255,207,74), 1));
    p.drawLine(box.center().x(), box.top(), box.center().x(), box.bottom());
    p.drawLine(box.left(), box.center().y(), box.right(), box.center().y());
    p.setPen(Qt::white);
    p.drawText(box.adjusted(4,4,-4,-4), Qt::AlignBottom|Qt::AlignLeft,
               QStringLiteral("%1, %2 · %3×").arg(center.x()).arg(center.y()).arg(zoom_));
}

QImage Canvas::annotatedImage(const QVector<QPointF>& points, ruler::Mode mode,
                              const QImage& mask) const {
    QImage result = image_.copy();
    if (result.isNull()) return result;
    QPainter p(&result);
    drawMarks(p, points, mode, mask);
    p.end();
    QRect bounds = mode == ruler::Mode::AutoArea ? maskBounds_ : QRect();
    for (const QPointF point : points) {
        const QRect marker(qFloor(point.x())-7, qFloor(point.y())-7, 15, 15);
        bounds = bounds.isNull() ? marker : bounds.united(marker);
    }
    if (bounds.isNull()) return result;
    bounds.adjust(-64,-64,64,64);
    return result.copy(bounds.intersected(result.rect()));
}
