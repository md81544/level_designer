#include "level.h"
#include "dialog.h"
#include "utils.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <ios>
#include <iostream>
#include <stdexcept>
#include <string>
#include <tuple>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

namespace mgo {
mgo::Level::Level(sf::Window& window, unsigned windowWidth, unsigned windowHeight)
    : m_window(window)
    , m_dialogTitle(m_font)
    , m_dialogText(m_font)
    , m_editModeText(m_font)
{
    if (!m_font.openFromFile("DroidSansMono.ttf")) {
        throw std::runtime_error("Could not load font file");
    }
    m_currentInsertionLine.inactive = true;
    m_currentInsertionLine.r = 255;
    m_currentInsertionLine.g = 0;
    m_currentInsertionLine.b = 0;
    m_view = sf::View(
        sf::FloatRect(
            { 0.f, 0.f }, { static_cast<float>(windowWidth), static_cast<float>(windowHeight) }));
    m_view.setViewport(sf::FloatRect({ 0.f, 0.f }, { 1.f, 1.f }));
    m_fixedView = sf::View(
        sf::FloatRect(
            { 0.f, 0.f }, { static_cast<float>(windowWidth), static_cast<float>(windowHeight) }));
    m_fixedView.setViewport(sf::FloatRect({ 0.f, 0.f }, { 1.f, 1.f }));
}

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
        std::vector<std::string> vec;
        boost::split(vec, currentLine, boost::is_any_of("~"));
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
                m_window.setTitle(m_fileName + " - " + m_levelDescription);
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
                    std::cout << "Unrecognised object type '" << vec[1] << "' in file\n";
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

void mgo::Level::save()
{
    msgbox("Save File", "Saving to: " + m_fileName, [&](bool okPressed, const std::string&) {
        if (okPressed) {
            std::ofstream outfile(m_fileName, std::ios::trunc);
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
                        << m_exitPosition.value().first << "~" << m_exitPosition.value().second
                        << "\n";
            }
            for (const auto& p : m_fuelObjects) {
                outfile << "N~FUEL~fuel\n"
                           "T~*~255~255~0~12\n"
                           "P~"
                        << p.first << "~" << p.second << "\n";
            }
            std::size_t counter = 0;
            for (const auto& m : m_movingObjects) {
                outfile << "N~MOVING~moving_" << counter;
                outfile << "~" << m.xDelta << "~" << m.xMaxDifference << "~" << m.yDelta << "~"
                        << m.yMaxDifference << "~" << m.rotationDelta << "~" << m.gravity << "\n";
                for (const auto& l : m.lines) {
                    outfile << "L~" << l.x0 << "~" << l.y0 << "~" << l.x1 << "~" << l.y1 << "~"
                            << static_cast<int>(l.r) << "~" << static_cast<int>(l.g) << "~"
                            << static_cast<int>(l.b) << "~6\n";
                }
                ++counter;
            }
            // Is there a moving object in progress?
            if (m_currentMovingObject.lines.size() > 0) {
                outfile << "N~MOVING~moving_" << counter;
                outfile << "~" << m_currentMovingObject.xDelta << "~"
                        << m_currentMovingObject.xMaxDifference << "~"
                        << m_currentMovingObject.yDelta << "~"
                        << m_currentMovingObject.yMaxDifference << "~"
                        << m_currentMovingObject.rotationDelta << "~"
                        << m_currentMovingObject.gravity << "\n";
                for (const auto& l : m_currentMovingObject.lines) {
                    outfile << "L~" << l.x0 << "~" << l.y0 << "~" << l.x1 << "~" << l.y1 << "~"
                            << static_cast<int>(l.r) << "~" << static_cast<int>(l.g) << "~"
                            << static_cast<int>(l.b) << "~6\n";
                }
            }
            outfile.close();
            m_dirty = false;
        }
    });
}

void mgo::Level::draw(sf::RenderWindow& window)
{
    std::size_t idx = 0;
    for (const auto& l : m_lines) {
        if (!l.inactive) {
            drawLine(window, l, idx);
        }
        ++idx;
    }
    if (m_currentNearestSnapPoint.has_value()) {
        sf::CircleShape c;
        c.setFillColor(sf::Color::Magenta);
        c.setRadius(3.f);
        c.setOrigin({ 3.f, 3.f });
        float x = std::get<0>(m_currentNearestSnapPoint.value());
        float y = std::get<1>(m_currentNearestSnapPoint.value());
        c.setPosition({ x, y });
        window.draw(c);
    }
    if (m_currentInsertionLine.inactive == false) {
        drawLine(window, m_currentInsertionLine, std::nullopt);
    }
    idx = 0;
    for (const auto& m : m_movingObjects) {
        drawMovingObjectBoundary(m, idx, window);
        ++idx;
    }
    for (const auto& l : m_currentMovingObject.lines) {
        drawLine(window, l, std::nullopt);
    }
    for (const auto& l : m_currentPolygon.lines) {
        drawLine(window, l, std::nullopt);
    }
}

void Level::drawMovingObjectBoundary(
    const mgo::MovingObject& m,
    size_t idx,
    sf::RenderWindow& window)
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
        drawLine(window, l, std::nullopt);
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
        Line l1 { minX, minY, minX, maxY, 128, 128, 0 };
        drawLine(window, l1, std::nullopt);
        Line l2 { minX, maxY, maxX, maxY, 128, 128, 0 };
        drawLine(window, l2, std::nullopt);
        Line l3 { maxX, maxY, maxX, minY, 128, 128, 0 };
        drawLine(window, l3, std::nullopt);
        Line l4 { minX, minY, maxX, minY, 128, 128, 0 };
        drawLine(window, l4, std::nullopt);
    } else {
        // It's rotating, so we calculate the max radius by finding the vertex furthest from the
        // centre
        float centreX = minX + (maxX - minX) / 2;
        float centreY = minY + (maxY - minY) / 2;
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
                window,
                centreX - m.xMaxDifference - maxRadius, // top left
                centreY - m.yMaxDifference - maxRadius,
                (centreX + m.xMaxDifference + maxRadius) - (centreX - m.xMaxDifference - maxRadius),
                (centreY + m.yMaxDifference + maxRadius) - (centreY - m.yMaxDifference - maxRadius),
                maxRadius,
                128,
                128,
                0);
        } else {
            // It's just rotating, so we can simply draw a circle around it to show all possible
            // positions
            drawCircle(maxRadius, centreX, centreY, window);
        }
    }
}

void Level::drawCircle(float maxRadius, float centreX, float centreY, sf::RenderWindow& window)
{
    sf::CircleShape circle;
    circle.setRadius(maxRadius);
    circle.setOutlineColor({ 128, 128, 0 });
    circle.setOutlineThickness(1);
    circle.setFillColor(sf::Color::Transparent);
    circle.setPosition({ centreX - maxRadius, centreY - maxRadius });
    window.draw(circle);
}

void Level::drawRoundedRect(
    sf::RenderWindow& window,
    float x,
    float y,
    float w,
    float h,
    float r,
    uint8_t red,
    uint8_t green,
    uint8_t blue)
{
    // Ensure radius isn't too big for size:
    r = std::min(r, std::min(w, h) / 2);
    auto addLine = [&](unsigned x0, unsigned y0, unsigned x1, unsigned y1) {
        Line line = { x0, y0, x1, y1, red, green, blue };
        drawLine(window, line);
    };
    constexpr unsigned segments = 6; // Number of segments to approximate quarter circles
    addLine(x + r, y, x + w - r, y);
    addLine(x + r, y + h, x + w - r, y + h);
    addLine(x, y + r, x, y + h - r);
    addLine(x + w, y + r, x + w, y + h - r);
    auto drawArc = [&](float cx, float cy, float startAngle, float endAngle) {
        float angleStep = (endAngle - startAngle) / segments;
        for (unsigned i = 0; i < segments; ++i) {
            float a0 = startAngle + i * angleStep;
            float a1 = a0 + angleStep;
            unsigned x0 = static_cast<unsigned>(cx + r * std::cos(a0));
            unsigned y0 = static_cast<unsigned>(cy + r * std::sin(a0));
            unsigned x1 = static_cast<unsigned>(cx + r * std::cos(a1));
            unsigned y1 = static_cast<unsigned>(cy + r * std::sin(a1));
            addLine(x0, y0, x1, y1);
        }
    };
    constexpr float PI = 3.14159265f;
    drawArc(x + r, y + r, PI, 1.5f * PI);
    drawArc(x + w - r, y + r, 1.5f * PI, 2.0f * PI);
    drawArc(x + w - r, y + h - r, 0.0f, 0.5f * PI);
    drawArc(x + r, y + h - r, 0.5f * PI, PI);
}

void mgo::Level::drawDialog(sf::RenderWindow& window)
{
    if (!m_isDialogActive) {
        return;
    }
    window.draw(m_dialog);
    window.draw(m_dialogTitle);
    window.draw(m_dialogText);
}

void mgo::Level::drawLine(sf::RenderWindow& window, const Line& l, std::optional<std::size_t> idx)
{
    sf::Vertex line[]
        = { sf::Vertex(sf::Vector2f(l.x0, l.y0)), sf::Vertex(sf::Vector2f(l.x1, l.y1)) };
    bool highlighted = false;
    if (idx.has_value()) {
        for (const size_t i : m_highlightedLineIndices) {
            if (i == *idx) {
                highlighted = true;
                break;
            }
        }
    }
    if (highlighted) {
        line[0].color = sf::Color(sf::Color::White);
        line[1].color = sf::Color(sf::Color::White);
    } else {
        line[0].color = sf::Color(l.r, l.g, l.b);
        line[1].color = sf::Color(l.r, l.g, l.b);
    }
    window.draw(line, 2, sf::PrimitiveType::Lines);
}

void mgo::Level::drawGridLines(sf::RenderWindow& window)
{
    for (unsigned n = 0; n <= 2000; n += 50) {
        drawLine(window, { n, 0, n, 2000, 0, 100, 0, 1 }, std::nullopt);
        drawLine(window, { 0, n, 2000, n, 0, 100, 0, 1 }, std::nullopt);
    }
}

std::optional<std::size_t>
Level::lineUnderCursor(sf::RenderWindow& window, unsigned mouseX, unsigned mouseY)
{
    // Note mouseX and mouseY are *window* coordinates, these need to be converted into
    // workspace coordinates.
    // We define four lines which bound the cursor in a diamond shape, and then check each
    // of these to see if they intersect any line on the workspace.
    // A positive origin e.g. 10,10 means the top left of the window is at, say, 10,10 on
    // the workspace, i.e. the workspace is slightly off screen to the left.
    const auto w = window.mapPixelToCoords({ static_cast<int>(mouseX), static_cast<int>(mouseY) });
    std::size_t idx = 0;
    const float selectionBoxSize = 1 + 4 * m_viewZoomLevel;
    for (const auto& l : m_lines) {
        if (!l.inactive) {
            if (utils::doLinesIntersect(
                    w.x - selectionBoxSize,
                    w.y,
                    w.x,
                    w.y - selectionBoxSize,
                    l.x0,
                    l.y0,
                    l.x1,
                    l.y1)) {
                return idx;
            }
            if (utils::doLinesIntersect(
                    w.x,
                    w.y - selectionBoxSize,
                    w.x + selectionBoxSize,
                    w.y,
                    l.x0,
                    l.y0,
                    l.x1,
                    l.y1)) {
                return idx;
            }
            if (utils::doLinesIntersect(
                    w.x + selectionBoxSize,
                    w.y,
                    w.x,
                    w.y + selectionBoxSize,
                    l.x0,
                    l.y0,
                    l.x1,
                    l.y1)) {
                return idx;
            }
            if (utils::doLinesIntersect(
                    w.x,
                    w.y + selectionBoxSize,
                    w.x - selectionBoxSize,
                    w.y,
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

std::optional<std::size_t>
Level::movingObjectUnderCursor(sf::RenderWindow& window, unsigned mouseX, unsigned mouseY)
{
    // Note, this returns the index of the entire moving object, not the individual line
    const auto w = window.mapPixelToCoords({ static_cast<int>(mouseX), static_cast<int>(mouseY) });
    std::size_t idx = 0;
    for (const auto& m : m_movingObjects) {
        for (const auto& l : m.lines) {
            if (!l.inactive) {
                if (utils::doLinesIntersect(w.x - 10, w.y, w.x, w.y - 10, l.x0, l.y0, l.x1, l.y1)) {
                    return idx;
                }
                if (utils::doLinesIntersect(w.x, w.y - 10, w.x + 10, w.y, l.x0, l.y0, l.x1, l.y1)) {
                    return idx;
                }
                if (utils::doLinesIntersect(w.x + 10, w.y, w.x, w.y + 10, l.x0, l.y0, l.x1, l.y1)) {
                    return idx;
                }
                if (utils::doLinesIntersect(w.x, w.y + 10, w.x - 10, w.y, l.x0, l.y0, l.x1, l.y1)) {
                    return idx;
                }
            }
        }
        ++idx;
    }
    return std::nullopt;
}

void mgo::Level::processEvent(sf::RenderWindow& window, const sf::Event& event)
{
    if (event.is<sf::Event::Closed>()) {
        quit(window);
    }
    if (m_isDialogActive) {
        // if a dialog is active then we respond differently to events:
        if (event.is<sf::Event::KeyPressed>()) {
            const auto scancode = event.getIf<sf::Event::KeyPressed>()->scancode;
            switch (scancode) {
                case sf::Keyboard::Scancode::Escape:
                    m_isDialogActive = false;
                    m_dialogCallback(false, "");
                    break;
                case sf::Keyboard::Scancode::Enter:
                    m_isDialogActive = false;
                    m_dialogCallback(true, "");
                    break;
                default:
                    break;
            }
        }
        return;
    }
    if (event.is<sf::Event::KeyPressed>()) {
        // Mode switchers - require shift key, e.g. Shift-L for line etc
        const auto scancode = event.getIf<sf::Event::KeyPressed>()->scancode;
        if (event.getIf<sf::Event::KeyPressed>()->shift) {
            switch (scancode) {
                case sf::Keyboard::Scancode::B:
                    changeMode(Mode::BREAKABLE);
                    break;
                case sf::Keyboard::Scancode::E:
                    changeMode(Mode::EDIT);
                    break;
                case sf::Keyboard::Scancode::L:
                    changeMode(Mode::LINE);
                    break;
                case sf::Keyboard::Scancode::F:
                    changeMode(Mode::FUEL);
                    break;
                case sf::Keyboard::Scancode::S:
                    changeMode(Mode::START);
                    break;
                case sf::Keyboard::Scancode::V:
                    changeMode(Mode::MOVING);
                    break;
                case sf::Keyboard::Scancode::X:
                    changeMode(Mode::EXIT);
                    break;
                case sf::Keyboard::Scancode::P:
                    changeMode(Mode::POLYGON_CENTRE);
                    break;
                default:
                    break;
            }
        } else {
            switch (scancode) {
                case sf::Keyboard::Scancode::V:
                    {
                        // Convert selected lines to a movable object
                        if (!m_highlightedLineIndices.empty()) {
                            MovingObject m;
                            for (std::size_t i : m_highlightedLineIndices) {
                                m_lines[i].r = 255;
                                m_lines[i].g = 172;
                                m_lines[i].b = 163;
                                m.lines.push_back(m_lines[i]);
                                m_lines[i].inactive = true;
                            }
                            m_movingObjects.push_back(m);
                        }
                    }
                    break;
                case sf::Keyboard::Scancode::T:
                    {
                        std::string title = getInputFromDialog(
                            window, m_fixedView, m_font, "Enter Level Title", m_levelDescription);
                        if (!title.empty()) {
                            m_levelDescription = title;
                            m_window.setTitle(m_fileName + " - " + m_levelDescription);
                            m_dirty = true;
                        }
                        break;
                    }
                case sf::Keyboard::Scancode::Equal:
                    {
                        zoomIn();
                        break;
                    }
                case sf::Keyboard::Scancode::Hyphen:
                    {
                        zoomOut();
                        break;
                    }
                case sf::Keyboard::Scancode::Q:
                    quit(window);
                    break;
                case sf::Keyboard::Scancode::S:
                    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LSystem)
                        || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RSystem)) {
                        save();
                    } else {
                        changeSnapMode();
                    }
                    break;
                case sf::Keyboard::Scancode::Y: // redo
                    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LSystem)
                        || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RSystem)) {
                        redo();
                    } else {
                        if (m_highlightedMovingObjectIdx.has_value()) {
                            auto& obj = m_movingObjects[m_highlightedMovingObjectIdx.value()];
                            // Edit moving object's Y delta and max
                            std::string s = getInputFromDialog(
                                window,
                                m_fixedView,
                                m_font,
                                "Enter Y Delta",
                                utils::to_string_with_precision(obj.yDelta, 1),
                                InputType::numeric);
                            if (!s.empty()) {
                                float delta = std::stof(s);
                                obj.yDelta = delta;
                                m_dirty = true;
                            }
                            s = getInputFromDialog(
                                window,
                                m_fixedView,
                                m_font,
                                "Enter Y +/- Max Motion (Squares = 50)",
                                utils::to_string_with_precision(obj.yMaxDifference, 1),
                                InputType::numeric);
                            if (!s.empty()) {
                                float diff = std::stof(s);
                                obj.yMaxDifference = diff;
                                m_dirty = true;
                            }
                        }
                    }
                    break;
                case sf::Keyboard::Scancode::Z: // undo
                    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LSystem)
                        || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RSystem)) {
                        undo();
                    }
                    break;
                case sf::Keyboard::Scancode::Backspace:
                case sf::Keyboard::Scancode::Delete:
                    for (const size_t i : m_highlightedLineIndices) {
                        m_lines[i].inactive = true;
                        addReplayItem({ Mode::EDIT, i });
                    }
                    m_highlightedLineIndices.clear();
                    m_dirty = true;
                    if (m_highlightedMovingObjectIdx.has_value()) {
                        m_movingObjects.erase(
                            m_movingObjects.begin() + m_highlightedMovingObjectIdx.value());
                        // TODO: not sure if this will work yet:
                        addReplayItem({ Mode::EDIT, m_highlightedMovingObjectIdx.value() });
                        m_highlightedMovingObjectIdx = std::nullopt;
                        m_dirty = true;
                    }
                    break;
                case sf::Keyboard::Scancode::Escape:
                    m_currentInsertionLine.inactive = true;
                    finishCurrentMovingObject();
                    break;
                case sf::Keyboard::Scancode::Left:
                    if (m_highlightedMovingObjectIdx.has_value()) {
                        moveMovingObject(*m_highlightedMovingObjectIdx, -1, 0);
                    } else {
                        moveLines(-1, 0);
                    }
                    break;
                case sf::Keyboard::Scancode::Right:
                    if (m_highlightedMovingObjectIdx.has_value()) {
                        moveMovingObject(*m_highlightedMovingObjectIdx, 1, 0);
                    } else {
                        moveLines(1, 0);
                    }
                    break;
                case sf::Keyboard::Scancode::Up:
                    if (m_highlightedMovingObjectIdx.has_value()) {
                        moveMovingObject(*m_highlightedMovingObjectIdx, 0, -1);
                    } else {
                        moveLines(0, -1);
                    }
                    break;
                case sf::Keyboard::Scancode::Down:
                    if (m_highlightedMovingObjectIdx.has_value()) {
                        moveMovingObject(*m_highlightedMovingObjectIdx, 0, 1);
                    } else {
                        moveLines(0, 1);
                    }
                    break;
                case sf::Keyboard::Scancode::X:
                    // Edit moving object's X delta and max difference
                    if (m_highlightedMovingObjectIdx.has_value()) {
                        auto& obj = m_movingObjects[m_highlightedMovingObjectIdx.value()];
                        std::string s = getInputFromDialog(
                            window,
                            m_fixedView,
                            m_font,
                            "Enter X Delta",
                            utils::to_string_with_precision(obj.xDelta, 1),
                            InputType::numeric);
                        if (!s.empty()) {
                            float delta = std::stof(s);
                            obj.xDelta = delta;
                            m_dirty = true;
                        }
                        s = getInputFromDialog(
                            window,
                            m_fixedView,
                            m_font,
                            "Enter X Max +/- Motion (Squares = 50)",
                            utils::to_string_with_precision(obj.xMaxDifference, 1),
                            InputType::numeric);
                        if (!s.empty()) {
                            float diff = std::stof(s);
                            obj.xMaxDifference = diff;
                            m_dirty = true;
                        }
                    }
                    break;
                case sf::Keyboard::Scancode::G:
                    if (m_highlightedMovingObjectIdx.has_value()) {
                        auto& obj = m_movingObjects[m_highlightedMovingObjectIdx.value()];
                        std::string s = getInputFromDialog(
                            window,
                            m_fixedView,
                            m_font,
                            "Enter Gravity (between 10 and 100 is good)",
                            utils::to_string_with_precision(obj.gravity, 1),
                            InputType::numeric);
                        if (!s.empty()) {
                            float gravity = std::stof(s);
                            obj.gravity = gravity;
                            m_dirty = true;
                        }
                    }
                    break;
                // Note Y is handled above as it's also used with Cmd for Redo
                case sf::Keyboard::Scancode::R:
                    // Edit moving object's rotation delta
                    if (m_highlightedMovingObjectIdx.has_value()) {
                        auto& obj = m_movingObjects[m_highlightedMovingObjectIdx.value()];
                        std::string s = getInputFromDialog(
                            window,
                            m_fixedView,
                            m_font,
                            "Enter Rotation delta",
                            utils::to_string_with_precision(obj.rotationDelta, 1),
                            InputType::numeric);
                        if (!s.empty()) {
                            float delta = std::stof(s);
                            obj.rotationDelta = delta;
                            m_dirty = true;
                        }
                    }
                    break;
                default:
                    break;
            }
        }
    }
    if (event.getIf<sf::Event::MouseMoved>()) {
        const auto mouseMove = event.getIf<sf::Event::MouseMoved>()->position;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift)) {
            if (m_oldMouseX.has_value()) {
                int xDelta = m_oldMouseX.value() - mouseMove.x;
                int yDelta = m_oldMouseY.value() - mouseMove.y;
                xDelta *= m_viewZoomLevel;
                yDelta *= m_viewZoomLevel;
                m_view.move({ static_cast<float>(xDelta), static_cast<float>(yDelta) });
            }
        } else {
            if (m_currentMode == Mode::LINE || m_currentMode == Mode::BREAKABLE
                || m_currentMode == Mode::MOVING) {
                if (m_snapMode == SnapMode::AUTO || m_snapMode == SnapMode::GRID) {
                    highlightGridVertex(window, mouseMove.x, mouseMove.y);
                }
                if (m_snapMode == SnapMode::AUTO || m_snapMode == SnapMode::LINE) {
                    highlightNearestLinePoint(window, mouseMove.x, mouseMove.y);
                }
                if (m_currentInsertionLine.inactive == false) {
                    if (m_currentNearestSnapPoint.has_value()) {
                        m_currentInsertionLine.x1 = std::get<0>(m_currentNearestSnapPoint.value());
                        m_currentInsertionLine.y1 = std::get<1>(m_currentNearestSnapPoint.value());
                    } else {
                        auto w = window.mapPixelToCoords(
                            { static_cast<int>(mouseMove.x), static_cast<int>(mouseMove.y) });
                        m_currentInsertionLine.x1 = w.x;
                        m_currentInsertionLine.y1 = w.y;
                    }
                }
            } else if (m_currentMode == Mode::POLYGON_RADIUS) {
                auto w = window.mapPixelToCoords(
                    { static_cast<int>(mouseMove.x), static_cast<int>(mouseMove.y) });
                m_currentPolygon.lines = utils::getRegularPolygon(
                    w.x,
                    w.y,
                    *m_currentPolygon.centreX,
                    *m_currentPolygon.centreY,
                    m_currentPolygon.sides);

            } else {
                m_currentNearestSnapPoint = std::nullopt;
            }
        }
        m_oldMouseX = mouseMove.x;
        m_oldMouseY = mouseMove.y;
    }
    if (event.getIf<sf::Event::MouseButtonPressed>()) {
        const auto mouseButton = event.getIf<sf::Event::MouseButtonPressed>()->button;
        const auto mousePos = event.getIf<sf::Event::MouseButtonPressed>()->position;
        if (mouseButton == sf::Mouse::Button::Right) {
            m_currentInsertionLine.inactive = true;
            finishCurrentMovingObject();
        } else if (mouseButton == sf::Mouse::Button::Left) {
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
                                    if (m_currentMode == Mode::LINE) {
                                        m_currentInsertionLine.breakable = false;
                                    } else {
                                        m_currentInsertionLine.breakable = true;
                                    }
                                    if (m_currentMode == Mode::MOVING) {
                                        Line l = m_currentInsertionLine;
                                        l.r = 255;
                                        l.g = 172;
                                        l.b = 163;
                                        m_currentMovingObject.lines.push_back(l);
                                    } else {
                                        m_lines.push_back(m_currentInsertionLine);
                                    }
                                    addReplayItem(
                                        { m_currentMode,
                                          0,
                                          m_currentInsertionLine.x0,
                                          m_currentInsertionLine.y0,
                                          m_currentInsertionLine.x1,
                                          m_currentInsertionLine.y1,
                                          0 });
                                    m_dirty = true;
                                    // next line starts at the current line's end:
                                    m_currentInsertionLine.x0 = m_currentInsertionLine.x1;
                                    m_currentInsertionLine.y0 = m_currentInsertionLine.y1;
                                }
                            } else {
                                // else this is a new line
                                unsigned x;
                                unsigned y;
                                if (!m_currentNearestSnapPoint.has_value()) {
                                    auto w = window.mapPixelToCoords(
                                        { static_cast<int>(mousePos.x),
                                          static_cast<int>(mousePos.y) });
                                    x = w.x;
                                    y = w.y;
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
                        if (!(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LSystem)
                              || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RSystem))) {
                            m_highlightedLineIndices.clear();
                        }
                        // Check to see if there is a line under the cursor
                        auto line = lineUnderCursor(window, mousePos.x, mousePos.y);
                        if (!(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl)
                              || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl))) {
                            addOrRemoveHighlightedLine(line, true);
                        } else {
                            addOrRemoveHighlightedLine(line, false);
                        }
                        if (line.has_value()) {
                            m_highlightedMovingObjectIdx = std::nullopt;
                        } else {
                            m_highlightedLineIndices.clear();
                            auto movingObject
                                = movingObjectUnderCursor(window, mousePos.x, mousePos.y);
                            if (movingObject.has_value()) {
                                m_highlightedMovingObjectIdx = movingObject.value();
                            }
                        }
                        break;
                    }
                case Mode::START:
                    {
                        auto w = window.mapPixelToCoords(
                            { static_cast<int>(mousePos.x), static_cast<int>(mousePos.y) });
                        // if we're within r workspace units of an existing start object,
                        // we treat additional clicks as a way to modify the object's rotation
                        unsigned r = 20;
                        if (m_startPosition.has_value()
                            && ((m_startPosition.value().x > w.x - r
                                 && m_startPosition.value().x < w.x + r)
                                && (m_startPosition.value().y > w.y - r
                                    && m_startPosition.value().y < w.y + r))) {
                            m_startPosition.value().r += 15;
                            if (m_startPosition.value().r >= 360) {
                                m_startPosition.value().r = 0;
                            }
                            addReplayItem(
                                { Mode::START,
                                  0,
                                  m_startPosition.value().x,
                                  m_startPosition.value().y,
                                  0,
                                  0,
                                  m_startPosition.value().r });
                            m_dirty = true;
                        } else {
                            m_startPosition
                                = { static_cast<unsigned>(w.x), static_cast<unsigned>(w.y), 0 };
                            addReplayItem(
                                { Mode::START,
                                  0,
                                  static_cast<unsigned>(w.x),
                                  static_cast<unsigned>(w.y) });
                            m_dirty = true;
                        }
                        break;
                    }
                case Mode::EXIT:
                    {
                        auto w = window.mapPixelToCoords(
                            { static_cast<int>(mousePos.x), static_cast<int>(mousePos.y) });
                        m_exitPosition = std::make_pair(w.x, w.y);
                        addReplayItem(
                            { Mode::EXIT,
                              0,
                              static_cast<unsigned>(w.x),
                              static_cast<unsigned>(w.y),
                              0,
                              0 });
                        m_dirty = true;
                        break;
                    }
                case Mode::FUEL:
                    {
                        auto w = window.mapPixelToCoords(
                            { static_cast<int>(mousePos.x), static_cast<int>(mousePos.y) });
                        // if we are within r workspace units of an existing fuel object, we
                        // treat this as a request to delete it instead of placing a new one
                        unsigned r = 20;
                        std::size_t idx = 0;
                        bool erased { false };
                        for (const auto& f : m_fuelObjects) {
                            if ((f.first > w.x - r && f.first < w.x + r)
                                && (f.second > w.y - r && f.second < w.y + r)) {
                                m_fuelObjects.erase(m_fuelObjects.begin() + idx);
                                erased = true;
                                addReplayItem({ Mode::FUEL, idx, 0, 0, 0, 0, 0, true });
                                m_dirty = true;
                                break;
                            }
                            ++idx;
                        }
                        if (!erased) {
                            m_fuelObjects.push_back(std::make_pair(w.x, w.y));
                            addReplayItem(
                                { Mode::FUEL,
                                  idx,
                                  static_cast<unsigned>(w.x),
                                  static_cast<unsigned>(w.y) });
                            m_dirty = true;
                        }
                        break;
                    }
                case Mode::POLYGON_CENTRE:
                    {
                        auto w = window.mapPixelToCoords(
                            { static_cast<int>(mousePos.x), static_cast<int>(mousePos.y) });
                        m_currentPolygon.centreX = w.x;
                        m_currentPolygon.centreY = w.y;
                        std::string s = getInputFromDialog(
                            window,
                            m_fixedView,
                            m_font,
                            "Enter Number of Sides (3-64)",
                            "12",
                            InputType::numeric);
                        if (!s.empty()) {
                            unsigned sides = std::stoi(s);
                            if (sides < 3) {
                                sides = 3;
                            }
                            if (sides > 64) {
                                sides = 64;
                            }
                            m_currentPolygon.sides = sides;
                            m_dirty = true;
                        }
                        changeMode(Mode::POLYGON_RADIUS);
                        break;
                    }
                case Mode::POLYGON_RADIUS:
                    {
                        // Finalise an in-progress polygon
                        auto w = window.mapPixelToCoords(
                            { static_cast<int>(mousePos.x), static_cast<int>(mousePos.y) });
                        auto maxDist = std::max(
                            std::abs(static_cast<float>(w.x) - *m_currentPolygon.centreX),
                            std::abs(static_cast<float>(w.y) - *m_currentPolygon.centreY));
                        if (maxDist > 5.f) { // arbitrary lower limit for polygons' radii
                            m_lines.insert(
                                m_lines.end(),
                                m_currentPolygon.lines.begin(),
                                m_currentPolygon.lines.end());
                            m_currentPolygon.lines.clear();
                            m_currentPolygon.centreX = std::nullopt;
                            m_currentPolygon.centreY = std::nullopt;
                            changeMode(Mode::POLYGON_CENTRE);
                        }
                    }

                default:
                    break;
            }
        }
    }
    if (event.getIf<sf::Event::MouseWheelScrolled>()) {
        const auto evt = event.getIf<sf::Event::MouseWheelScrolled>();
        if (evt->wheel == sf::Mouse::Wheel::Vertical) {
            float amt = evt->delta;
            if (std::abs(amt) > 0.1 && std::abs(amt) < 10.f) {
                if (amt > 0.f) {
                    auto c = m_view.getCenter();
                    auto m = window.mapPixelToCoords({ evt->position.x, evt->position.y });
                    float dx = (m.x - c.x) * 0.05f;
                    float dy = (m.y - c.y) * 0.05f;
                    m_view.move({ dx, dy });
                    zoomIn();
                } else if (amt < 0.f) {
                    zoomOut();
                }
            }
        }
    }
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

void Level::quit(sf::RenderWindow& window)
{
    std::string msg = "Are you sure?";
    if (m_dirty) {
        msg = "File has UNSAVED CHANGES! Are you sure?";
    }
    msgbox("Quit", msg, [&window](bool yes, const std::string) {
        if (yes) {
            window.close();
        }
    });
}

void Level::zoomOut()
{
    if (m_view.getSize().x < 2400.f) {
        m_view.zoom(1.05f);
        m_viewZoomLevel *= 1.05f;
    }
}

void Level::zoomIn()
{
    if (m_view.getSize().x > 100.f) {
        m_view.zoom(0.95f);
        m_viewZoomLevel *= 0.95f;
    }
}

bool mgo::Level::msgbox(
    const std::string& title,
    const std::string& message,
    std::function<void(bool, const std::string&)> callback)
{
    m_isDialogActive = true;
    m_dialog.setSize(sf::Vector2f(600, 120));
    m_dialog.setFillColor(sf::Color(220, 220, 220));
    m_dialog.setPosition({ 25.f, 25.f });
    m_dialogTitle.setFont(m_font);
    m_dialogTitle.setCharacterSize(20);
    m_dialogTitle.setFillColor(sf::Color::Black);
    m_dialogTitle.setStyle(sf::Text::Bold);
    m_dialogTitle.setString(title);
    m_dialogTitle.setPosition({ 40.f, 30.f });
    m_dialogText.setFont(m_font);
    m_dialogText.setCharacterSize(14);
    m_dialogText.setFillColor(sf::Color::Black);
    m_dialogText.setString(message + "\n\nEnter for OK, Esc for Cancel");
    m_dialogText.setPosition({ 40.f, 70.f });
    m_dialogCallback = callback;
    return false;
}

void mgo::Level::drawModes(sf::RenderWindow& window)
{
    // Main insertion mode:
    sf::Text txtMode(m_font);
    txtMode.setFillColor(sf::Color::Cyan);
    txtMode.setCharacterSize(14);
    txtMode.setPosition({ 5.f, 5.f });
    switch (m_currentMode) {
        case Mode::LINE:
            txtMode.setString("LINE");
            break;
        case Mode::BREAKABLE:
            txtMode.setString("BREAKABLE");
            break;
        case Mode::EDIT:
            txtMode.setString("EDIT");
            break;
        case Mode::START:
            txtMode.setString("START");
            break;
        case Mode::EXIT:
            txtMode.setString("EXIT");
            break;
        case Mode::FUEL:
            txtMode.setString("FUEL");
            break;
        case Mode::MOVING:
            txtMode.setString("MOVING");
            break;
        case Mode::POLYGON_CENTRE:
        case Mode::POLYGON_RADIUS:
            txtMode.setString("POLYGON");
            break;
        default:
            break;
    }
    window.draw(txtMode);

    // Snapping mode:
    sf::Text txtSnap(m_font);
    txtSnap.setFillColor(sf::Color::Green);
    txtSnap.setCharacterSize(14);
    txtSnap.setPosition({ 95.f, 5.f });
    switch (m_snapMode) {
        case SnapMode::AUTO:
            txtSnap.setString("Snap: AUTO");
            break;
        case SnapMode::GRID:
            txtSnap.setString("Snap: GRID");
            break;
        case SnapMode::LINE:
            txtSnap.setString("Snap: LINE");
            break;
        case SnapMode::NONE:
            txtSnap.setString("Snap: NONE");
            break;
        default:
            break;
    }
    window.draw(txtSnap);
}

void mgo::Level::highlightGridVertex(sf::RenderWindow& window, unsigned mouseX, unsigned mouseY)
{
    const auto w = window.mapPixelToCoords({ static_cast<int>(mouseX), static_cast<int>(mouseY) });

    const unsigned x = static_cast<unsigned>(static_cast<double>(w.x) / 50.0 + 0.5) * 50.0;
    const unsigned y = static_cast<unsigned>(static_cast<double>(w.y) / 50.0 + 0.5) * 50.0;
    if (x > 2000 || y > 2000) {
        m_currentNearestSnapPoint = std::nullopt;
    } else {
        m_currentNearestSnapPoint = std::tie(x, y);
    }
}

void Level::highlightNearestLinePoint(sf::RenderWindow& window, unsigned mouseX, unsigned mouseY)
{
    const auto w = window.mapPixelToCoords({ static_cast<int>(mouseX), static_cast<int>(mouseY) });
    for (const auto& l : m_lines) {
        if (l.inactive) {
            continue;
        }
        auto nearest = utils::closestPointOnLine(l.x0, l.y0, l.x1, l.y1, w.x, w.y, 5);
        if (nearest.has_value()) {
            m_currentNearestSnapPoint = std::tie(nearest.value().first, nearest.value().second);
            return;
        }
    }
    for (const auto& m : m_movingObjects) {
        for (const auto& l : m.lines) {
            if (l.inactive) {
                continue;
            }
            auto nearest = utils::closestPointOnLine(l.x0, l.y0, l.x1, l.y1, w.x, w.y, 5);
            if (nearest.has_value()) {
                m_currentNearestSnapPoint = std::tie(nearest.value().first, nearest.value().second);
                return;
            }
        }
    }
    // Also check moving object in progress
    for (const auto& l : m_currentMovingObject.lines) {
        if (l.inactive) {
            continue;
        }
        auto nearest = utils::closestPointOnLine(l.x0, l.y0, l.x1, l.y1, w.x, w.y, 5);
        if (nearest.has_value()) {
            m_currentNearestSnapPoint = std::tie(nearest.value().first, nearest.value().second);
            return;
        }
    }
}

void Level::drawObjects(sf::RenderWindow& window)
{
    if (m_startPosition.has_value()) {
        sf::ConvexShape ship;
        ship.setPointCount(3);
        // define the points
        ship.setPoint(0, sf::Vector2f(0, -20));
        ship.setPoint(1, sf::Vector2f(10, 20));
        ship.setPoint(2, sf::Vector2f(-10, 20));
        ship.setFillColor(sf::Color::Green);
        ship.setPosition(
            { static_cast<float>(m_startPosition.value().x),
              static_cast<float>(m_startPosition.value().y) });
        ship.setRotation(sf::degrees(360.f - m_startPosition.value().r));
        window.draw(ship);
    }
    if (m_exitPosition.has_value()) {
        sf::Text exit(m_font);
        exit.setCharacterSize(26.f);
        exit.setString("EXIT");
        sf::FloatRect textRect = exit.getLocalBounds();
        exit.setOrigin({ textRect.size.x / 2.f, textRect.size.y / 2.f });
        exit.setFillColor({ 52, 213, 235 });
        exit.setPosition(
            { static_cast<float>(m_exitPosition.value().first),
              static_cast<float>(m_exitPosition.value().second) });
        window.draw(exit);
    }
    if (!m_fuelObjects.empty()) {
        for (const auto& p : m_fuelObjects) {
            sf::CircleShape c;
            c.setFillColor(sf::Color::Yellow);
            float r = 10.f;
            c.setRadius(r);
            c.setOrigin({ r, r });
            c.setPosition({ static_cast<float>(p.first), static_cast<float>(p.second) });
            window.draw(c);
        }
    }
}

sf::View& Level::getView()
{
    return m_view;
}

sf::View& Level::getFixedView()
{
    return m_fixedView;
}

void Level::clampViewport()
{
    const float mapWidth = 2000.f;
    const float mapHeight = 2000.f;

    sf::Vector2f viewCentre = m_view.getCenter();

    // Calculate half the view size (what's visible on screen)
    sf::Vector2f halfSize = m_view.getSize() / 2.f;

    viewCentre.x = std::max(halfSize.x, std::min(viewCentre.x, mapWidth - halfSize.x));
    viewCentre.y = std::max(halfSize.y, std::min(viewCentre.y, mapHeight - halfSize.y));

    // If the view is larger than the map in either dimension, centre it
    if (m_view.getSize().x > mapWidth) {
        viewCentre.x = mapWidth / 2.f;
    }
    if (m_view.getSize().y > mapHeight) {
        viewCentre.y = mapHeight / 2.f;
    }
    m_view.setCenter(viewCentre);
}

void Level::revert()
{
    // Revert to last saved state
    // Clear collections:
    m_lines.clear();
    m_startPosition = std::nullopt;
    m_exitPosition = std::nullopt;
    m_fuelObjects.clear();
    m_movingObjects.clear();
    // reload
    load(m_fileName);
    m_dirty = false;
}

void Level::undo()
{
    // The way undo works is to revert to the last save, then reapply all steps held in
    // m_replay. This will allow for redo as well.
    m_currentInsertionLine.inactive = true;
    revert();
    replay();
    if (m_replayIndex >= 0) {
        --m_replayIndex; // can go to -1
    }
}

void Level::replay()
{
    for (long i = 0; i < m_replayIndex; ++i) {
        if (i > static_cast<long>(m_replay.size()) - 1) {
            break;
        }
        const auto& a = m_replay[i];
        switch (a.actionType) {
            case Mode::LINE:
                {
                    Line line;
                    line.breakable = false;
                    line.x0 = a.x0;
                    line.y0 = a.y0;
                    line.x1 = a.x1;
                    line.y1 = a.y1;
                    line.r = 255;
                    m_lines.push_back(line);
                }
                break;
            case Mode::BREAKABLE:
                {
                    Line line;
                    line.breakable = true;
                    line.x0 = a.x0;
                    line.y0 = a.y0;
                    line.x1 = a.x1;
                    line.y1 = a.y1;
                    m_lines.push_back(line);
                }
                break;
            case Mode::EDIT:
                m_lines[a.index].inactive = true;
                break;
            case Mode::EXIT:
                m_exitPosition = std::make_pair(a.x0, a.y0);
                break;
            case Mode::START:
                m_startPosition = { a.x0, a.y0, a.rotation };
                break;
            case Mode::FUEL:
                if (a.erased) {
                    m_fuelObjects.erase(m_fuelObjects.begin() + a.index);
                } else {
                    m_fuelObjects.push_back(std::make_pair(a.x0, a.y0));
                }
                break;
            case Mode::MOVING:
                {
                    Line l;
                    l.x0 = a.x0;
                    l.y0 = a.y0;
                    l.x1 = a.x1;
                    l.y1 = a.y1;
                    l.r = 255;
                    l.g = 172;
                    l.b = 163;
                    m_currentMovingObject.lines.push_back(l);
                }
                break;
            default:
                std::cout << "Unknown action type in replay: " << static_cast<int>(a.actionType)
                          << std::endl;
                assert(false);
        }
    }
}

void Level::redo()
{
    if (m_replay.empty() || m_replayIndex >= static_cast<long>(m_replay.size())) {
        return;
    }
    m_replayIndex += 2;
    replay();
}

void Level::addReplayItem(const Action& action)
{
    std::size_t from = 1 + (m_replayIndex > 0 ? m_replayIndex : 0);
    if (from < m_replay.size()) {
        m_replay.erase(m_replay.begin() + from, m_replay.end());
    }
    m_replay.push_back(action);
    m_replayIndex = m_replay.size() - 1;
}

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
    }
    m_currentInsertionLine.inactive = true;
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

void Level::changeSnapMode()
{
    switch (m_snapMode) {
        case SnapMode::AUTO:
            m_snapMode = SnapMode::LINE;
            break;
        case SnapMode::LINE:
            m_snapMode = SnapMode::GRID;
            break;
        case SnapMode::GRID:
            m_snapMode = SnapMode::NONE;
            break;
        case SnapMode::NONE:
            m_snapMode = SnapMode::AUTO;
            break;
        default:
            assert(false);
            break;
    }
    m_currentNearestSnapPoint = std::nullopt;
}

} // namespace