#include "level.h"
#include "dialog.h"
#include "helperfunctions.h"

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
        Line l1 { minX, minY, minX, maxY, 128, 128, 0 };
        drawLine(window, l1, std::nullopt);
        Line l2 { minX, maxY, maxX, maxY, 128, 128, 0 };
        drawLine(window, l2, std::nullopt);
        Line l3 { maxX, maxY, maxX, minY, 128, 128, 0 };
        drawLine(window, l3, std::nullopt);
        Line l4 { minX, minY, maxX, minY, 128, 128, 0 };
        drawLine(window, l4, std::nullopt);
        ++idx;
    }
    for (const auto& l : m_currentMovingObject.lines) {
        drawLine(window, l, std::nullopt);
    }
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
    if (idx.has_value() && m_highlightedLineIdx.has_value()
        && m_highlightedLineIdx.value() == idx.value()) {
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
            if (helperfunctions::doLinesIntersect(
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
            if (helperfunctions::doLinesIntersect(
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
            if (helperfunctions::doLinesIntersect(
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
            if (helperfunctions::doLinesIntersect(
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
                if (helperfunctions::doLinesIntersect(
                        w.x - 10, w.y, w.x, w.y - 10, l.x0, l.y0, l.x1, l.y1)) {
                    return idx;
                }
                if (helperfunctions::doLinesIntersect(
                        w.x, w.y - 10, w.x + 10, w.y, l.x0, l.y0, l.x1, l.y1)) {
                    return idx;
                }
                if (helperfunctions::doLinesIntersect(
                        w.x + 10, w.y, w.x, w.y + 10, l.x0, l.y0, l.x1, l.y1)) {
                    return idx;
                }
                if (helperfunctions::doLinesIntersect(
                        w.x, w.y + 10, w.x - 10, w.y, l.x0, l.y0, l.x1, l.y1)) {
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
        bool handled = false;
        if (event.getIf<sf::Event::KeyPressed>()->shift) {
            switch (scancode) {
                case sf::Keyboard::Scancode::B:
                    changeMode(Mode::BREAKABLE);
                    handled = true;
                    break;
                case sf::Keyboard::Scancode::E:
                    changeMode(Mode::EDIT);
                    handled = true;
                    break;
                case sf::Keyboard::Scancode::L:
                    changeMode(Mode::LINE);
                    handled = true;
                    break;
                case sf::Keyboard::Scancode::F:
                    changeMode(Mode::FUEL);
                    handled = true;
                    break;
                case sf::Keyboard::Scancode::S:
                    changeMode(Mode::START);
                    handled = true;
                    break;
                case sf::Keyboard::Scancode::V:
                    changeMode(Mode::MOVING);
                    handled = true;
                    break;
                case sf::Keyboard::Scancode::X:
                    changeMode(Mode::EXIT);
                    handled = true;
                    break;
                default:
                    break;
            }
        }
        if (!handled) {
            switch (scancode) {
                case sf::Keyboard::Scancode::T:
                    {
                        m_levelDescription = getInputFromDialog(
                            window, m_fixedView, m_font, "Enter Level Title", m_levelDescription);
                        m_window.setTitle(m_fileName + " - " + m_levelDescription);
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
                case sf::Keyboard::Scancode::M:
                    cycleMode(event.getIf<sf::Event::KeyPressed>()->shift);
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
                                helperfunctions::to_string_with_precision(obj.yDelta, 1),
                                InputType::numeric);
                            if (!s.empty()) {
                                float delta = std::stof(s);
                                obj.yDelta = delta;
                            }
                            s = getInputFromDialog(
                                window,
                                m_fixedView,
                                m_font,
                                "Enter Y +/- Max Motion (Squares = 50)",
                                helperfunctions::to_string_with_precision(obj.yMaxDifference, 1),
                                InputType::numeric);
                            if (!s.empty()) {
                                float diff = std::stof(s);
                                obj.yMaxDifference = diff;
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
                    if (m_highlightedLineIdx.has_value()) {
                        m_lines[m_highlightedLineIdx.value()].inactive = true;
                        addReplayItem({ Mode::EDIT, m_highlightedLineIdx.value() });
                        m_highlightedLineIdx = std::nullopt;
                        m_dirty = true;
                    } else if (m_highlightedMovingObjectIdx.has_value()) {
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
                    m_view.move({ -25, 0 });
                    break;
                case sf::Keyboard::Scancode::Right:
                    m_view.move({ 25, 0 });
                    break;
                case sf::Keyboard::Scancode::Up:
                    m_view.move({ 0, -25 });
                    break;
                case sf::Keyboard::Scancode::Down:
                    m_view.move({ 0, 25 });
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
                            helperfunctions::to_string_with_precision(obj.xDelta, 1),
                            InputType::numeric);
                        if (!s.empty()) {
                            float delta = std::stof(s);
                            obj.xDelta = delta;
                        }
                        s = getInputFromDialog(
                            window,
                            m_fixedView,
                            m_font,
                            "Enter X Max +/- Motion (Squares = 50)",
                            helperfunctions::to_string_with_precision(obj.xMaxDifference, 1),
                            InputType::numeric);
                        if (!s.empty()) {
                            float diff = std::stof(s);
                            obj.xMaxDifference = diff;
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
                            helperfunctions::to_string_with_precision(obj.gravity, 1),
                            InputType::numeric);
                        if (!s.empty()) {
                            float gravity = std::stof(s);
                            obj.gravity = gravity;
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
                            helperfunctions::to_string_with_precision(obj.rotationDelta, 1),
                            InputType::numeric);
                        if (!s.empty()) {
                            float delta = std::stof(s);
                            obj.rotationDelta = delta;
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
            if (m_currentMode == Mode::EDIT) {
                // If we're in edit mode, we want to highlight the line under the cursor
                auto line = lineUnderCursor(window, mouseMove.x, mouseMove.y);
                if (line.has_value()) {
                    m_highlightedLineIdx = line.value();
                    m_highlightedMovingObjectIdx = std::nullopt;
                } else {
                    auto movingObject = movingObjectUnderCursor(window, mouseMove.x, mouseMove.y);
                    if (movingObject.has_value()) {
                        m_highlightedMovingObjectIdx = movingObject.value();
                        m_highlightedLineIdx = std::nullopt;
                    } else {
                        m_highlightedLineIdx = std::nullopt;
                        m_highlightedMovingObjectIdx = std::nullopt;
                    }
                }
            }
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
                        // TODO this is probably redundant now that we highlight the
                        // object on hover instead
                        // Check to see if there is a line under the cursor
                        auto line = lineUnderCursor(window, mousePos.x, mousePos.y);
                        if (line.has_value()) {
                            m_highlightedLineIdx = line.value();
                            m_highlightedMovingObjectIdx = std::nullopt;
                        } else {
                            auto movingObject
                                = movingObjectUnderCursor(window, mousePos.x, mousePos.y);
                            if (movingObject.has_value()) {
                                m_highlightedMovingObjectIdx = movingObject.value();
                                m_highlightedLineIdx = std::nullopt;
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
        auto nearest = helperfunctions::closestPointOnLine(l.x0, l.y0, l.x1, l.y1, w.x, w.y, 5);
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
            auto nearest = helperfunctions::closestPointOnLine(l.x0, l.y0, l.x1, l.y1, w.x, w.y, 5);
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
        auto nearest = helperfunctions::closestPointOnLine(l.x0, l.y0, l.x1, l.y1, w.x, w.y, 5);
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
        exit.setCharacterSize(18.f);
        exit.setString("EXIT");
        sf::FloatRect textRect = exit.getLocalBounds();
        exit.setOrigin({ textRect.size.x / 2, textRect.size.y / 2 });
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

void Level::processViewport()
{
    // Clamp the view within the bounds of the map
    sf::Vector2f viewSize = m_view.getSize();
    sf::Vector2f viewCenter = m_view.getCenter();

    const float leftBound = viewSize.x / 3;
    const float rightBound = 2000 - viewSize.x / 3;
    const float topBound = viewSize.y / 3;
    const float bottomBound = 2000 - viewSize.y / 3;

    if (viewCenter.x < leftBound) {
        m_view.setCenter({ leftBound, viewCenter.y });
    }
    if (viewCenter.x > rightBound) {
        m_view.setCenter({ rightBound, viewCenter.y });
    }
    if (viewCenter.y < topBound) {
        m_view.setCenter({ viewCenter.x, topBound });
    }
    if (viewCenter.y > bottomBound) {
        m_view.setCenter({ viewCenter.x, bottomBound });
    }
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
    m_highlightedLineIdx = std::nullopt;
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

void Level::cycleMode(bool backwards)
{
    switch (m_currentMode) {
        case Mode::LINE:
            if (backwards) {
                changeMode(Mode::FUEL);
            } else {
                changeMode(Mode::BREAKABLE);
            }
            break;
        case Mode::BREAKABLE:
            if (backwards) {
                changeMode(Mode::LINE);
            } else {
                changeMode(Mode::EDIT);
            }
            break;
        case Mode::EDIT:
            if (backwards) {
                changeMode(Mode::BREAKABLE);
            } else {
                changeMode(Mode::START);
            }
            break;
        case Mode::START:
            if (backwards) {
                changeMode(Mode::EDIT);
            } else {
                changeMode(Mode::EXIT);
            }
            break;
        case Mode::EXIT:
            if (backwards) {
                changeMode(Mode::START);
            } else {
                changeMode(Mode::FUEL);
            }
            break;
        case Mode::FUEL:
            if (backwards) {
                changeMode(Mode::EXIT);
            } else {
                changeMode(Mode::LINE);
            }
            break;
        default:
            break;
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