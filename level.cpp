#include "level.h"
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

} // namespace

void Level::load(const std::string& filename)
{
    // Note, this is likely to break if the format of the file is incorrect, there's
    // no checking for size of vectors below currently. Which is reasonable as this
    // program should be creating the files being imported.
    m_fileName = filename;
    if (!std::filesystem::exists(filename)) {
        // file doesn't exist, so we save the name and will write to it when we save
        return;
    }

    std::ifstream in(filename);
    if (!in) {
        throw(std::runtime_error("Failed to load Level file " + filename));
    }
    std::string currentLine;
    enum class ObjectType {
        OBSTRUCTION,
        EXIT,
        FUEL,
        BREAKABLE,
        MOVING
    };
    ObjectType currentObject = ObjectType::OBSTRUCTION;
    while (!in.eof()) {
        getline(in, currentLine);
        const std::vector<std::string> vec = utils::split(currentLine, '~');
        if (vec.size() == 0) {
            continue;
        }
        // When we get here, vec is a vector of all the items on the current line.
        const char c = vec[0][0];
        switch (c) {
            case '!': // timelimit (unused), fuel (unused) , ship x, ship y, angle, description
                if (vec.size() < 7) {
                    throw std::runtime_error("Invalid first line of level file");
                }
                m_startPosition = { static_cast<unsigned>(stoi(vec[3])),
                                    static_cast<unsigned>(stoi(vec[4])),
                                    static_cast<unsigned>(stoi(vec[5])) };
                m_levelDescription = vec[6];
                break;
            case 'N': // New object, parameter 1 is type, parameter 2 appears unused
                if (vec[1] == "OBSTRUCTION") {
                    currentObject = ObjectType::OBSTRUCTION;
                } else if (vec[1] == "EXIT") {
                    currentObject = ObjectType::EXIT;
                } else if (vec[1] == "FUEL") {
                    currentObject = ObjectType::FUEL;
                } else if (vec[1] == "BREAKABLE") {
                    currentObject = ObjectType::BREAKABLE;
                } else if (vec[1] == "MOVING") {
                    currentObject = ObjectType::MOVING;
                    if (m_currentMovingObject.lines.size() > 0) {
                        m_movingObjects.push_back(m_currentMovingObject);
                        m_currentMovingObject.lines.clear();
                    }
                    m_currentMovingObject.xDelta = std::stof(vec[3]);
                    m_currentMovingObject.xMaxDifference = std::stof(vec[4]);
                    m_currentMovingObject.yDelta = std::stof(vec[5]);
                    m_currentMovingObject.yMaxDifference = std::stof(vec[6]);
                    m_currentMovingObject.rotationDelta = std::stof(vec[7]);
                    if (vec.size() > 8) {
                        m_currentMovingObject.gravity = std::stof(vec[8]);
                    }
                } else {
                    Log::warn("Unrecognised object type '{}' in file", vec[1]);
                }
                break;
            case 'L':
                {
                    const unsigned x0 = std::stoi(vec[1]);
                    const unsigned y0 = std::stoi(vec[2]);
                    const unsigned x1 = std::stoi(vec[3]);
                    const unsigned y1 = std::stoi(vec[4]);
                    const uint8_t r = std::stoi(vec[5]);
                    const uint8_t g = std::stoi(vec[6]);
                    const uint8_t b = std::stoi(vec[7]);
                    if (currentObject == ObjectType::OBSTRUCTION) {
                        m_lines.push_back({ x0, y0, x1, y1, r, g, b, 1, false, false });
                    } else if (currentObject == ObjectType::BREAKABLE) {
                        m_lines.push_back({ x0, y0, x1, y1, r, g, b, 1, false, true });
                    } else if (currentObject == ObjectType::MOVING) {
                        m_currentMovingObject.lines.push_back(
                            { x0, y0, x1, y1, r, g, b, 1, false });
                    }
                    break;
                }
            case 'P': // position
                if (currentObject == ObjectType::EXIT) {
                    m_exitPosition = std::make_pair(stoi(vec[1]), stoi(vec[2]));
                }
                if (currentObject == ObjectType::FUEL) {
                    m_fuelObjects.push_back(std::make_pair(stoi(vec[1]), stoi(vec[2])));
                }
                break;
            case 'T': // text
                break;
            default:
                break;
        }
        currentLine.clear();
    }
    if (m_currentMovingObject.lines.size() > 0) {
        m_movingObjects.push_back(m_currentMovingObject);
        m_currentMovingObject.lines.clear();
    }
    in.close();
}

void Level::save(const std::string& filename)
{
    std::ofstream outfile(filename, std::ios::trunc);
    if (!outfile) {
        throw std::runtime_error("Could not open " + filename + " for writing");
    }
    // Header
    // time limit, fuel, startX, startY, angle, title
    unsigned startX = 0;
    unsigned startY = 0;
    unsigned rotation = 0;
    if (m_startPosition.has_value()) {
        startX = m_startPosition.value().x;
        startY = m_startPosition.value().y;
        rotation = m_startPosition.value().r;
    }
    outfile << "!~0~0~" << startX << "~" << startY << "~" << rotation << "~"
            << m_levelDescription << "\n";
    outfile << "N~OBSTRUCTION~obstruction\n";
    for (const auto& l : m_lines) {
        if (!l.inactive && !l.breakable) {
            outfile << "L~" << l.x0 << "~" << l.y0 << "~" << l.x1 << "~" << l.y1
                    << "~255~0~0~2\n";
        }
    }
    // Each breakable line is its own object
    for (const auto& l : m_lines) {
        if (!l.inactive && l.breakable) {
            outfile << "N~BREAKABLE~breakable\n";
            outfile << "L~" << l.x0 << "~" << l.y0 << "~" << l.x1 << "~" << l.y1
                    << "~255~150~50~6\n";
        }
    }
    if (m_exitPosition.has_value()) {
        outfile << "N~EXIT~exit\n"
                   "T~EXIT~52~213~235~6\n"
                   "P~"
                << m_exitPosition.value().first << "~" << m_exitPosition.value().second << "\n";
    }
    for (const auto& p : m_fuelObjects) {
        outfile << "N~FUEL~fuel\n"
                   "T~*~255~255~0~12\n"
                   "P~"
                << p.first << "~" << p.second << "\n";
    }
    auto writeMovingObject = [&outfile](const MovingObject& m, std::size_t counter) {
        outfile << "N~MOVING~moving_" << counter;
        outfile << "~" << m.xDelta << "~" << m.xMaxDifference << "~" << m.yDelta << "~"
                << m.yMaxDifference << "~" << m.rotationDelta << "~" << m.gravity << "\n";
        for (const auto& l : m.lines) {
            outfile << "L~" << l.x0 << "~" << l.y0 << "~" << l.x1 << "~" << l.y1 << "~"
                    << static_cast<int>(l.r) << "~" << static_cast<int>(l.g) << "~"
                    << static_cast<int>(l.b) << "~6\n";
        }
    };
    std::size_t counter = 0;
    for (const auto& m : m_movingObjects) {
        writeMovingObject(m, counter);
        ++counter;
    }
    // Is there a moving object in progress?
    if (m_currentMovingObject.lines.size() > 0) {
        writeMovingObject(m_currentMovingObject, counter);
    }
    outfile.close();
    if (!outfile) {
        throw std::runtime_error("Failed writing to " + filename);
    }
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
    for (const auto& m : m_movingObjects) {
        drawMovingObjectBoundary(painter, m, idx);
        ++idx;
    }
    for (const auto& l : m_currentMovingObject.lines) {
        drawLine(painter, l);
    }
    for (const auto& l : m_currentPolygon.lines) {
        drawLine(painter, l);
    }
}

void Level::drawMovingObjectBoundary(QPainter& painter, const MovingObject& m, size_t idx) const
{
    unsigned minX { std::numeric_limits<unsigned>::max() };
    unsigned minY { std::numeric_limits<unsigned>::max() };
    unsigned maxX { 0 };
    unsigned maxY { 0 };
    for (auto l : m.lines) {
        if (m_highlightedMovingObjectIdx.has_value()
            && m_highlightedMovingObjectIdx.value() == idx) {
            l.r = 255;
            l.g = 255;
            l.b = 255;
        }
        drawLine(painter, l);
        // Determine bounding box:
        if (l.x0 < minX) {
            minX = l.x0 - 1;
        }
        if (l.x1 < minX) {
            minX = l.x1 - 1;
        }
        if (l.y0 < minY) {
            minY = l.y0 - 1;
        }
        if (l.y1 < minY) {
            minY = l.y1 - 1;
        }
        if (l.x0 > maxX) {
            maxX = l.x0 + 1;
        }
        if (l.x1 > maxX) {
            maxX = l.x1 + 1;
        }
        if (l.y0 > maxY) {
            maxY = l.y0 + 1;
        }
        if (l.y1 > maxY) {
            maxY = l.y1 + 1;
        }
    }
    if (m.xDelta != 0.f) {
        minX -= m.xMaxDifference;
        maxX += m.xMaxDifference;
    }
    if (m.yDelta != 0.f) {
        minY -= m.yMaxDifference;
        maxY += m.yMaxDifference;
    }

    if (m.rotationDelta == 0.f) {
        painter.setPen(hairline(BOUNDARY_COLOUR));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(QRectF(QPointF(minX, minY), QPointF(maxX, maxY)));
    } else {
        // It's rotating, so we calculate the max radius by finding the vertex furthest from the
        // centre
        float centreX = minX + (maxX - minX) / 2.f;
        float centreY = minY + (maxY - minY) / 2.f;
        float maxRadius { 0.f };
        for (const auto& l : m.lines) {
            auto r = std::sqrt(
                ((l.x0 - centreX) * (l.x0 - centreX)) + ((l.y0 - centreY) * (l.y0 - centreY)));
            if (r > maxRadius) {
                maxRadius = r;
            }
            r = std::sqrt(
                ((l.x1 - centreX) * (l.x1 - centreX)) + ((l.y1 - centreY) * (l.y1 - centreY)));
            if (r > maxRadius) {
                maxRadius = r;
            }
        }
        if (m.xMaxDifference > 0.f || m.yMaxDifference > 0.f) {
            // Circumscribe all possible positions
            drawRoundedRect(
                painter,
                centreX - m.xMaxDifference - maxRadius, // top left
                centreY - m.yMaxDifference - maxRadius,
                2 * (m.xMaxDifference + maxRadius),
                2 * (m.yMaxDifference + maxRadius),
                maxRadius);
        } else {
            // It's just rotating, so we can simply draw a circle around it to show all possible
            // positions
            drawCircle(painter, maxRadius, centreX, centreY);
        }
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
        QPen pen = hairline(SHIP_COLOUR);
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
        painter.setPen(QColor(52, 213, 235));
        const QPointF centre(m_exitPosition.value().first, m_exitPosition.value().second);
        painter.drawText(QRectF(centre - QPointF(100, 50), QSizeF(200, 100)), Qt::AlignCenter, "EXIT");
    }
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::yellow);
    for (const auto& p : m_fuelObjects) {
        painter.drawEllipse(QPointF(p.first, p.second), 10.0, 10.0);
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

std::optional<std::size_t> Level::movingObjectUnderCursor(QPointF w) const
{
    // Note, this returns the index of the entire moving object, not the individual line
    const long x = w.x();
    const long y = w.y();
    std::size_t idx = 0;
    for (const auto& m : m_movingObjects) {
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

void Level::highlightNearestLinePoint(QPointF w)
{
    auto check = [&](const Line& l) {
        if (l.inactive) {
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
    for (const auto& m : m_movingObjects) {
        for (const auto& l : m.lines) {
            if (check(l)) {
                return;
            }
        }
    }
    // Also check moving object in progress
    for (const auto& l : m_currentMovingObject.lines) {
        if (check(l)) {
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

void Level::mouseMoved(QPointF w)
{
    if (m_currentMode == Mode::LINE || m_currentMode == Mode::BREAKABLE
        || m_currentMode == Mode::MOVING) {
        if (m_snapMode == SnapMode::AUTO || m_snapMode == SnapMode::GRID) {
            highlightGridVertex(w);
        }
        if (m_snapMode == SnapMode::AUTO || m_snapMode == SnapMode::LINE) {
            highlightNearestLinePoint(w);
        }
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
        case Mode::MOVING:
            {
                // Insert a new line
                if (m_currentNearestSnapPoint.has_value() || m_snapMode == SnapMode::NONE) {
                    // Are we in the middle of drawing a line? If so, add the current
                    // insertion line into the vector
                    if (!m_currentInsertionLine.inactive) {
                        if (m_currentInsertionLine.x0 != m_currentInsertionLine.x1
                            || m_currentInsertionLine.y0 != m_currentInsertionLine.y1) {
                            checkpoint();
                            m_currentInsertionLine.breakable = m_currentMode != Mode::LINE;
                            if (m_currentMode == Mode::MOVING) {
                                Line l = m_currentInsertionLine;
                                l.r = 255;
                                l.g = 172;
                                l.b = 163;
                                m_currentMovingObject.lines.push_back(l);
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
                if (!extendSelection) {
                    m_highlightedLineIndices.clear();
                }
                // Check to see if there is a line under the cursor
                auto line = lineUnderCursor(w, zoomLevel);
                addOrRemoveHighlightedLine(line, !singleLine);
                if (line.has_value()) {
                    m_highlightedMovingObjectIdx = std::nullopt;
                } else {
                    m_highlightedLineIndices.clear();
                    m_highlightedMovingObjectIdx = movingObjectUnderCursor(w);
                }
                break;
            }
        case Mode::START:
            {
                // if we're within r workspace units of an existing start object,
                // we treat additional clicks as a way to modify the object's rotation
                unsigned r = 20;
                checkpoint();
                if (m_startPosition.has_value()
                    && ((m_startPosition.value().x > w.x() - r
                         && m_startPosition.value().x < w.x() + r)
                        && (m_startPosition.value().y > w.y() - r
                            && m_startPosition.value().y < w.y() + r))) {
                    m_startPosition.value().r += 15;
                    if (m_startPosition.value().r >= 360) {
                        m_startPosition.value().r = 0;
                    }
                } else {
                    m_startPosition
                        = { static_cast<unsigned>(w.x()), static_cast<unsigned>(w.y()), 0 };
                }
                break;
            }
        case Mode::EXIT:
            {
                checkpoint();
                m_exitPosition = std::make_pair(w.x(), w.y());
                break;
            }
        case Mode::FUEL:
            {
                // if we are within r workspace units of an existing fuel object, we
                // treat this as a request to delete it instead of placing a new one
                unsigned r = 20;
                checkpoint();
                std::size_t idx = 0;
                bool erased { false };
                for (const auto& f : m_fuelObjects) {
                    if ((f.first > w.x() - r && f.first < w.x() + r)
                        && (f.second > w.y() - r && f.second < w.y() + r)) {
                        m_fuelObjects.erase(m_fuelObjects.begin() + idx);
                        erased = true;
                        break;
                    }
                    ++idx;
                }
                if (!erased) {
                    m_fuelObjects.push_back(std::make_pair(w.x(), w.y()));
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
    finishCurrentMovingObject();
}

// ---------------------------------------------------------------------------
// Selection
// ---------------------------------------------------------------------------

bool Level::hasSelection() const
{
    return hasSelectedLines() || m_highlightedMovingObjectIdx.has_value();
}

bool Level::hasSelectedLines() const
{
    return !m_highlightedLineIndices.empty();
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
    m_highlightedLineIndices.clear();
    if (m_highlightedMovingObjectIdx.has_value()) {
        m_movingObjects.erase(m_movingObjects.begin() + m_highlightedMovingObjectIdx.value());
        m_highlightedMovingObjectIdx = std::nullopt;
    }
}

void Level::nudgeSelection(int x, int y)
{
    if (!hasSelection()) {
        return;
    }
    // Consecutive nudges undo together
    checkpoint(Change::Nudge);
    if (m_highlightedMovingObjectIdx.has_value()) {
        moveMovingObject(*m_highlightedMovingObjectIdx, x, y);
    } else {
        moveLines(x, y);
    }
}

void Level::convertSelectionToMovingObject()
{
    if (m_highlightedLineIndices.empty()) {
        return;
    }
    checkpoint();
    MovingObject m;
    for (std::size_t i : m_highlightedLineIndices) {
        m_lines[i].r = 255;
        m_lines[i].g = 172;
        m_lines[i].b = 163;
        m.lines.push_back(m_lines[i]);
        m_lines[i].inactive = true;
    }
    m_movingObjects.push_back(m);
    m_highlightedLineIndices.clear();
    m_highlightedMovingObjectIdx = m_movingObjects.size() - 1;
}

std::optional<MovingObject> Level::selectedMovingObject() const
{
    if (!m_highlightedMovingObjectIdx.has_value()) {
        return std::nullopt;
    }
    return m_movingObjects[*m_highlightedMovingObjectIdx];
}

void Level::setSelectedMovingObjectMotion(const MovingObject& motion)
{
    if (!m_highlightedMovingObjectIdx.has_value()) {
        return;
    }
    checkpoint();
    auto& obj = m_movingObjects[*m_highlightedMovingObjectIdx];
    obj.xDelta = motion.xDelta;
    obj.xMaxDifference = motion.xMaxDifference;
    obj.yDelta = motion.yDelta;
    obj.yMaxDifference = motion.yMaxDifference;
    obj.rotationDelta = motion.rotationDelta;
    obj.gravity = motion.gravity;
}

void Level::addOrRemoveHighlightedLine(std::optional<size_t>& lineIdx, bool includeConnectedLines)
{
    if (!lineIdx.has_value()) {
        return;
    }
    auto i = std::find(m_highlightedLineIndices.begin(), m_highlightedLineIndices.end(), *lineIdx);
    if (i == m_highlightedLineIndices.end()) {
        if (includeConnectedLines) {
            addConnectedLinesToHighlight(m_lines[*lineIdx]);
        } else {
            m_highlightedLineIndices.insert(*lineIdx);
        }
    } else {
        m_highlightedLineIndices.erase(i);
        // TODO remove connected lines if includeConnectedLines == true
    }
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

void Level::moveMovingObject(std::size_t movingObjectIdx, int x, int y)
{
    auto& obj = m_movingObjects[movingObjectIdx];
    for (auto& line : obj.lines) {
        line.x0 += x;
        line.y0 += y;
        line.x1 += x;
        line.y1 += y;
    }
}

void Level::moveLines(int x, int y)
{
    for (auto& idx : m_highlightedLineIndices) {
        m_lines[idx].x0 += x;
        m_lines[idx].y0 += y;
        m_lines[idx].x1 += x;
        m_lines[idx].y1 += y;
    }
}

// ---------------------------------------------------------------------------
// Undo / redo
// ---------------------------------------------------------------------------

Level::Snapshot Level::snapshot() const
{
    return { m_stateId,         m_levelDescription, m_lines,         m_startPosition,
             m_exitPosition,    m_fuelObjects,      m_movingObjects, m_currentMovingObject };
}

void Level::restore(const Snapshot& snapshot)
{
    m_stateId = snapshot.stateId;
    m_levelDescription = snapshot.description;
    m_lines = snapshot.lines;
    m_startPosition = snapshot.startPosition;
    m_exitPosition = snapshot.exitPosition;
    m_fuelObjects = snapshot.fuelObjects;
    m_movingObjects = snapshot.movingObjects;
    m_currentMovingObject = snapshot.currentMovingObject;
    // A moving object can only be in progress in MOVING mode
    if (m_currentMode != Mode::MOVING && !m_currentMovingObject.lines.empty()) {
        m_movingObjects.push_back(m_currentMovingObject);
        m_currentMovingObject = {};
    }
    // The selection and anything in progress may refer to things which no
    // longer exist
    m_highlightedLineIndices.clear();
    m_highlightedMovingObjectIdx = std::nullopt;
    m_currentInsertionLine.inactive = true;
    m_lastChangeWasNudge = false;
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

void Level::finishCurrentMovingObject()
{
    if (m_currentMode == Mode::MOVING) {
        // Write any existing  moving object and start a new one
        if (!m_currentMovingObject.lines.empty()) {
            m_movingObjects.push_back(m_currentMovingObject);
        }
        m_currentMovingObject = {};
    }
}

void Level::changeMode(Mode mode)
{
    if (mode != Mode::MOVING) {
        finishCurrentMovingObject();
    }
    if (mode != Mode::POLYGON_RADIUS) {
        m_currentPolygon.centreX = std::nullopt;
        m_currentPolygon.centreY = std::nullopt;
        m_currentPolygon.lines.clear();
    }
    if (mode != Mode::EDIT) {
        m_highlightedLineIndices.clear();
        m_highlightedMovingObjectIdx = std::nullopt;
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
    } else if (m_currentMode == Mode::MOVING) {
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
