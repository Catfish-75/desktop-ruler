#pragma once
#include "geometry.h"

#include <QWidget>
#include <functional>

class Canvas final : public QWidget {
public:
    using Complete = std::function<void(ruler::Measurement, QVector<QPointF>, QImage)>;
    explicit Canvas(QWidget* parent = nullptr);
    void setImage(const QImage& image);
    void setMode(ruler::Mode mode);
    void setTolerance(int tolerance);
    void setMagnifierZoom(int zoom);
    void setHudText(const QString& text) { hudText_ = text; update(); }
    void setScale(double scale, const QString& unit) { scale_ = scale; unit_ = unit; preview(); update(); }
    void setOnComplete(Complete callback) { onComplete_ = std::move(callback); }
    void setOnError(std::function<void(QString)> callback) { onError_ = std::move(callback); }
    void setOnPreview(std::function<void(QString)> callback) { onPreview_ = std::move(callback); }
    void setOnCancel(std::function<void()> callback) { onCancel_ = std::move(callback); }
    void finish();
    void cancel();
    void undo();
    QImage annotatedImage(const QVector<QPointF>& points, ruler::Mode mode, const QImage& mask = {}) const;
    QSize sizeHint() const override { return {960, 540}; }

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;

private:
    QRectF imageRect() const;
    QPointF pointAt(QPointF widgetPoint) const;
    void preview();
    void drawMarks(QPainter& painter, const QVector<QPointF>& points, ruler::Mode mode,
                   const QImage& mask) const;
    void drawMagnifier(QPainter& painter) const;
    QImage image_;
    ruler::Mode mode_ = ruler::Mode::Path;
    QVector<QPointF> points_;
    QVector<int> strokeStarts_;
    QImage mask_;
    QImage tintedMask_;
    QRect maskBounds_;
    ruler::Measurement autoResult_;
    int tolerance_ = 0;
    int zoom_ = 8;
    bool drawing_ = false;
    QPointF cursor_;
    bool hasCursor_ = false;
    QString hudText_, previewText_, unit_ = QStringLiteral("px");
    double scale_ = 1;
    Complete onComplete_;
    std::function<void(QString)> onError_, onPreview_;
    std::function<void()> onCancel_;
};
