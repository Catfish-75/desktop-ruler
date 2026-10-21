#include "canvas.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <cmath>

Canvas::Canvas(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAutoFillBackground(false);
}

void Canvas::setImage(const QImage& image) {
    image_ = image.convertToFormat(QImage::Format_ARGB32);
    points_.clear(); strokeStarts_.clear(); mask_ = {}; tintedMask_ = {}; maskBounds_ = {}; autoResult_ = {};
    update();
}
void Canvas::setMode(ruler::Mode mode) {
    mode_ = mode;
    points_.clear(); strokeStarts_.clear(); mask_ = {}; tintedMask_ = {}; maskBounds_ = {}; autoResult_ = {};
    drawing_ = false;
    update(); preview();
}
void Canvas::setTolerance(int tolerance) { tolerance_ = qBound(0, tolerance, 255); }
void Canvas::setMagnifierZoom(int zoom) { zoom_ = qBound(2, zoom, 24); update(); }

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
        preview(); update();
        return;
    }
    if (mode_ == ruler::Mode::Count) {
        points_.append(p);
        preview(); update();
        return;
    }
    if (mode_ == ruler::Mode::Angle && points_.size() >= 3) points_.clear();
    points_.append(p);
    drawing_ = mode_ == ruler::Mode::Path || mode_ == ruler::Mode::Calibrate ||
               mode_ == ruler::Mode::Area || mode_ == ruler::Mode::Perimeter;
    if (drawing_) strokeStarts_.append(points_.size()-1);
    if (mode_ == ruler::Mode::Angle && points_.size() == 3) { finish(); return; }
    preview(); update();
}

void Canvas::mouseMoveEvent(QMouseEvent* event) {
    if (image_.isNull()) return;
    const QRectF r = imageRect();
    QPointF w = event->position();
    w.setX(qBound(r.left(), w.x(), r.right()));
    w.setY(qBound(r.top(), w.y(), r.bottom()));
    cursor_ = pointAt(w); hasCursor_ = true;
    if (drawing_ && (event->buttons() & Qt::LeftButton) && !points_.isEmpty() &&
        std::hypot(cursor_.x()-points_.last().x(), cursor_.y()-points_.last().y()) >= 1.0) {
        points_.append(cursor_);
    }
    preview(); update();
}

void Canvas::mouseReleaseEvent(QMouseEvent* event) {
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
    if (mode_ == ruler::Mode::Magnifier || (mode_ == ruler::Mode::AutoArea && !autoResult_.valid)) return;
    const auto value = mode_ == ruler::Mode::AutoArea ? autoResult_ : ruler::measure(mode_, points_);
    if (!value.valid) {
        if (onError_) onError_(value.error);
        return;
    }
    if (onComplete_) onComplete_(value, points_, mask_);
    points_.clear(); strokeStarts_.clear(); mask_ = {}; tintedMask_ = {}; maskBounds_ = {}; autoResult_ = {};
    preview(); update();
}
void Canvas::cancel() {
    points_.clear(); strokeStarts_.clear(); mask_ = {}; tintedMask_ = {}; maskBounds_ = {}; autoResult_ = {};
    drawing_ = false;
    preview(); update();
    if (onCancel_) onCancel_();
}
void Canvas::undo() {
    if (points_.isEmpty()) return;
    if (mode_ == ruler::Mode::AutoArea) {
        points_.clear(); mask_ = {}; tintedMask_ = {}; maskBounds_ = {}; autoResult_ = {};
        preview(); update(); return;
    }
    if ((mode_ == ruler::Mode::Path || mode_ == ruler::Mode::Calibrate ||
         mode_ == ruler::Mode::Area || mode_ == ruler::Mode::Perimeter) &&
        !strokeStarts_.isEmpty()) {
        const int start = strokeStarts_.takeLast();
        points_.resize(start);
    } else points_.removeLast();
    preview(); update();
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
    if (hasCursor_) drawMagnifier(p);
    const QString hud = hudText_ + (previewText_.isEmpty() ? QString() : QStringLiteral("  ·  ")+previewText_);
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

void Canvas::drawMagnifier(QPainter& p) const {
    const int sourceSide = 21;
    const int half = sourceSide/2;
    const int pixel = zoom_;
    const QSize boxSize(sourceSide*pixel, sourceSide*pixel);
    const QRectF r = imageRect();
    const QPointF cursorWidget(r.left()+cursor_.x()*r.width()/image_.width(),
                               r.top()+cursor_.y()*r.height()/image_.height());
    const bool atRight = cursorWidget.x() > width()/2;
    const bool atTop = cursorWidget.y() < height()/2;
    QRect box(QPoint(atRight ? 16 : width()-boxSize.width()-16,
                     atTop ? height()-boxSize.height()-16 : 16), boxSize);
    if (box.left() < 0 || box.bottom() > height()) return;
    const QPoint center(qBound(0, static_cast<int>(std::floor(cursor_.x())), image_.width()-1),
                        qBound(0, static_cast<int>(std::floor(cursor_.y())), image_.height()-1));
    p.fillRect(box.adjusted(-3,-3,3,3), QColor(10,14,18,230));
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
