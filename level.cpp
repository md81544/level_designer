#include "level.h"
#include "levelformat.h"
#include "log.h"
#include "utils.h"

#include <QColor>
#include <QPainter>
#include <QPen>
#include <QPolygonF>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <limits>
#include <map>
#include <numbers>
#include <stdexcept>
#include <string>
#include <tuple>

namespace mgo {

namespace {

// A pen one (logical) pixel wide, whatever the zoom level
QPen hairline(QColor colour)
{
    QPen pen(colour, 1.0);
    pen.setCosmetic(true);
    return pen;
}

const QColor BOUNDARY_COLOUR { 128, 128, 0 };

// The ship's outline and colour, as in amaze (see ShipModel's constructor),
// pointing up with its centre at the origin
const QPolygonF SHIP_OUTLINE({ { 0, -20 },
                               { 6, -11 },
                               { 6, 1 },
                               { 12, 7 },
                               { 12, 19 },
                               { 6, 13 },
                               { 2, 13 },
                               { 6, 16 },
                               { -6, 16 },
                               { -2, 13 },
                               { -6, 13 },
                               { -12, 19 },
                               { -12, 7 },
                               { -6, 1 },
                               { -6, -11 } });
const QColor SHIP_COLOUR { 192, 192, 255 };
const QColor SELECTED_COLOUR { Qt::white };

// Sizes of items, for hit testing and selection (workspace units)
constexpr double SHIP_RADIUS = 20.0;
constexpr double FUEL_RADIUS = 10.0;
constexpr double EXIT_HALF_WIDTH = 32.0;
constexpr double EXIT_HALF_HEIGHT = 14.0;
// How near (in pixels) the mouse must be to a vertex to pick it
constexpr double VERTEX_HIT_PIXELS = 6.0;
constexpr double GRID_SPACING = 50.0;
// The speed given to a new path (distance per tick)
constexpr double DEFAULT_PATH_SPEED = 0.5;
// Brighter than in amaze (see LEVEL_FORMAT.md), to show up against our brighter grid
const QColor DECORATION_COLOUR { 0, 170, 0 };
const QColor SPECIAL_OBJECT_GHOST_COLOUR { 255, 172, 163, 80 };

// Offsets are kept to whole units, like positions
QPointF roundedOffset(QPointF offset)
{
    return QPointF(std::round(offset.x()), std::round(offset.y()));
}

// The point on the line nearest p
QPointF nearestPointOnLine(QPointF p, const Line& l)
{
    const QPointF a(l.x0, l.y0);
    const QPointF b(l.x1, l.y1);
    const QPointF ab = b - a;
    const double lengthSquared = QPointF::dotProduct(ab, ab);
    if (lengthSquared == 0.0) {
        return a;
    }
    const double t = std::clamp(QPointF::dotProduct(p - a, ab) / lengthSquared, 0.0, 1.0);
    return a + ab * t;
}

double distance(QPointF a, QPointF b)
{
    return std::hypot(a.x() - b.x(), a.y() - b.y());
}

} // namespace

QPointF specialObjectCentre(const SpecialObject& m)
{
    if (m.lines.empty()) {
        return {};
    }
    double minX = m.lines[0].x0, maxX = minX, minY = m.lines[0].y0, maxY = minY;
    for (const auto& l : m.lines) {
        minX = std::min({ minX, double(l.x0), double(l.x1) });
        maxX = std::max({ maxX, double(l.x0), double(l.x1) });
        minY = std::min({ minY, double(l.y0), double(l.y1) });
        maxY = std::max({ maxY, double(l.y0), double(l.y1) });
    }
    return QPointF((minX + maxX) / 2, (minY + maxY) / 2);
}

void Level::load(const std::string& filename)
{
    m_fileName = filename;
    if (!std::filesystem::exists(filename)) {
        // file doesn't exist, so we save the name and will write to it when we save
        return;
    }
    const levelformat::LevelData data = levelformat::read(filename);

    auto coord = [](double v) { return static_cast<unsigned>(std::lround(std::max(v, 0.0))); };
    auto angle = [](double degrees) {
        const long a = std::lround(degrees) % 360;
        return static_cast<unsigned>(a < 0 ? a + 360 : a);
    };
    auto addLines = [&](const levelformat::Polyline& polyline, Line line, std::vector<Line>& lines) {
        for (const auto& segment : levelformat::toSegments(polyline)) {
            line.x0 = coord(segment.start.x);
            line.y0 = coord(segment.start.y);
            line.x1 = coord(segment.end.x);
            line.y1 = coord(segment.end.y);
            line.thickness = segment.thickness.value_or(0);
            lines.push_back(line);
        }
    };

    m_levelDescription = data.title;
    m_levelIntroduction = data.introduction;
    if (data.ship.has_value()) {
        m_startPosition = { coord(data.ship->x), coord(data.ship->y), angle(data.ship->rotation) };
    }
    if (data.exit.has_value()) {
        m_exitPosition = { coord(data.exit->x),
                           coord(data.exit->y),
                           angle(data.exit->rotation),
                           data.exit->thickness.value_or(0) };
    }
    for (const auto& f : data.fuel) {
        m_fuelObjects.push_back({ coord(f.x), coord(f.y), f.thickness.value_or(0) });
    }
    for (const auto& polyline : data.walls) {
        addLines(polyline, { 0, 0, 0, 0, 255, 0, 0 }, m_lines);
    }
    for (const auto& breakable : data.breakables) {
        const unsigned group = m_nextBreakableGroup++;
        for (const auto& polyline : breakable.lines) {
            addLines(polyline, { 0, 0, 0, 0, 255, 150, 50, 0, false, true, group }, m_lines);
        }
    }
    for (const auto& polyline : data.decorations) {
        Line line { 0, 0, 0, 0 };
        line.r = DECORATION_COLOUR.red();
        line.g = DECORATION_COLOUR.green();
        line.b = DECORATION_COLOUR.blue();
        line.decoration = true;
        addLines(polyline, line, m_lines);
    }
    for (const auto& m : data.specialObjects) {
        SpecialObject obj;
        obj.xDelta = m.speedX;
        obj.xMaxDifference = m.rangeX;
        obj.yDelta = m.speedY;
        obj.yMaxDifference = m.rangeY;
        obj.rotationDelta = m.spin;
        obj.gravity = m.gravity;
        obj.name = m.name;
        if (m.path.has_value()) {
            for (const auto& offset : m.path->offsets) {
                obj.pathOffsets.emplace_back(offset.x, offset.y);
            }
            obj.pathSpeed = m.path->speed;
            obj.pathLoops = m.path->loop;
        }
        for (const auto& polyline : m.lines) {
            addLines(polyline, { 0, 0, 0, 0, 255, 172, 163 }, obj.lines);
        }
        m_specialObjects.push_back(std::move(obj));
    }
}

void Level::save(const std::string& filename)
{
    auto segment = [](const Line& l) {
        return levelformat::Segment { { static_cast<double>(l.x0), static_cast<double>(l.y0) },
                                    { static_cast<double>(l.x1), static_cast<double>(l.y1) },
                                    l.thickness > 0 ? std::optional(l.thickness) : std::nullopt };
    };
    auto optionalThickness
        = [](double thickness) { return thickness > 0 ? std::optional(thickness) : std::nullopt; };

    levelformat::LevelData data;
    data.title = m_levelDescription;
    data.introduction = m_levelIntroduction;
    if (m_startPosition.has_value()) {
        data.ship = levelformat::Ship { static_cast<double>(m_startPosition->x),
                                      static_cast<double>(m_startPosition->y),
                                      static_cast<double>(m_startPosition->r) };
    }
    if (m_exitPosition.has_value()) {
        data.exit = levelformat::Exit { static_cast<double>(m_exitPosition->x),
                                      static_cast<double>(m_exitPosition->y),
                                      static_cast<double>(m_exitPosition->r),
                                      optionalThickness(m_exitPosition->thickness) };
    }
    for (const auto& f : m_fuelObjects) {
        data.fuel.push_back({ static_cast<double>(f.x),
                              static_cast<double>(f.y),
                              optionalThickness(f.thickness) });
    }
    std::vector<levelformat::Segment> walls;
    std::map<unsigned, std::vector<levelformat::Segment>> breakables; // by group
    std::vector<levelformat::Segment> decorations;
    for (const auto& l : m_lines) {
        if (l.inactive) {
            continue;
        }
        if (l.breakable) {
            breakables[l.breakableGroup].push_back(segment(l));
        } else if (l.decoration) {
            decorations.push_back(segment(l));
        } else {
            walls.push_back(segment(l));
        }
    }
    data.walls = levelformat::toPolylines(walls);
    for (const auto& [group, segments] : breakables) {
        data.breakables.push_back({ levelformat::toPolylines(segments) });
    }
    data.decorations = levelformat::toPolylines(decorations);
    auto addSpecialObject = [&](const SpecialObject& m) {
        std::vector<levelformat::Segment> segments;
        for (const auto& l : m.lines) {
            segments.push_back(segment(l));
        }
        std::optional<levelformat::Path> path;
        if (!m.pathOffsets.empty()) {
            path = levelformat::Path { {}, m.pathSpeed, m.pathLoops };
            for (const QPointF& offset : m.pathOffsets) {
                path->offsets.push_back({ offset.x(), offset.y() });
            }
        }
        data.specialObjects.push_back({ levelformat::toPolylines(segments),
                                       m.xDelta,
                                       m.xMaxDifference,
                                       m.yDelta,
                                       m.yMaxDifference,
                                       m.rotationDelta,
                                       m.gravity,
                                       m.name,
                                       path });
    };
    for (const auto& m : m_specialObjects) {
        addSpecialObject(m);
    }
    // Is there a special object in progress?
    if (!m_currentSpecialObject.lines.empty()) {
        addSpecialObject(m_currentSpecialObject);
    }
    levelformat::write(filename, data);
    m_fileName = filename;
    m_savedStateId = m_stateId;
}

const std::string& Level::fileName() const
{
    return m_fileName;
}

const std::string& Level::description() const
{
    return m_levelDescription;
}

void Level::setDescription(const std::string& description)
{
    if (description != m_levelDescription) {
        checkpoint();
        m_levelDescription = description;
    }
}

bool Level::isDirty() const
{
    return m_stateId != m_savedStateId;
}

Mode Level::mode() const
{
    return m_currentMode;
}

SnapMode Level::snapMode() const
{
    return m_snapMode;
}

unsigned Level::polygonSides() const
{
    return m_currentPolygon.sides;
}

void Level::setPolygonSides(unsigned sides)
{
    m_currentPolygon.sides = std::clamp(sides, 3u, 64u);
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

void Level::draw(QPainter& painter) const
{
    drawGridLines(painter);
    drawEdits(painter);
    drawObjects(painter);
}

void Level::drawEdits(QPainter& painter) const
{
    std::size_t idx = 0;
    for (const auto& l : m_lines) {
        if (!l.inactive) {
            drawLine(painter, l, idx);
        }
        ++idx;
    }
    if (m_currentNearestSnapPoint.has_value()) {
        const auto [x, y] = m_currentNearestSnapPoint.value();
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::magenta);
        painter.drawEllipse(QPointF(x, y), 3.0, 3.0);
    }
    if (m_currentInsertionLine.inactive == false) {
        drawLine(painter, m_currentInsertionLine);
    }
    idx = 0;
    for (const auto& m : m_specialObjects) {
        drawSpecialObjectBoundary(painter, m, idx);
        ++idx;
    }
    for (const auto& l : m_currentSpecialObject.lines) {
        drawLine(painter, l);
    }
    for (const auto& l : m_currentPolygon.lines) {
        drawLine(painter, l);
    }
}

void Level::drawSpecialObjectBoundary(QPainter& painter, const SpecialObject& m, size_t idx) const
{
    for (auto l : m.lines) {
        if (m_selectedSpecialObjects.contains(idx)) {
            l.r = 255;
            l.g = 255;
            l.b = 255;
        }
        drawLine(painter, l);
    }
    if (m.lines.empty()) {
        return;
    }
    // The furthest any vertex is from the centre, which it can reach when spinning
    const QPointF centre = specialObjectCentre(m);
    double maxRadius = 0;
    for (const auto& l : m.lines) {
        maxRadius = std::max(
            { maxRadius,
              distance(centre, QPointF(l.x0, l.y0)),
              distance(centre, QPointF(l.x1, l.y1)) });
    }

    if (!m.pathOffsets.empty()) {
        drawSpecialObjectPath(painter, m);
        if (m.rotationDelta != 0) {
            // Where it can reach while spinning, at each point of the path
            drawCircle(painter, maxRadius, centre.x(), centre.y());
            for (const QPointF& offset : m.pathOffsets) {
                drawCircle(painter, maxRadius, centre.x() + offset.x(), centre.y() + offset.y());
            }
        }
        return;
    }

    // Bouncing within a box
    const double rangeX = m.xDelta != 0 ? m.xMaxDifference : 0;
    const double rangeY = m.yDelta != 0 ? m.yMaxDifference : 0;
    if (m.rotationDelta == 0) {
        QRectF bounds;
        for (const auto& l : m.lines) {
            bounds |= QRectF(QPointF(l.x0, l.y0), QPointF(l.x1, l.y1)).normalized();
        }
        painter.setPen(hairline(BOUNDARY_COLOUR));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(bounds.adjusted(-rangeX - 1, -rangeY - 1, rangeX + 1, rangeY + 1));
    } else if (rangeX > 0 || rangeY > 0) {
        // Circumscribe all possible positions
        drawRoundedRect(
            painter,
            centre.x() - rangeX - maxRadius, // top left
            centre.y() - rangeY - maxRadius,
            2 * (rangeX + maxRadius),
            2 * (rangeY + maxRadius),
            maxRadius);
    } else {
        // It's just spinning, so a circle around it shows all possible positions
        drawCircle(painter, maxRadius, centre.x(), centre.y());
    }
}

void Level::drawSpecialObjectPath(QPainter& painter, const SpecialObject& m) const
{
    // The object's outline at each point of the path
    painter.setPen(hairline(SPECIAL_OBJECT_GHOST_COLOUR));
    for (const QPointF& offset : m.pathOffsets) {
        for (const auto& l : m.lines) {
            painter.drawLine(QPointF(l.x0, l.y0) + offset, QPointF(l.x1, l.y1) + offset);
        }
    }
    // The route its centre takes
    const QPointF centre = specialObjectCentre(m);
    QPolygonF route { centre };
    for (const QPointF& offset : m.pathOffsets) {
        route << centre + offset;
    }
    QPen pen = hairline(BOUNDARY_COLOUR);
    pen.setStyle(Qt::DashLine);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    if (m.pathLoops) {
        painter.drawPolygon(route);
    } else {
        painter.drawPolyline(route);
    }
    // Small arrowheads show which way it goes from the start
    const QPointF towards = route[1] - route[0];
    const double length = std::hypot(towards.x(), towards.y());
    if (length > 0) {
        const QPointF direction = towards / length;
        const QPointF normal(-direction.y(), direction.x());
        const QPointF tip = route[0] + direction * std::min(length / 2, 30.0);
        painter.setPen(hairline(BOUNDARY_COLOUR));
        painter.drawLine(tip, tip - direction * 8 + normal * 5);
        painter.drawLine(tip, tip - direction * 8 - normal * 5);
    }
}

void Level::drawCircle(QPainter& painter, float maxRadius, float centreX, float centreY) const
{
    painter.setPen(hairline(BOUNDARY_COLOUR));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QPointF(centreX, centreY), maxRadius, maxRadius);
}

void Level::drawRoundedRect(QPainter& painter, float x, float y, float w, float h, float r) const
{
    // Ensure radius isn't too big for size:
    r = std::min(r, std::min(w, h) / 2);
    painter.setPen(hairline(BOUNDARY_COLOUR));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(QRectF(x, y, w, h), r, r);
}

void Level::drawLine(QPainter& painter, const Line& l, std::optional<std::size_t> idx) const
{
    const bool highlighted = idx.has_value() && m_highlightedLineIndices.contains(*idx);
    painter.setPen(hairline(highlighted ? QColor(Qt::white) : QColor(l.r, l.g, l.b)));
    painter.drawLine(QPointF(l.x0, l.y0), QPointF(l.x1, l.y1));
}

void Level::drawGridLines(QPainter& painter) const
{
    for (unsigned n = 0; n <= MAP_SIZE; n += 50) {
        drawLine(painter, { n, 0, n, 2000, 0, 100, 0, 1 });
        drawLine(painter, { 0, n, 2000, n, 0, 100, 0, 1 });
    }
}

void Level::drawObjects(QPainter& painter) const
{
    if (m_startPosition.has_value()) {
        painter.save();
        painter.translate(m_startPosition.value().x, m_startPosition.value().y);
        painter.rotate(360.0 - m_startPosition.value().r);
        painter.setRenderHint(QPainter::Antialiasing);
        // In the game the ship's lines are a little thicker than the walls'
        QPen pen = hairline(m_shipSelected ? SELECTED_COLOUR : SHIP_COLOUR);
        pen.setWidthF(1.5);
        pen.setJoinStyle(Qt::RoundJoin);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawPolygon(SHIP_OUTLINE);
        painter.restore();
    }
    if (m_exitPosition.has_value()) {
        QFont font = painter.font();
        font.setPixelSize(26);
        painter.setFont(font);
        painter.setPen(m_exitSelected ? SELECTED_COLOUR : QColor(52, 213, 235));
        painter.save();
        painter.translate(m_exitPosition->x, m_exitPosition->y);
        painter.rotate(360.0 - m_exitPosition->r);
        painter.drawText(QRectF(-100, -50, 200, 100), Qt::AlignCenter, "EXIT");
        painter.restore();
    }
    for (std::size_t i = 0; i < m_fuelObjects.size(); ++i) {
        const QPointF centre(m_fuelObjects[i].x, m_fuelObjects[i].y);
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::yellow);
        painter.drawEllipse(centre, FUEL_RADIUS, FUEL_RADIUS);
        if (m_selectedFuel.contains(i)) {
            painter.setPen(hairline(SELECTED_COLOUR));
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(centre, FUEL_RADIUS + 3, FUEL_RADIUS + 3);
        }
    }
}

// ---------------------------------------------------------------------------
// Hit testing and snapping
// ---------------------------------------------------------------------------

std::optional<std::size_t> Level::lineUnderCursor(QPointF w, float zoomLevel) const
{
    // We define four lines which bound the cursor in a diamond shape, and then check each
    // of these to see if they intersect any line on the workspace.
    std::size_t idx = 0;
    const float selectionBoxSize = 1 + 4 * zoomLevel;
    for (const auto& l : m_lines) {
        if (!l.inactive) {
            if (utils::doLinesIntersect(
                    w.x() - selectionBoxSize,
                    w.y(),
                    w.x(),
                    w.y() - selectionBoxSize,
                    l.x0,
                    l.y0,
                    l.x1,
                    l.y1)) {
                return idx;
            }
            if (utils::doLinesIntersect(
                    w.x(),
                    w.y() - selectionBoxSize,
                    w.x() + selectionBoxSize,
                    w.y(),
                    l.x0,
                    l.y0,
                    l.x1,
                    l.y1)) {
                return idx;
            }
            if (utils::doLinesIntersect(
                    w.x() + selectionBoxSize,
                    w.y(),
                    w.x(),
                    w.y() + selectionBoxSize,
                    l.x0,
                    l.y0,
                    l.x1,
                    l.y1)) {
                return idx;
            }
            if (utils::doLinesIntersect(
                    w.x(),
                    w.y() + selectionBoxSize,
                    w.x() - selectionBoxSize,
                    w.y(),
                    l.x0,
                    l.y0,
                    l.x1,
                    l.y1)) {
                return idx;
            }
        }
        ++idx;
    }
    return std::nullopt;
}

std::optional<std::size_t> Level::specialObjectUnderCursor(QPointF w) const
{
    // Note, this returns the index of the entire special object, not the individual line
    const long x = w.x();
    const long y = w.y();
    std::size_t idx = 0;
    for (const auto& m : m_specialObjects) {
        for (const auto& l : m.lines) {
            if (!l.inactive) {
                if (utils::doLinesIntersect(x - 10, y, x, y - 10, l.x0, l.y0, l.x1, l.y1)) {
                    return idx;
                }
                if (utils::doLinesIntersect(x, y - 10, x + 10, y, l.x0, l.y0, l.x1, l.y1)) {
                    return idx;
                }
                if (utils::doLinesIntersect(x + 10, y, x, y + 10, l.x0, l.y0, l.x1, l.y1)) {
                    return idx;
                }
                if (utils::doLinesIntersect(x, y + 10, x - 10, y, l.x0, l.y0, l.x1, l.y1)) {
                    return idx;
                }
            }
        }
        ++idx;
    }
    return std::nullopt;
}

void Level::highlightGridVertex(QPointF w)
{
    const unsigned x = static_cast<unsigned>(w.x() / 50.0 + 0.5) * 50.0;
    const unsigned y = static_cast<unsigned>(w.y() / 50.0 + 0.5) * 50.0;
    if (x > MAP_SIZE || y > MAP_SIZE) {
        m_currentNearestSnapPoint = std::nullopt;
    } else {
        m_currentNearestSnapPoint = std::tie(x, y);
    }
}

void Level::highlightNearestLinePoint(QPointF w, std::optional<std::pair<unsigned, unsigned>> exclude)
{
    auto check = [&](const Line& l) {
        if (l.inactive) {
            return false;
        }
        if (exclude.has_value()
            && ((l.x0 == exclude->first && l.y0 == exclude->second)
                || (l.x1 == exclude->first && l.y1 == exclude->second))) {
            return false;
        }
        auto nearest = utils::closestPointOnLine(l.x0, l.y0, l.x1, l.y1, w.x(), w.y(), 5);
        if (nearest.has_value()) {
            m_currentNearestSnapPoint = std::tie(nearest.value().first, nearest.value().second);
            return true;
        }
        return false;
    };
    for (const auto& l : m_lines) {
        if (check(l)) {
            return;
        }
    }
    for (const auto& m : m_specialObjects) {
        for (const auto& l : m.lines) {
            if (check(l)) {
                return;
            }
        }
    }
    // Also check special object in progress
    for (const auto& l : m_currentSpecialObject.lines) {
        if (check(l)) {
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

void Level::updateSnapPoint(QPointF w, std::optional<std::pair<unsigned, unsigned>> exclude)
{
    // Snap to whatever's near enough (lines take precedence over grid
    // points); if there's nothing, points go exactly where the mouse is
    m_currentNearestSnapPoint = std::nullopt;
    if (m_snapMode == SnapMode::AUTO || m_snapMode == SnapMode::GRID) {
        highlightGridVertex(w);
    }
    if (m_snapMode == SnapMode::AUTO || m_snapMode == SnapMode::LINE) {
        highlightNearestLinePoint(w, exclude);
    }
}

void Level::mouseMoved(QPointF w)
{
    if (m_currentMode == Mode::LINE || m_currentMode == Mode::BREAKABLE
        || m_currentMode == Mode::DECORATION || m_currentMode == Mode::SPECIAL) {
        updateSnapPoint(w);
        if (m_currentInsertionLine.inactive == false) {
            if (m_currentNearestSnapPoint.has_value()) {
                m_currentInsertionLine.x1 = std::get<0>(m_currentNearestSnapPoint.value());
                m_currentInsertionLine.y1 = std::get<1>(m_currentNearestSnapPoint.value());
            } else {
                m_currentInsertionLine.x1 = w.x();
                m_currentInsertionLine.y1 = w.y();
            }
        }
    } else if (m_currentMode == Mode::POLYGON_RADIUS) {
        m_currentPolygon.lines = utils::getRegularPolygon(
            w.x(),
            w.y(),
            *m_currentPolygon.centreX,
            *m_currentPolygon.centreY,
            m_currentPolygon.sides);
    } else {
        m_currentNearestSnapPoint = std::nullopt;
    }
}

void Level::leftClicked(QPointF w, float zoomLevel, bool extendSelection, bool singleLine)
{
    // e.g. selecting something else ends a series of nudges which undo together
    m_lastChangeWasNudge = false;
    switch (m_currentMode) {
        case Mode::LINE:
        case Mode::BREAKABLE:
        case Mode::DECORATION:
        case Mode::SPECIAL:
            {
                // Insert a new line
                {
                    // Are we in the middle of drawing a line? If so, add the current
                    // insertion line into the vector
                    if (!m_currentInsertionLine.inactive) {
                        if (m_currentInsertionLine.x0 != m_currentInsertionLine.x1
                            || m_currentInsertionLine.y0 != m_currentInsertionLine.y1) {
                            checkpoint();
                            m_currentInsertionLine.breakable = m_currentMode == Mode::BREAKABLE;
                            m_currentInsertionLine.decoration = m_currentMode == Mode::DECORATION;
                            // Each breakable line drawn breaks separately
                            m_currentInsertionLine.breakableGroup
                                = m_currentMode == Mode::BREAKABLE ? m_nextBreakableGroup++ : 0;
                            if (m_currentMode == Mode::SPECIAL) {
                                Line l = m_currentInsertionLine;
                                l.r = 255;
                                l.g = 172;
                                l.b = 163;
                                m_currentSpecialObject.lines.push_back(l);
                            } else {
                                m_lines.push_back(m_currentInsertionLine);
                            }
                            // next line starts at the current line's end:
                            m_currentInsertionLine.x0 = m_currentInsertionLine.x1;
                            m_currentInsertionLine.y0 = m_currentInsertionLine.y1;
                        }
                    } else {
                        // else this is a new line
                        unsigned x;
                        unsigned y;
                        if (!m_currentNearestSnapPoint.has_value()) {
                            x = w.x();
                            y = w.y();
                        } else {
                            x = std::get<0>(m_currentNearestSnapPoint.value());
                            y = std::get<1>(m_currentNearestSnapPoint.value());
                        }
                        m_currentInsertionLine.x0 = x;
                        m_currentInsertionLine.y0 = y;
                        m_currentInsertionLine.x1 = x;
                        m_currentInsertionLine.y1 = y;
                        m_currentInsertionLine.inactive = false;
                    }
                }
                break;
            }
        case Mode::EDIT:
            {
                const auto item = itemAt(w, zoomLevel);
                if (!extendSelection) {
                    clearSelection();
                }
                if (item.has_value()) {
                    if (extendSelection && isSelected(*item)) {
                        deselect(*item);
                    } else {
                        select(*item, !singleLine);
                    }
                }
                break;
            }
        case Mode::START:
            placeShip(w);
            break;
        case Mode::EXIT:
            placeExit(w);
            break;
        case Mode::FUEL:
            {
                // if we are within r workspace units of an existing fuel object, we
                // treat this as a request to delete it instead of placing a new one
                unsigned r = 20;
                checkpoint();
                std::size_t idx = 0;
                bool erased { false };
                for (const auto& f : m_fuelObjects) {
                    if ((f.x > w.x() - r && f.x < w.x() + r)
                        && (f.y > w.y() - r && f.y < w.y() + r)) {
                        m_fuelObjects.erase(m_fuelObjects.begin() + idx);
                        erased = true;
                        break;
                    }
                    ++idx;
                }
                if (!erased) {
                    m_fuelObjects.push_back(
                        { static_cast<unsigned>(w.x()), static_cast<unsigned>(w.y()) });
                }
                break;
            }
        case Mode::POLYGON_CENTRE:
            {
                m_currentPolygon.centreX = w.x();
                m_currentPolygon.centreY = w.y();
                changeMode(Mode::POLYGON_RADIUS);
                break;
            }
        case Mode::POLYGON_RADIUS:
            {
                // Finalise an in-progress polygon
                auto maxDist = std::max(
                    std::abs(static_cast<float>(w.x()) - *m_currentPolygon.centreX),
                    std::abs(static_cast<float>(w.y()) - *m_currentPolygon.centreY));
                if (maxDist > 5.f) { // arbitrary lower limit for polygons' radii
                    checkpoint();
                    m_lines.insert(
                        m_lines.end(),
                        m_currentPolygon.lines.begin(),
                        m_currentPolygon.lines.end());
                    m_currentPolygon.lines.clear();
                    changeMode(Mode::POLYGON_CENTRE);
                }
                break;
            }
    }
}

void Level::finishCurrentLine()
{
    m_currentInsertionLine.inactive = true;
    finishCurrentSpecialObject();
}

void Level::placeShip(QPointF w)
{
    // Keeping its rotation, and selecting it, ready to be moved or rotated
    checkpoint();
    const unsigned rotation = m_startPosition.has_value() ? m_startPosition->r : 0;
    m_startPosition = { static_cast<unsigned>(w.x()), static_cast<unsigned>(w.y()), rotation };
    changeMode(Mode::EDIT);
    clearSelection();
    m_shipSelected = true;
}

void Level::placeExit(QPointF w)
{
    // As for the ship
    checkpoint();
    ExitPosition exit { static_cast<unsigned>(w.x()), static_cast<unsigned>(w.y()) };
    if (m_exitPosition.has_value()) {
        exit.r = m_exitPosition->r;
        exit.thickness = m_exitPosition->thickness;
    }
    m_exitPosition = exit;
    changeMode(Mode::EDIT);
    clearSelection();
    m_exitSelected = true;
}

void Level::addFuel(QPointF w)
{
    checkpoint();
    m_fuelObjects.push_back({ static_cast<unsigned>(w.x()), static_cast<unsigned>(w.y()) });
}

bool Level::isDrawing() const
{
    return !m_currentInsertionLine.inactive || m_currentMode == Mode::POLYGON_RADIUS
        || !m_currentSpecialObject.lines.empty();
}

// ---------------------------------------------------------------------------
// Selection
// ---------------------------------------------------------------------------

std::optional<Level::Item> Level::itemAt(QPointF w, float zoomLevel) const
{
    auto near = [&w](double x, double y, double halfWidth, double halfHeight) {
        return std::abs(w.x() - x) <= halfWidth && std::abs(w.y() - y) <= halfHeight;
    };
    if (m_startPosition.has_value()
        && near(m_startPosition->x, m_startPosition->y, SHIP_RADIUS, SHIP_RADIUS)) {
        return Item { Item::Type::SHIP };
    }
    if (m_exitPosition.has_value()
        && near(
            m_exitPosition->x, m_exitPosition->y, exitHalfSize().width(), exitHalfSize().height())) {
        return Item { Item::Type::EXIT };
    }
    for (std::size_t i = 0; i < m_fuelObjects.size(); ++i) {
        if (near(m_fuelObjects[i].x, m_fuelObjects[i].y, FUEL_RADIUS, FUEL_RADIUS)) {
            return Item { Item::Type::FUEL, i };
        }
    }
    if (auto line = lineUnderCursor(w, zoomLevel)) {
        return Item { Item::Type::LINE, *line };
    }
    if (auto specialObject = specialObjectUnderCursor(w)) {
        return Item { Item::Type::SPECIAL_OBJECT, *specialObject };
    }
    return std::nullopt;
}

QSizeF Level::exitHalfSize() const
{
    // The exit's text, rotated, fits in this box
    const double radians = (m_exitPosition.has_value() ? m_exitPosition->r : 0) * std::numbers::pi / 180.0;
    const double c = std::abs(std::cos(radians));
    const double s = std::abs(std::sin(radians));
    return { EXIT_HALF_WIDTH * c + EXIT_HALF_HEIGHT * s, EXIT_HALF_WIDTH * s + EXIT_HALF_HEIGHT * c };
}

bool Level::isSelected(const Item& item) const
{
    switch (item.type) {
        case Item::Type::SHIP:
            return m_shipSelected;
        case Item::Type::EXIT:
            return m_exitSelected;
        case Item::Type::FUEL:
            return m_selectedFuel.contains(item.index);
        case Item::Type::LINE:
            return m_highlightedLineIndices.contains(item.index);
        case Item::Type::SPECIAL_OBJECT:
            return m_selectedSpecialObjects.contains(item.index);
    }
    return false;
}

void Level::select(const Item& item, bool includeConnectedLines)
{
    switch (item.type) {
        case Item::Type::SHIP:
            m_shipSelected = true;
            break;
        case Item::Type::EXIT:
            m_exitSelected = true;
            break;
        case Item::Type::FUEL:
            m_selectedFuel.insert(item.index);
            break;
        case Item::Type::LINE:
            m_highlightedLineIndices.insert(item.index);
            if (includeConnectedLines) {
                addConnectedLinesToHighlight(m_lines[item.index]);
            }
            break;
        case Item::Type::SPECIAL_OBJECT:
            m_selectedSpecialObjects.insert(item.index);
            break;
    }
}

void Level::deselect(const Item& item)
{
    switch (item.type) {
        case Item::Type::SHIP:
            m_shipSelected = false;
            break;
        case Item::Type::EXIT:
            m_exitSelected = false;
            break;
        case Item::Type::FUEL:
            m_selectedFuel.erase(item.index);
            break;
        case Item::Type::LINE:
            // TODO remove connected lines too?
            m_highlightedLineIndices.erase(item.index);
            break;
        case Item::Type::SPECIAL_OBJECT:
            m_selectedSpecialObjects.erase(item.index);
            break;
    }
}

bool Level::isItemAt(QPointF pos, float zoomLevel) const
{
    return itemAt(pos, zoomLevel).has_value();
}

bool Level::isSelectedItemAt(QPointF pos, float zoomLevel) const
{
    const auto item = itemAt(pos, zoomLevel);
    return item.has_value() && isSelected(*item);
}

void Level::selectOnlyItemAt(QPointF pos, float zoomLevel, bool singleLine)
{
    clearSelection();
    if (const auto item = itemAt(pos, zoomLevel)) {
        select(*item, !singleLine);
    }
}

void Level::selectInRect(const QRectF& rect, bool extendSelection)
{
    if (!extendSelection) {
        clearSelection();
    }
    auto inside = [&rect](double x, double y, double halfWidth = 0, double halfHeight = 0) {
        return rect.contains(QPointF(x - halfWidth, y - halfHeight))
            && rect.contains(QPointF(x + halfWidth, y + halfHeight));
    };
    auto lineInside = [&](const Line& l) { return inside(l.x0, l.y0) && inside(l.x1, l.y1); };
    for (std::size_t i = 0; i < m_lines.size(); ++i) {
        if (!m_lines[i].inactive && lineInside(m_lines[i])) {
            m_highlightedLineIndices.insert(i);
        }
    }
    for (std::size_t i = 0; i < m_specialObjects.size(); ++i) {
        if (std::ranges::all_of(m_specialObjects[i].lines, lineInside)) {
            m_selectedSpecialObjects.insert(i);
        }
    }
    for (std::size_t i = 0; i < m_fuelObjects.size(); ++i) {
        if (inside(m_fuelObjects[i].x, m_fuelObjects[i].y, FUEL_RADIUS, FUEL_RADIUS)) {
            m_selectedFuel.insert(i);
        }
    }
    if (m_exitPosition.has_value()
        && inside(
            m_exitPosition->x, m_exitPosition->y, exitHalfSize().width(), exitHalfSize().height())) {
        m_exitSelected = true;
    }
    if (m_startPosition.has_value()
        && inside(m_startPosition->x, m_startPosition->y, SHIP_RADIUS, SHIP_RADIUS)) {
        m_shipSelected = true;
    }
}

void Level::selectAll()
{
    selectInRect(QRectF(0, 0, MAP_SIZE, MAP_SIZE).adjusted(-100, -100, 100, 100), false);
}

void Level::clearSelection()
{
    m_highlightedLineIndices.clear();
    m_selectedSpecialObjects.clear();
    m_selectedFuel.clear();
    m_exitSelected = false;
    m_shipSelected = false;
}

bool Level::hasSelection() const
{
    return hasSelectedLines() || !m_selectedSpecialObjects.empty() || !m_selectedFuel.empty()
        || m_exitSelected || m_shipSelected;
}

bool Level::hasSelectedLines() const
{
    return !m_highlightedLineIndices.empty();
}

std::optional<QRectF> Level::selectionBounds() const
{
    bool any = false;
    double minX = 0, minY = 0, maxX = 0, maxY = 0;
    auto add = [&](double x, double y, double halfWidth = 0, double halfHeight = 0) {
        if (!any) {
            minX = x - halfWidth;
            maxX = x + halfWidth;
            minY = y - halfHeight;
            maxY = y + halfHeight;
            any = true;
        } else {
            minX = std::min(minX, x - halfWidth);
            maxX = std::max(maxX, x + halfWidth);
            minY = std::min(minY, y - halfHeight);
            maxY = std::max(maxY, y + halfHeight);
        }
    };
    auto addLine = [&](const Line& l) {
        add(l.x0, l.y0);
        add(l.x1, l.y1);
    };
    for (std::size_t i : m_highlightedLineIndices) {
        addLine(m_lines[i]);
    }
    for (std::size_t i : m_selectedSpecialObjects) {
        std::ranges::for_each(m_specialObjects[i].lines, addLine);
    }
    for (std::size_t i : m_selectedFuel) {
        add(m_fuelObjects[i].x, m_fuelObjects[i].y, FUEL_RADIUS, FUEL_RADIUS);
    }
    if (m_exitSelected && m_exitPosition.has_value()) {
        add(m_exitPosition->x, m_exitPosition->y, exitHalfSize().width(), exitHalfSize().height());
    }
    if (m_shipSelected && m_startPosition.has_value()) {
        add(m_startPosition->x, m_startPosition->y, SHIP_RADIUS, SHIP_RADIUS);
    }
    if (!any) {
        return std::nullopt;
    }
    return QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
}

template <typename Fn> void Level::forEachSelectedPoint(Fn fn)
{
    for (std::size_t i : m_highlightedLineIndices) {
        fn(m_lines[i].x0, m_lines[i].y0);
        fn(m_lines[i].x1, m_lines[i].y1);
    }
    for (std::size_t i : m_selectedSpecialObjects) {
        for (auto& l : m_specialObjects[i].lines) {
            fn(l.x0, l.y0);
            fn(l.x1, l.y1);
        }
    }
    for (std::size_t i : m_selectedFuel) {
        fn(m_fuelObjects[i].x, m_fuelObjects[i].y);
    }
    if (m_exitSelected && m_exitPosition.has_value()) {
        fn(m_exitPosition->x, m_exitPosition->y);
    }
    if (m_shipSelected && m_startPosition.has_value()) {
        fn(m_startPosition->x, m_startPosition->y);
    }
}

void Level::deleteSelection()
{
    if (!hasSelection()) {
        return;
    }
    checkpoint();
    for (const size_t i : m_highlightedLineIndices) {
        m_lines[i].inactive = true;
    }
    // Erase from the end, so the indices of those still to erase stay valid
    for (auto i = m_selectedSpecialObjects.rbegin(); i != m_selectedSpecialObjects.rend(); ++i) {
        m_specialObjects.erase(m_specialObjects.begin() + *i);
    }
    for (auto i = m_selectedFuel.rbegin(); i != m_selectedFuel.rend(); ++i) {
        m_fuelObjects.erase(m_fuelObjects.begin() + *i);
    }
    if (m_exitSelected) {
        m_exitPosition = std::nullopt;
    }
    if (m_shipSelected) {
        m_startPosition = std::nullopt;
    }
    clearSelection();
}

void Level::translateSelection(int x, int y)
{
    // Limit the movement to keep the selection on the map
    long minX = std::numeric_limits<long>::max();
    long minY = std::numeric_limits<long>::max();
    long maxX = std::numeric_limits<long>::min();
    long maxY = std::numeric_limits<long>::min();
    forEachSelectedPoint([&](unsigned& px, unsigned& py) {
        minX = std::min<long>(minX, px);
        maxX = std::max<long>(maxX, px);
        minY = std::min<long>(minY, py);
        maxY = std::max<long>(maxY, py);
    });
    if (minX > maxX) {
        return; // nothing selected
    }
    const long mapSize = static_cast<long>(MAP_SIZE);
    const long dx = std::clamp<long>(x, -minX, mapSize - maxX);
    const long dy = std::clamp<long>(y, -minY, mapSize - maxY);
    forEachSelectedPoint([&](unsigned& px, unsigned& py) {
        px = static_cast<unsigned>(px + dx);
        py = static_cast<unsigned>(py + dy);
    });
}

void Level::nudgeSelection(int x, int y)
{
    if (!hasSelection()) {
        return;
    }
    // Consecutive nudges undo together
    checkpoint(Change::Nudge);
    translateSelection(x, y);
}

void Level::beginTransform()
{
    m_transformOrigin = snapshot();
    m_transformCheckpointed = false;
    m_lastValidRotation = 0.0;
    m_lastChangeWasNudge = false;
}

void Level::transformCheckpoint()
{
    // The contents are as at the start of the transform when this is first
    // called, so that's what's undone to
    if (!m_transformCheckpointed) {
        checkpoint();
        m_transformCheckpointed = true;
    }
}

void Level::moveSelection(int x, int y)
{
    if (!m_transformOrigin.has_value()) {
        return;
    }
    restoreContents(*m_transformOrigin);
    if (x != 0 || y != 0) {
        transformCheckpoint();
        translateSelection(x, y);
    }
}

bool Level::applyRotation(double degrees, QPointF centre)
{
    const double radians = degrees * std::numbers::pi / 180.0;
    const double cosine = std::cos(radians);
    const double sine = std::sin(radians);
    // y is down, so this rotates clockwise on screen
    auto rotated = [&](unsigned x, unsigned y) {
        const double dx = x - centre.x();
        const double dy = y - centre.y();
        return QPointF(
            std::round(centre.x() + dx * cosine - dy * sine),
            std::round(centre.y() + dx * sine + dy * cosine));
    };
    // Don't rotate anything off the map
    bool onMap = true;
    forEachSelectedPoint([&](unsigned& x, unsigned& y) {
        const QPointF p = rotated(x, y);
        if (p.x() < 0 || p.y() < 0 || p.x() > MAP_SIZE || p.y() > MAP_SIZE) {
            onMap = false;
        }
    });
    if (!onMap) {
        return false;
    }
    if (degrees == 0.0) {
        return true;
    }
    transformCheckpoint();
    forEachSelectedPoint([&](unsigned& x, unsigned& y) {
        const QPointF p = rotated(x, y);
        x = static_cast<unsigned>(p.x());
        y = static_cast<unsigned>(p.y());
    });
    // The ship's and exit's rotations are anticlockwise
    auto turn = [degrees](unsigned& rotation) {
        const long r = std::lround(rotation - degrees) % 360;
        rotation = static_cast<unsigned>(r < 0 ? r + 360 : r);
    };
    if (m_shipSelected && m_startPosition.has_value()) {
        turn(m_startPosition->r);
    }
    if (m_exitSelected && m_exitPosition.has_value()) {
        turn(m_exitPosition->r);
    }
    // Paths turn with their objects
    for (std::size_t i : m_selectedSpecialObjects) {
        for (QPointF& offset : m_specialObjects[i].pathOffsets) {
            offset = QPointF(
                std::round(offset.x() * cosine - offset.y() * sine),
                std::round(offset.x() * sine + offset.y() * cosine));
        }
    }
    return true;
}

void Level::rotateSelection(double degrees, QPointF centre)
{
    if (!m_transformOrigin.has_value()) {
        return;
    }
    restoreContents(*m_transformOrigin);
    if (applyRotation(degrees, centre)) {
        m_lastValidRotation = degrees;
    } else {
        // It would go off the map, so stay at the last rotation which didn't
        applyRotation(m_lastValidRotation, centre);
    }
}

void Level::endTransform()
{
    if (m_draggedVertex.has_value() && m_transformCheckpointed) {
        // Remove any lines the drag has reduced to nothing
        for (std::size_t i = 0; i < m_lines.size(); ++i) {
            Line& l = m_lines[i];
            if (!l.inactive && l.x0 == l.x1 && l.y0 == l.y1) {
                l.inactive = true;
                m_highlightedLineIndices.erase(i);
            }
        }
    }
    m_draggedVertex = std::nullopt;
    m_draggedPathPoint = std::nullopt;
    m_currentNearestSnapPoint = std::nullopt;
    m_transformOrigin = std::nullopt;
}

void Level::convertSelectionToSpecialObject()
{
    if (m_highlightedLineIndices.empty()) {
        return;
    }
    checkpoint();
    SpecialObject m;
    for (std::size_t i : m_highlightedLineIndices) {
        m_lines[i].r = 255;
        m_lines[i].g = 172;
        m_lines[i].b = 163;
        m_lines[i].breakable = false;
        m_lines[i].decoration = false;
        m.lines.push_back(m_lines[i]);
        m_lines[i].inactive = true;
    }
    m_specialObjects.push_back(m);
    m_highlightedLineIndices.clear();
    m_selectedSpecialObjects.insert(m_specialObjects.size() - 1);
}

bool Level::canConvertSelectionToDecoration() const
{
    return std::any_of(m_highlightedLineIndices.begin(),
                       m_highlightedLineIndices.end(),
                       [this](std::size_t i) { return !m_lines[i].decoration; });
}

bool Level::canConvertSelectionToWalls() const
{
    return std::any_of(
        m_highlightedLineIndices.begin(), m_highlightedLineIndices.end(), [this](std::size_t i) {
            return m_lines[i].decoration || m_lines[i].breakable;
        });
}

void Level::convertSelectionToDecoration()
{
    if (!canConvertSelectionToDecoration()) {
        return;
    }
    checkpoint();
    for (std::size_t i : m_highlightedLineIndices) {
        Line& l = m_lines[i];
        l.r = DECORATION_COLOUR.red();
        l.g = DECORATION_COLOUR.green();
        l.b = DECORATION_COLOUR.blue();
        l.breakable = false;
        l.breakableGroup = 0;
        l.decoration = true;
    }
}

void Level::convertSelectionToWalls()
{
    if (!canConvertSelectionToWalls()) {
        return;
    }
    checkpoint();
    for (std::size_t i : m_highlightedLineIndices) {
        Line& l = m_lines[i];
        l.r = 255;
        l.g = 0;
        l.b = 0;
        l.breakable = false;
        l.breakableGroup = 0;
        l.decoration = false;
    }
}

std::optional<SpecialObject> Level::selectedSpecialObject() const
{
    if (m_selectedSpecialObjects.size() != 1) {
        return std::nullopt;
    }
    return m_specialObjects[*m_selectedSpecialObjects.begin()];
}

void Level::setSelectedSpecialObjectMotion(const SpecialObject& motion)
{
    if (m_selectedSpecialObjects.size() != 1) {
        return;
    }
    checkpoint();
    auto& obj = m_specialObjects[*m_selectedSpecialObjects.begin()];
    obj.xDelta = motion.xDelta;
    obj.xMaxDifference = motion.xMaxDifference;
    obj.yDelta = motion.yDelta;
    obj.yMaxDifference = motion.yMaxDifference;
    obj.rotationDelta = motion.rotationDelta;
    obj.gravity = motion.gravity;
    obj.pathOffsets = motion.pathOffsets;
    obj.pathSpeed = motion.pathSpeed;
    obj.pathLoops = motion.pathLoops;
    if (!obj.pathOffsets.empty()) {
        // A path replaces bouncing within a box
        obj.xDelta = obj.xMaxDifference = obj.yDelta = obj.yMaxDifference = 0;
    }
}

SpecialObject* Level::singleSelectedSpecialObject()
{
    if (m_selectedSpecialObjects.size() != 1) {
        return nullptr;
    }
    return &m_specialObjects[*m_selectedSpecialObjects.begin()];
}

const SpecialObject* Level::singleSelectedSpecialObject() const
{
    if (m_selectedSpecialObjects.size() != 1) {
        return nullptr;
    }
    return &m_specialObjects[*m_selectedSpecialObjects.begin()];
}

void Level::setPathEndAt(QPointF pos)
{
    SpecialObject* m = singleSelectedSpecialObject();
    if (m == nullptr) {
        return;
    }
    checkpoint();
    const QPointF offset = roundedOffset(pos - specialObjectCentre(*m));
    if (m->pathOffsets.empty()) {
        m->pathOffsets.push_back(offset);
        if (m->pathSpeed == 0) {
            m->pathSpeed = DEFAULT_PATH_SPEED;
        }
        m->xDelta = m->xMaxDifference = m->yDelta = m->yMaxDifference = 0;
    } else {
        m->pathOffsets.back() = offset;
    }
}

void Level::addPathPointAt(QPointF pos)
{
    SpecialObject* m = singleSelectedSpecialObject();
    if (m == nullptr || m->pathOffsets.empty()) {
        return;
    }
    checkpoint();
    m->pathOffsets.push_back(roundedOffset(pos - specialObjectCentre(*m)));
}

std::optional<std::size_t> Level::pathPointAt(QPointF pos, float zoomLevel) const
{
    std::optional<std::size_t> nearest;
    double nearestDistance = VERTEX_HIT_PIXELS * zoomLevel;
    const auto points = selectedPathPoints();
    for (std::size_t i = 0; i < points.size(); ++i) {
        const double d = distance(pos, points[i]);
        if (d <= nearestDistance) {
            nearest = i;
            nearestDistance = d;
        }
    }
    return nearest;
}

void Level::removePathPoint(std::size_t index)
{
    SpecialObject* m = singleSelectedSpecialObject();
    if (m == nullptr || index >= m->pathOffsets.size()) {
        return;
    }
    checkpoint();
    m->pathOffsets.erase(m->pathOffsets.begin() + static_cast<std::ptrdiff_t>(index));
}

std::vector<QPointF> Level::selectedPathPoints() const
{
    const SpecialObject* m = singleSelectedSpecialObject();
    if (m == nullptr) {
        return {};
    }
    const QPointF centre = specialObjectCentre(*m);
    std::vector<QPointF> points;
    for (const QPointF& offset : m->pathOffsets) {
        points.push_back(centre + offset);
    }
    return points;
}

void Level::beginPathPointDrag(std::size_t index)
{
    beginTransform();
    m_draggedPathPoint = index;
}

void Level::movePathPoint(QPointF pos)
{
    if (!m_transformOrigin.has_value() || !m_draggedPathPoint.has_value()) {
        return;
    }
    restoreContents(*m_transformOrigin);
    SpecialObject* m = singleSelectedSpecialObject();
    if (m == nullptr || *m_draggedPathPoint >= m->pathOffsets.size()) {
        return;
    }
    QPointF offset = pos - specialObjectCentre(*m);
    if (m_snapMode == SnapMode::AUTO || m_snapMode == SnapMode::GRID) {
        // Snap to whole grid squares, so the object lines up with the grid
        // as it did where it's drawn
        offset = QPointF(
            std::round(offset.x() / GRID_SPACING) * GRID_SPACING,
            std::round(offset.y() / GRID_SPACING) * GRID_SPACING);
    } else {
        offset = roundedOffset(offset);
    }
    if (offset == m->pathOffsets[*m_draggedPathPoint]) {
        return;
    }
    transformCheckpoint();
    m->pathOffsets[*m_draggedPathPoint] = offset;
}

void Level::addConnectedLinesToHighlight(const Line& line)
{
    for (std::size_t i = 0; i < m_lines.size(); ++i) {
        if (!m_lines[i].inactive) {
            if (!m_highlightedLineIndices.contains(i)) {
                if ((m_lines[i].x0 == line.x0 && m_lines[i].y0 == line.y0)
                    || (m_lines[i].x1 == line.x1 && m_lines[i].y1 == line.y1)
                    || (m_lines[i].x0 == line.x1 && m_lines[i].y0 == line.y1)
                    || (m_lines[i].x1 == line.x0 && m_lines[i].y1 == line.y0)) {
                    m_highlightedLineIndices.insert(i);
                    addConnectedLinesToHighlight(m_lines[i]);
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Editing individual lines
// ---------------------------------------------------------------------------

std::optional<std::size_t> Level::nearestLine(QPointF pos, float zoomLevel) const
{
    // As near as lineUnderCursor() requires
    const double maxDistance = 1 + 4 * zoomLevel;
    std::optional<std::size_t> nearest;
    double nearestDistance = maxDistance;
    for (std::size_t i = 0; i < m_lines.size(); ++i) {
        if (!m_lines[i].inactive) {
            const double d = distance(pos, nearestPointOnLine(pos, m_lines[i]));
            if (d <= nearestDistance) {
                nearest = i;
                nearestDistance = d;
            }
        }
    }
    return nearest;
}

std::vector<std::size_t> Level::linesWithEndAt(unsigned x, unsigned y) const
{
    std::vector<std::size_t> result;
    for (std::size_t i = 0; i < m_lines.size(); ++i) {
        const Line& l = m_lines[i];
        if (!l.inactive && ((l.x0 == x && l.y0 == y) || (l.x1 == x && l.y1 == y))) {
            result.push_back(i);
        }
    }
    return result;
}

std::optional<QLineF> Level::lineAt(QPointF pos, float zoomLevel) const
{
    const auto i = nearestLine(pos, zoomLevel);
    if (!i.has_value()) {
        return std::nullopt;
    }
    const Line& l = m_lines[*i];
    return QLineF(l.x0, l.y0, l.x1, l.y1);
}

std::optional<QPointF> Level::vertexAt(QPointF pos, float zoomLevel, bool selectedOnly) const
{
    std::optional<QPointF> nearest;
    double nearestDistance = VERTEX_HIT_PIXELS * zoomLevel;
    auto check = [&](QPointF p) {
        const double d = distance(pos, p);
        if (d <= nearestDistance) {
            nearest = p;
            nearestDistance = d;
        }
    };
    for (std::size_t i = 0; i < m_lines.size(); ++i) {
        const Line& l = m_lines[i];
        if (!l.inactive && (!selectedOnly || m_highlightedLineIndices.contains(i))) {
            check(QPointF(l.x0, l.y0));
            check(QPointF(l.x1, l.y1));
        }
    }
    return nearest;
}

std::vector<QPointF> Level::selectedLineVertices() const
{
    std::set<std::pair<unsigned, unsigned>> vertices;
    for (std::size_t i : m_highlightedLineIndices) {
        vertices.insert({ m_lines[i].x0, m_lines[i].y0 });
        vertices.insert({ m_lines[i].x1, m_lines[i].y1 });
    }
    std::vector<QPointF> result;
    for (const auto& [x, y] : vertices) {
        result.emplace_back(x, y);
    }
    return result;
}

void Level::deleteLineAt(QPointF pos, float zoomLevel)
{
    const auto i = nearestLine(pos, zoomLevel);
    if (!i.has_value()) {
        return;
    }
    checkpoint();
    m_lines[*i].inactive = true;
    m_highlightedLineIndices.erase(*i);
}

bool Level::canSplitLineAt(QPointF pos, float zoomLevel) const
{
    const auto i = nearestLine(pos, zoomLevel);
    if (!i.has_value()) {
        return false;
    }
    const Line& l = m_lines[*i];
    const QPointF p = nearestPointOnLine(pos, l);
    const unsigned x = static_cast<unsigned>(std::lround(p.x()));
    const unsigned y = static_cast<unsigned>(std::lround(p.y()));
    return !(x == l.x0 && y == l.y0) && !(x == l.x1 && y == l.y1);
}

void Level::splitLineAt(QPointF pos, float zoomLevel)
{
    if (!canSplitLineAt(pos, zoomLevel)) {
        return;
    }
    checkpoint();
    const std::size_t i = *nearestLine(pos, zoomLevel);
    const QPointF p = nearestPointOnLine(pos, m_lines[i]);
    const unsigned x = static_cast<unsigned>(std::lround(p.x()));
    const unsigned y = static_cast<unsigned>(std::lround(p.y()));
    // The new line is the second half, with the same properties
    Line second = m_lines[i];
    second.x0 = x;
    second.y0 = y;
    m_lines[i].x1 = x;
    m_lines[i].y1 = y;
    m_lines.push_back(second);
    if (m_highlightedLineIndices.contains(i)) {
        m_highlightedLineIndices.insert(m_lines.size() - 1);
    }
}

bool Level::canJoinLinesAt(QPointF pos, float zoomLevel) const
{
    const auto vertex = vertexAt(pos, zoomLevel, false);
    if (!vertex.has_value()) {
        return false;
    }
    const auto lines = linesWithEndAt(vertex->x(), vertex->y());
    if (lines.size() != 2) {
        return false;
    }
    const Line& a = m_lines[lines[0]];
    const Line& b = m_lines[lines[1]];
    // The far ends must differ, else joining would leave nothing
    auto farEnd = [&vertex](const Line& l) {
        return l.x0 == vertex->x() && l.y0 == vertex->y() ? std::pair(l.x1, l.y1)
                                                          : std::pair(l.x0, l.y0);
    };
    return a.breakable == b.breakable && a.breakableGroup == b.breakableGroup
        && a.decoration == b.decoration
        && farEnd(a) != farEnd(b);
}

void Level::joinLinesAt(QPointF pos, float zoomLevel)
{
    if (!canJoinLinesAt(pos, zoomLevel)) {
        return;
    }
    checkpoint();
    const QPointF vertex = *vertexAt(pos, zoomLevel, false);
    const auto lines = linesWithEndAt(vertex.x(), vertex.y());
    Line& a = m_lines[lines[0]];
    Line& b = m_lines[lines[1]];
    // Make a run from its far end to the vertex, and b from the vertex to its
    // far end, so a can then run from its far end to b's
    if (a.x0 == vertex.x() && a.y0 == vertex.y()) {
        std::swap(a.x0, a.x1);
        std::swap(a.y0, a.y1);
    }
    if (b.x1 == vertex.x() && b.y1 == vertex.y()) {
        std::swap(b.x0, b.x1);
        std::swap(b.y0, b.y1);
    }
    a.x1 = b.x1;
    a.y1 = b.y1;
    b.inactive = true;
    if (m_highlightedLineIndices.erase(lines[1]) > 0) {
        m_highlightedLineIndices.insert(lines[0]);
    }
}

void Level::selectLineAt(QPointF pos, float zoomLevel, bool includeConnectedLines)
{
    const auto i = nearestLine(pos, zoomLevel);
    if (!i.has_value()) {
        return;
    }
    if (m_currentMode != Mode::EDIT) {
        changeMode(Mode::EDIT);
    }
    clearSelection();
    select(Item { Item::Type::LINE, *i }, includeConnectedLines);
}

void Level::beginVertexDrag(QPointF vertex)
{
    beginTransform();
    m_draggedVertex = { static_cast<unsigned>(std::lround(vertex.x())),
                        static_cast<unsigned>(std::lround(vertex.y())) };
}

void Level::moveVertex(QPointF pos)
{
    if (!m_transformOrigin.has_value() || !m_draggedVertex.has_value()) {
        return;
    }
    restoreContents(*m_transformOrigin);
    // Snap as when drawing, but not to the lines being moved
    updateSnapPoint(pos, m_draggedVertex);
    const auto [fromX, fromY] = *m_draggedVertex;
    unsigned toX;
    unsigned toY;
    if (m_currentNearestSnapPoint.has_value()) {
        std::tie(toX, toY) = *m_currentNearestSnapPoint;
    } else {
        toX = static_cast<unsigned>(std::lround(std::clamp(pos.x(), 0.0, double(MAP_SIZE))));
        toY = static_cast<unsigned>(std::lround(std::clamp(pos.y(), 0.0, double(MAP_SIZE))));
    }
    if (toX == fromX && toY == fromY) {
        return;
    }
    transformCheckpoint();
    for (std::size_t i : linesWithEndAt(fromX, fromY)) {
        Line& l = m_lines[i];
        if (l.x0 == fromX && l.y0 == fromY) {
            l.x0 = toX;
            l.y0 = toY;
        }
        if (l.x1 == fromX && l.y1 == fromY) {
            l.x1 = toX;
            l.y1 = toY;
        }
    }
}

// ---------------------------------------------------------------------------
// Undo / redo
// ---------------------------------------------------------------------------

Level::Snapshot Level::snapshot() const
{
    return { m_stateId,         m_levelDescription, m_lines,         m_startPosition,
             m_exitPosition,    m_fuelObjects,      m_specialObjects, m_currentSpecialObject };
}

void Level::restore(const Snapshot& snapshot)
{
    m_stateId = snapshot.stateId;
    restoreContents(snapshot);
    // A special object can only be in progress in SPECIAL mode
    if (m_currentMode != Mode::SPECIAL && !m_currentSpecialObject.lines.empty()) {
        m_specialObjects.push_back(m_currentSpecialObject);
        m_currentSpecialObject = {};
    }
    // The selection and anything in progress may refer to things which no
    // longer exist
    clearSelection();
    m_transformOrigin = std::nullopt;
    m_draggedVertex = std::nullopt;
    m_draggedPathPoint = std::nullopt;
    m_currentInsertionLine.inactive = true;
    m_lastChangeWasNudge = false;
}

void Level::restoreContents(const Snapshot& snapshot)
{
    m_levelDescription = snapshot.description;
    m_lines = snapshot.lines;
    m_startPosition = snapshot.startPosition;
    m_exitPosition = snapshot.exitPosition;
    m_fuelObjects = snapshot.fuelObjects;
    m_specialObjects = snapshot.specialObjects;
    m_currentSpecialObject = snapshot.currentSpecialObject;
}

void Level::checkpoint(Change change)
{
    const bool merge = change == Change::Nudge && m_lastChangeWasNudge && !m_undoStack.empty();
    if (!merge) {
        m_undoStack.push_back(snapshot());
        if (m_undoStack.size() > MAX_UNDO) {
            m_undoStack.erase(m_undoStack.begin());
        }
    }
    m_redoStack.clear();
    m_stateId = m_nextStateId++;
    m_lastChangeWasNudge = change == Change::Nudge;
}

bool Level::canUndo() const
{
    return !m_undoStack.empty();
}

bool Level::canRedo() const
{
    return !m_redoStack.empty();
}

void Level::undo()
{
    if (m_undoStack.empty()) {
        return;
    }
    m_redoStack.push_back(snapshot());
    restore(m_undoStack.back());
    m_undoStack.pop_back();
}

void Level::redo()
{
    if (m_redoStack.empty()) {
        return;
    }
    m_undoStack.push_back(snapshot());
    restore(m_redoStack.back());
    m_redoStack.pop_back();
}

// ---------------------------------------------------------------------------
// Modes
// ---------------------------------------------------------------------------

void Level::finishCurrentSpecialObject()
{
    if (m_currentMode == Mode::SPECIAL) {
        // Write any existing  special object and start a new one
        if (!m_currentSpecialObject.lines.empty()) {
            m_specialObjects.push_back(m_currentSpecialObject);
        }
        m_currentSpecialObject = {};
    }
}

void Level::changeMode(Mode mode)
{
    if (mode != Mode::SPECIAL) {
        finishCurrentSpecialObject();
    }
    if (mode != Mode::POLYGON_RADIUS) {
        m_currentPolygon.centreX = std::nullopt;
        m_currentPolygon.centreY = std::nullopt;
        m_currentPolygon.lines.clear();
    }
    if (mode != Mode::EDIT) {
        clearSelection();
    }
    m_currentInsertionLine.inactive = true;
    m_currentNearestSnapPoint = std::nullopt;
    m_currentMode = mode;
    if (m_currentMode == Mode::LINE) {
        m_currentInsertionLine.r = 255;
        m_currentInsertionLine.g = 0;
        m_currentInsertionLine.b = 0;
    } else if (m_currentMode == Mode::BREAKABLE) {
        m_currentInsertionLine.r = 255;
        m_currentInsertionLine.g = 150;
        m_currentInsertionLine.b = 50;
    } else if (m_currentMode == Mode::DECORATION) {
        m_currentInsertionLine.r = DECORATION_COLOUR.red();
        m_currentInsertionLine.g = DECORATION_COLOUR.green();
        m_currentInsertionLine.b = DECORATION_COLOUR.blue();
    } else if (m_currentMode == Mode::SPECIAL) {
        m_currentInsertionLine.r = 255;
        m_currentInsertionLine.g = 172;
        m_currentInsertionLine.b = 163;
    }
}

void Level::setSnapMode(SnapMode snapMode)
{
    m_snapMode = snapMode;
    m_currentNearestSnapPoint = std::nullopt;
}

void Level::cycleSnapMode()
{
    switch (m_snapMode) {
        case SnapMode::AUTO:
            setSnapMode(SnapMode::LINE);
            break;
        case SnapMode::LINE:
            setSnapMode(SnapMode::GRID);
            break;
        case SnapMode::GRID:
            setSnapMode(SnapMode::NONE);
            break;
        case SnapMode::NONE:
            setSnapMode(SnapMode::AUTO);
            break;
    }
}

} // namespace mgo
