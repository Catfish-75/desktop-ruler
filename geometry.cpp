#include "geometry.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <QColor>

namespace ruler {
namespace {
bool finitePoint(QPointF point) {
    return std::isfinite(point.x()) && std::isfinite(point.y());
}

Measurement invalid(const QString& message) {
    Measurement result;
    result.error = message;
    return result;
}

double segmentLength(QPointF a, QPointF b) {
    return std::hypot(b.x() - a.x(), b.y() - a.y());
}

long double orientation(QPointF a, QPointF b, QPointF c) {
    const long double abx = static_cast<long double>(b.x()) - a.x();
    const long double aby = static_cast<long double>(b.y()) - a.y();
    const long double acx = static_cast<long double>(c.x()) - a.x();
    const long double acy = static_cast<long double>(c.y()) - a.y();
    return abx * acy - aby * acx;
}

bool onSegment(QPointF a, QPointF b, QPointF p) {
    return orientation(a, b, p) == 0 &&
        p.x() >= std::min(a.x(), b.x()) && p.x() <= std::max(a.x(), b.x()) &&
        p.y() >= std::min(a.y(), b.y()) && p.y() <= std::max(a.y(), b.y());
}

int sign(long double value) {
    return value < 0 ? -1 : (value > 0 ? 1 : 0);
}

bool segmentsIntersect(QPointF a, QPointF b, QPointF c, QPointF d) {
    if (std::max(std::min(a.x(), b.x()), std::min(c.x(), d.x())) >
            std::min(std::max(a.x(), b.x()), std::max(c.x(), d.x())) ||
        std::max(std::min(a.y(), b.y()), std::min(c.y(), d.y())) >
            std::min(std::max(a.y(), b.y()), std::max(c.y(), d.y())))
        return false;
    const long double abC = orientation(a, b, c);
    const long double abD = orientation(a, b, d);
    const long double cdA = orientation(c, d, a);
    const long double cdB = orientation(c, d, b);
    if (sign(abC) * sign(abD) < 0 && sign(cdA) * sign(cdB) < 0) return true;
    return (abC == 0 && onSegment(a, b, c)) || (abD == 0 && onSegment(a, b, d)) ||
        (cdA == 0 && onSegment(c, d, a)) || (cdB == 0 && onSegment(c, d, b));
}

Measurement polygonMeasurement(const QVector<QPointF>& points, bool includeArea) {
    qsizetype n = points.size();
    if (n > 1 && points.first() == points.last()) --n;
    if (n < 3) return invalid(QStringLiteral("At least three points are required"));
    for (qsizetype i = 0; i < n; ++i) {
        const QPointF point = points[i];
        if (!finitePoint(point)) return invalid(QStringLiteral("Coordinates must be finite"));
    }

    long double twiceArea = 0;
    double perimeter = 0;
    for (qsizetype i = 0; i < n; ++i) {
        const QPointF a = points[i];
        const QPointF b = points[(i + 1) % n];
        const double edge = segmentLength(a, b);
        if (!std::isfinite(edge) || edge <= 0)
            return invalid(QStringLiteral("Polygon edges must have positive finite length"));
        perimeter += edge;
        if (!std::isfinite(perimeter)) return invalid(QStringLiteral("Perimeter is out of range"));
        const long double ax = static_cast<long double>(a.x()) - points[0].x();
        const long double ay = static_cast<long double>(a.y()) - points[0].y();
        const long double bx = static_cast<long double>(b.x()) - points[0].x();
        const long double by = static_cast<long double>(b.y()) - points[0].y();
        twiceArea += ax * by - bx * ay;
    }

    for (qsizetype i = 0; i < n; ++i) {
        const QPointF previous = points[(i + n - 1) % n];
        const QPointF current = points[i];
        const QPointF next = points[(i + 1) % n];
        if (orientation(previous, current, next) == 0) {
            const long double backX = static_cast<long double>(previous.x()) - current.x();
            const long double backY = static_cast<long double>(previous.y()) - current.y();
            const long double forwardX = static_cast<long double>(next.x()) - current.x();
            const long double forwardY = static_cast<long double>(next.y()) - current.y();
            if (backX * forwardX + backY * forwardY > 0)
                return invalid(QStringLiteral("Adjacent polygon edges must not overlap"));
        }
    }

    for (qsizetype i = 0; i < n; ++i) {
        const qsizetype iNext = (i + 1) % n;
        for (qsizetype j = i + 1; j < n; ++j) {
            const qsizetype jNext = (j + 1) % n;
            if (j == iNext || (i == 0 && jNext == 0)) continue;
            if (segmentsIntersect(points[i], points[iNext], points[j], points[jNext]))
                return invalid(QStringLiteral("Polygon edges must not cross or touch"));
        }
    }

    const long double absoluteArea = std::abs(twiceArea) / 2;
    if (!std::isfinite(absoluteArea) || absoluteArea <= 0 ||
        absoluteArea > std::numeric_limits<double>::max())
        return invalid(QStringLiteral("Polygon area must be positive and finite"));

    Measurement result;
    result.valid = true;
    result.length = perimeter;
    result.area = includeArea ? static_cast<double>(absoluteArea) : 0;
    return result;
}
} // namespace

Measurement measure(Mode mode, const QVector<QPointF>& points) {
    if (mode == Mode::Area || mode == Mode::Perimeter)
        return polygonMeasurement(points, mode == Mode::Area);
    if (mode == Mode::Count) {
        if (points.isEmpty()) return invalid(QStringLiteral("At least one point is required"));
        for (const QPointF point : points)
            if (!finitePoint(point)) return invalid(QStringLiteral("Coordinates must be finite"));
        Measurement result;
        result.valid = true;
        result.count = points.size();
        return result;
    }
    if (mode == Mode::Angle) {
        if (points.size() != 3) return invalid(QStringLiteral("Angle requires a vertex and two ray points"));
        for (const QPointF point : points)
            if (!finitePoint(point)) return invalid(QStringLiteral("Coordinates must be finite"));
        const long double ax = static_cast<long double>(points[1].x()) - points[0].x();
        const long double ay = static_cast<long double>(points[1].y()) - points[0].y();
        const long double bx = static_cast<long double>(points[2].x()) - points[0].x();
        const long double by = static_cast<long double>(points[2].y()) - points[0].y();
        const long double aLength = std::hypot(ax, ay);
        const long double bLength = std::hypot(bx, by);
        if (aLength <= 0 || bLength <= 0)
            return invalid(QStringLiteral("Angle rays must have positive length"));
        const long double cross = ax * by - ay * bx;
        const long double dot = ax * bx + ay * by;
        const double degrees = static_cast<double>(std::atan2(std::abs(cross), dot) *
            (180.0L / std::acos(-1.0L)));
        if (!std::isfinite(degrees))
            return invalid(QStringLiteral("Angle is out of range"));
        Measurement result;
        result.valid = true;
        result.angle = degrees;
        return result;
    }
    if (mode != Mode::Path && mode != Mode::Calibrate)
        return invalid(QStringLiteral("Unsupported geometry mode"));
    if (points.size() < 2)
        return invalid(QStringLiteral("At least two points are required"));
    for (const QPointF point : points) {
        if (!finitePoint(point))
            return invalid(QStringLiteral("Coordinates must be finite"));
    }

    double length = 0;
    for (qsizetype i = 1; i < points.size(); ++i) {
        length += segmentLength(points[i - 1], points[i]);
        if (!std::isfinite(length))
            return invalid(QStringLiteral("Path length is out of range"));
    }
    if (length <= 0)
        return invalid(QStringLiteral("Path length must be positive"));

    Measurement result;
    result.valid = true;
    result.length = length;
    return result;
}

Region floodRegion(const QImage& source, QPoint seed, int tolerance) {
    Region region;
    if (source.isNull()) {
        region.measurement = invalid(QStringLiteral("Image is empty"));
        return region;
    }
    if (tolerance < 0 || tolerance > 255) {
        region.measurement = invalid(QStringLiteral("Tolerance must be between 0 and 255"));
        return region;
    }
    const int width = source.width();
    const int height = source.height();
    if (seed.x() < 0 || seed.y() < 0 || seed.x() >= width || seed.y() >= height) {
        region.measurement = invalid(QStringLiteral("Seed is outside the image"));
        return region;
    }
    const qint64 pixelCount = static_cast<qint64>(width) * height;
    const QImage image = source.convertToFormat(QImage::Format_ARGB32);
    if (image.isNull()) {
        region.measurement = invalid(QStringLiteral("Could not convert image for region selection"));
        return region;
    }
    const QRgb seedColor = reinterpret_cast<const QRgb*>(image.constScanLine(seed.y()))[seed.x()];
    region.mask = QImage(width, height, QImage::Format_Alpha8);
    if (region.mask.isNull()) {
        region.measurement = invalid(QStringLiteral("Could not allocate region mask"));
        return region;
    }
    region.mask.fill(0);
    QVector<qsizetype> pending;
    pending.reserve(static_cast<qsizetype>(std::min<qint64>(pixelCount, 65536)));
    const qsizetype seedIndex = static_cast<qsizetype>(seed.y()) * width + seed.x();
    pending.append(seedIndex);
    region.mask.scanLine(seed.y())[seed.x()] = 255;

    int minX = seed.x(), maxX = seed.x(), minY = seed.y(), maxY = seed.y();
    qint64 area = 0;
    qint64 perimeter = 0;
    const auto matches = [&](int x, int y) {
        const QRgb pixel = reinterpret_cast<const QRgb*>(image.constScanLine(y))[x];
        return std::max({std::abs(qRed(pixel) - qRed(seedColor)),
                         std::abs(qGreen(pixel) - qGreen(seedColor)),
                         std::abs(qBlue(pixel) - qBlue(seedColor))}) <= tolerance;
    };
    for (qsizetype head = 0; head < pending.size(); ++head) {
        const qsizetype index = pending[head];
        const int x = static_cast<int>(index % width);
        const int y = static_cast<int>(index / width);
        ++area;
        minX = std::min(minX, x); maxX = std::max(maxX, x);
        minY = std::min(minY, y); maxY = std::max(maxY, y);
        const int dx[] = {-1, 1, 0, 0};
        const int dy[] = {0, 0, -1, 1};
        for (int direction = 0; direction < 4; ++direction) {
            const int nx = x + dx[direction];
            const int ny = y + dy[direction];
            if (nx < 0 || ny < 0 || nx >= width || ny >= height) {
                ++perimeter;
                continue;
            }
            unsigned char* neighbor = region.mask.scanLine(ny) + nx;
            if (*neighbor != 0) continue;
            if (matches(nx, ny)) {
                *neighbor = 255;
                pending.append(static_cast<qsizetype>(ny) * width + nx);
            } else {
                ++perimeter;
            }
        }
    }
    region.bounds = QRect(QPoint(minX, minY), QPoint(maxX, maxY));
    region.measurement.valid = true;
    region.measurement.area = static_cast<double>(area);
    region.measurement.length = static_cast<double>(perimeter);
    return region;
}

QPointF imagePoint(QPointF viewportPoint, QSizeF viewportSize, QSize imageSize) {
    if (!finitePoint(viewportPoint) || !std::isfinite(viewportSize.width()) ||
        !std::isfinite(viewportSize.height()) || viewportSize.width() <= 0 ||
        viewportSize.height() <= 0 || imageSize.width() <= 0 || imageSize.height() <= 0)
        return {};
    return {viewportPoint.x() * imageSize.width() / viewportSize.width(),
            viewportPoint.y() * imageSize.height() / viewportSize.height()};
}

double unitsPerPixel(double knownLength, double pixelLength) {
    if (!std::isfinite(knownLength) || !std::isfinite(pixelLength) ||
        knownLength <= 0 || pixelLength <= 0)
        return 0;
    const double result = knownLength / pixelLength;
    return std::isfinite(result) && result > 0 ? result : 0;
}
} // namespace ruler
