#include "../geometry.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void near(double actual, double expected, const char* message) {
    check(std::isfinite(actual) && std::abs(actual - expected) < 1e-9, message);
}

void pathAndCalibration() {
    const auto triangle = ruler::measure(ruler::Mode::Path,
        {{0, 0}, {3, 0}, {3, 4}});
    check(triangle.valid, "3-4-5 path should be valid");
    near(triangle.length, 7, "path sums independent 3-4-5 segments");

    const auto oneSegment = ruler::measure(ruler::Mode::Path, {{0, 0}, {3, 4}});
    check(oneSegment.valid, "single segment should be valid");
    near(oneSegment.length, 5, "single segment length should be 5 pixels");

    const auto singleton = ruler::measure(ruler::Mode::Path, {{2, 2}});
    check(!singleton.valid, "path with fewer than two points must be rejected");
    const auto nonFinite = ruler::measure(ruler::Mode::Path,
        {{0, 0}, {std::numeric_limits<double>::infinity(), 1}});
    check(!nonFinite.valid, "non-finite path coordinate must be rejected");

    near(ruler::unitsPerPixel(10, 5), 2, "calibration returns units per pixel");
    near(ruler::unitsPerPixel(10, 0), 0, "zero pixel length rejects calibration");
    near(ruler::unitsPerPixel(-1, 2), 0, "negative known length rejects calibration");
    near(ruler::unitsPerPixel(1, std::numeric_limits<double>::infinity()), 0,
         "non-finite pixel length rejects calibration");
}

void closedGeometry() {
    const auto rectangle = ruler::measure(ruler::Mode::Area,
        {{0, 0}, {4, 0}, {4, 3}, {0, 3}});
    check(rectangle.valid, "rectangle should be valid");
    near(rectangle.length, 14, "rectangle perimeter should be 14");
    near(rectangle.area, 12, "rectangle area should be 12");
    const auto explicitlyClosed = ruler::measure(ruler::Mode::Area,
        {{0, 0}, {4, 0}, {4, 3}, {0, 3}, {0, 0}});
    check(explicitlyClosed.valid, "repeating the first point should close a polygon");
    near(explicitlyClosed.length, 14, "repeated closing point does not add perimeter");
    near(explicitlyClosed.area, 12, "repeated closing point preserves area");

    const auto reversed = ruler::measure(ruler::Mode::Area,
        {{0, 3}, {4, 3}, {4, 0}, {0, 0}});
    check(reversed.valid, "reversed rectangle should be valid");
    near(reversed.area, 12, "reversed polygon preserves positive area");
    near(reversed.length, 14, "reversed polygon preserves perimeter");

    const auto concave = ruler::measure(ruler::Mode::Area,
        {{0, 0}, {4, 0}, {4, 4}, {2, 2}, {0, 4}});
    check(concave.valid, "simple concave polygon should be valid");
    near(concave.area, 12, "concave polygon area should be 12");

    const auto perimeterOnly = ruler::measure(ruler::Mode::Perimeter,
        {{0, 0}, {4, 0}, {4, 3}, {0, 3}});
    check(perimeterOnly.valid, "perimeter-only polygon should be valid");
    near(perimeterOnly.length, 14, "perimeter-only mode returns closed perimeter");
    near(perimeterOnly.area, 0, "perimeter-only mode does not report area");

    check(!ruler::measure(ruler::Mode::Area,
        {{0, 0}, {2, 2}, {0, 2}, {2, 0}}).valid,
        "bow-tie self-crossing polygon must be rejected");
    check(!ruler::measure(ruler::Mode::Area,
        {{0, 0}, {4, 0}, {4, 4}, {2, 0}, {0, 4}}).valid,
        "polygon touching a nonadjacent edge must be rejected");
    check(!ruler::measure(ruler::Mode::Area,
        {{0, 0}, {2, 0}, {2, 0}, {0, 2}}).valid,
        "zero-length polygon edge must be rejected");
    check(!ruler::measure(ruler::Mode::Area,
        {{0, 0}, {4, 0}, {2, 0}, {2, 2}, {0, 2}}).valid,
        "adjacent polygon edges that backtrack over each other must be rejected");
    check(!ruler::measure(ruler::Mode::Area,
        {{0, 0}, {1, 0}, {2, 0}}).valid,
        "zero-area polygon must be rejected");
}

void angleAndCount() {
    const auto right = ruler::measure(ruler::Mode::Angle,
        {{1, 1}, {4, 1}, {1, 5}});
    check(right.valid, "right angle should be valid");
    near(right.angle, 90, "angle vertex is the first point");

    const auto obtuse = ruler::measure(ruler::Mode::Angle,
        {{0, 0}, {1, 0}, {-1, 1}});
    check(obtuse.valid, "obtuse angle should be valid");
    near(obtuse.angle, 135, "obtuse angle should be 135 degrees");
    const auto zero = ruler::measure(ruler::Mode::Angle,
        {{0, 0}, {1, 0}, {2, 0}});
    check(zero.valid, "same-direction rays define a valid zero angle");
    near(zero.angle, 0, "same-direction rays measure zero degrees");
    const auto straight = ruler::measure(ruler::Mode::Angle,
        {{0, 0}, {1, 0}, {-2, 0}});
    check(straight.valid, "opposite rays define a valid straight angle");
    near(straight.angle, 180, "opposite rays measure 180 degrees");
    check(!ruler::measure(ruler::Mode::Angle,
        {{0, 0}, {0, 0}, {1, 0}}).valid,
        "zero-length angle ray should be rejected");

    const auto count = ruler::measure(ruler::Mode::Count,
        {{0, 0}, {2, 3}, {-1, 4}});
    check(count.valid && count.count == 3, "count mode returns point count");
}

void viewportMapping() {
    const QPointF fractional = ruler::imagePoint({50, 25}, {100, 50}, {250, 125});
    near(fractional.x(), 125, "fractional display scale maps horizontal image point");
    near(fractional.y(), 62.5, "fractional display scale preserves subpixel y");
    const QPointF doubled = ruler::imagePoint({640, 360}, {960, 540}, {1920, 1080});
    near(doubled.x(), 1280, "2x image scale maps x");
    near(doubled.y(), 720, "2x image scale maps y");
}

QImage solidImage(int width, int height, QColor color) {
    QImage image(width, height, QImage::Format_ARGB32);
    image.fill(color);
    return image;
}

void floodRegions() {
    QImage ring = solidImage(5, 5, Qt::black);
    for (int y = 1; y <= 3; ++y)
        for (int x = 1; x <= 3; ++x)
            if (x != 2 || y != 2) ring.setPixelColor(x, y, Qt::white);
    const auto holed = ruler::floodRegion(ring, {1, 1});
    check(holed.measurement.valid, "ring region should be valid");
    near(holed.measurement.area, 8, "ring area counts occupied pixels");
    near(holed.measurement.length, 16, "ring perimeter includes hole edges");
    check(holed.bounds == QRect(1, 1, 3, 3), "ring bounds enclose selected pixels");
    check(holed.mask.size() == ring.size(), "region mask matches image size");
    check(qAlpha(holed.mask.pixel(1, 1)) != 0 && qAlpha(holed.mask.pixel(2, 2)) == 0,
          "mask marks ring and leaves hole transparent");

    QImage tolerant = solidImage(4, 1, QColor(101, 90, 100));
    tolerant.setPixelColor(0, 0, QColor(100, 100, 100));
    tolerant.setPixelColor(1, 0, QColor(110, 95, 100));
    tolerant.setPixelColor(2, 0, QColor(111, 100, 100));
    const auto withinTolerance = ruler::floodRegion(tolerant, {0, 0}, 10);
    near(withinTolerance.measurement.area, 2, "max-channel tolerance includes exact boundary");
    near(withinTolerance.measurement.length, 6, "two-pixel run has six exposed edges");

    QImage diagonal = solidImage(2, 2, Qt::black);
    diagonal.setPixelColor(0, 0, Qt::red);
    diagonal.setPixelColor(1, 1, Qt::red);
    const auto fourConnected = ruler::floodRegion(diagonal, {0, 0});
    near(fourConnected.measurement.area, 1, "diagonal same-color pixel is disconnected");
    near(fourConnected.measurement.length, 4, "single pixel perimeter is four");

    check(!ruler::floodRegion(diagonal, {-1, 0}).measurement.valid,
          "out-of-image seed must be rejected");
    check(!ruler::floodRegion(diagonal, {0, 0}, -1).measurement.valid,
          "negative tolerance must be rejected");
}
} // namespace

int main() {
    try {
        pathAndCalibration();
        closedGeometry();
        angleAndCount();
        viewportMapping();
        floodRegions();
        std::cout << "geometry tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << "geometry test failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
