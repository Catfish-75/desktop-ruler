#pragma once
#include <QImage>
#include <QPointF>
#include <QRect>
#include <QString>
#include <QVector>

namespace ruler {
enum class Mode { Path, Area, Perimeter, AutoArea, Angle, Count, Calibrate, Magnifier };
struct Measurement {
    bool valid = false;
    QString error;
    double length = 0; // image pixels; closed perimeter for Area/Perimeter
    double area = 0;   // image pixels squared
    double angle = 0;  // degrees, vertex is points[0]
    int count = 0;
};
struct Region {
    Measurement measurement;
    QImage mask; // image-size alpha mask, selected pixels nonzero alpha
    QRect bounds;
};
Measurement measure(Mode mode, const QVector<QPointF>& points);
Region floodRegion(const QImage& image, QPoint seed, int tolerance = 0);
QPointF imagePoint(QPointF viewportPoint, QSizeF viewportSize, QSize imageSize);
double unitsPerPixel(double knownLength, double pixelLength); // 0 for invalid
} // namespace ruler
