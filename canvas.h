#pragma once
#include "geometry.h"

#include <QWidget>
#include <functional>

class QToolButton;
class QWidget;
class QResizeEvent;
class QEvent;

class Canvas final : public QWidget {
public:
    using Complete = std::function<void(ruler::Measurement, QVector<QPointF>, QImage)>;
    explicit Canvas(QWidget* parent = nullptr);
    void setImage(const QImage& image);
    void setMode(ruler::Mode mode);
    void setTolerance(int tolerance);
    void setMagnifierZoom(int zoom);
    void setLanguage(bool english);
    void setScreenshotSelection(bool enabled);
    void setHudText(const QString& text) { hudText_ = text; update(); }
    void setScale(double scale, const QString& unit) { scale_ = scale; unit_ = unit; preview(); update(); }
    void setOnComplete(Complete callback) { onComplete_ = std::move(callback); }
    void setOnError(std::function<void(QString)> callback) { onError_ = std::move(callback); }
    void setOnPreview(std::function<void(QString)> callback) { onPreview_ = std::move(callback); }
    void setOnCancel(std::function<void()> callback) { onCancel_ = std::move(callback); }
    void setOnScreenshot(std::function<void(QImage)> callback) { onScreenshot_ = std::move(callback); }
    void finish();
    void cancel();
    void undo();
    QImage annotatedImage(const QVector<QPointF>& points, ruler::Mode mode, const QImage& mask = {}) const;
    QSize sizeHint() const override { return {960, 540}; }

protected:
    bool event(QEvent*) override;
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;

private:
    QRectF imageRect() const;
    QPointF pointAt(QPointF widgetPoint) const;
    void preview();
    QString usageText() const;
    void updateControls();
    void positionControls();
    void resetScreenshotSelection();
    QRectF screenshotRect() const;
    void drawMarks(QPainter& painter, const QVector<QPointF>& points, ruler::Mode mode,
                   const QImage& mask) const;
    void drawMagnifier(QPainter& painter) const;
    QImage image_;
    QWidget* controls_ = nullptr;
    QToolButton *doneButton_ = nullptr, *undoButton_ = nullptr, *cancelButton_ = nullptr;
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
    bool english_ = false;
    bool screenshotSelection_ = false;
    bool screenshotHasAnchor_ = false;
    bool screenshotHasRect_ = false;
    bool screenshotDragging_ = false;
    QPointF screenshotAnchor_;
    QPointF screenshotCurrent_;
    Complete onComplete_;
    std::function<void(QString)> onError_, onPreview_;
    std::function<void()> onCancel_;
    std::function<void(QImage)> onScreenshot_;
};
