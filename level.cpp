#include "level.h"
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
mgo::Level::Level(unsigned windowWidth, unsigned windowHeight)
{
    if (!m_font.loadFromFile("DroidSansMono.ttf")) {
        throw std::runtime_error("Could not load font file");
    }
    m_currentInsertionLine.inactive = true;
    m_currentInsertionLine.r = 255;
    m_currentInsertionLine.g = 0;
    m_currentInsertionLine.b = 0;
    m_view.reset(sf::FloatRect(0, 0, windowWidth, windowHeight));
    m_view.setViewport(sf::FloatRect(0, 0, 1, 1));
    m_fixedView.reset(sf::FloatRect(0, 0, windowWidth, windowHeight));
    m_fixedView.setViewport(sf::FloatRect(0, 0, 1, 1));
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
        BREAKABLE
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
        char c = vec[0][0];
        switch (c) {
            case '!': // timelimit, fuel, ship x, ship y, angle, description
                m_startPosition = { static_cast<unsigned>(stoi(vec[3])),
                                    static_cast<unsigned>(stoi(vec[4])),
                                    static_cast<unsigned>(stoi(vec[5])) };
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
                } else {
                    std::cout << "Unrecognized object type '" << vec[1] << "' in file\n";
                }
                break;
            case 'L':
                {
                    unsigned x0 = std::stoi(vec[1]);
                    unsigned y0 = std::stoi(vec[2]);
                    unsigned x1 = std::stoi(vec[3]);
                    unsigned y1 = std::stoi(vec[4]);
                    uint8_t r = std::stoi(vec[5]);
                    uint8_t g = std::stoi(vec[6]);
                    uint8_t b = std::stoi(vec[7]);
                    if (currentObject == ObjectType::OBSTRUCTION) {
                        m_lines.push_back({ x0, y0, x1, y1, r, g, b, 1, false, false });
                    } else {
                        m_lines.push_back({ x0, y0, x1, y1, r, g, b, 1, false, true });
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
    in.close();
}

void mgo::Level::save()
{
    // TODO, currently just outputs to stdout - may actually be OK like that?
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
            outfile << "!~0~0~" << startX << "~" << startY << "~" << rotation << "~Title\n";
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
    if (m_currentNearestGridVertex.has_value()) {
        sf::CircleShape c;
        c.setFillColor(sf::Color::Magenta);
        c.setRadius(3.f);
        c.setOrigin({ 3.f, 3.f });
        float x = std::get<0>(m_currentNearestGridVertex.value());
        float y = std::get<1>(m_currentNearestGridVertex.value());
        c.setPosition(x, y);
        window.draw(c);
    }
    if (m_currentInsertionLine.inactive == false) {
        drawLine(window, m_currentInsertionLine, std::nullopt);
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
    window.draw(line, 2, sf::Lines);
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
    auto w = window.mapPixelToCoords({ static_cast<int>(mouseX), static_cast<int>(mouseY) });
    std::size_t idx = 0;
    for (const auto& l : m_lines) {
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
        ++idx;
    }
    return std::nullopt;
}

void mgo::Level::processEvent(sf::RenderWindow& window, const sf::Event& event)
{
    if (event.type == sf::Event::Closed) {
        quit(window);
    }
    if (m_isDialogActive) {
        // if a dialog is active then we respond differently to events:
        if (event.type == sf::Event::KeyPressed) {
            switch (event.key.code) {
                case sf::Keyboard::Escape:
                    m_isDialogActive = false;
                    m_dialogCallback(false, "");
                    break;
                case sf::Keyboard::Enter:
                    m_isDialogActive = false;
                    m_dialogCallback(true, "");
                    break;
                default:
                    break;
            }
        }
        return;
    }
    if (event.type == sf::Event::KeyPressed) {
        // Mode switchers - require shift key, e.g. Shift-L for line etc
        if (event.key.shift) {
            switch (event.key.code) {
                case sf::Keyboard::B:
                    changeMode(Mode::BREAKABLE);
                    break;
                case sf::Keyboard::E:
                    changeMode(Mode::EDIT);
                    break;
                case sf::Keyboard::L:
                    changeMode(Mode::LINE);
                    break;
                case sf::Keyboard::F:
                    changeMode(Mode::FUEL);
                    break;
                case sf::Keyboard::S:
                    changeMode(Mode::START);
                    break;
                case sf::Keyboard::X:
                    changeMode(Mode::EXIT);
                    break;
                default:
                    break;
            }
        } else {
            switch (event.key.code) {
                case sf::Keyboard::Equal:
                    {
                        zoomIn();
                        break;
                    }
                case sf::Keyboard::Hyphen:
                    {
                        zoomOut();
                        break;
                    }
                case sf::Keyboard::Q:
                    quit(window);
                    break;
                case sf::Keyboard::S:
                    if (sf::Keyboard::isKeyPressed(sf::Keyboard::LSystem)
                        || sf::Keyboard::isKeyPressed(sf::Keyboard::RSystem)) {
                        save();
                    } else {
                        changeSnapMode();
                    }
                    break;
                case sf::Keyboard::M:
                    cycleMode(event.key.shift);
                    break;
                case sf::Keyboard::BackSpace:
                case sf::Keyboard::Delete:
                case sf::Keyboard::X:
                    if (m_highlightedLineIdx.has_value()) {
                        m_lines[m_highlightedLineIdx.value()].inactive = true;
                        m_highlightedLineIdx = std::nullopt;
                        m_dirty = true;
                    }
                    break;
                case sf::Keyboard::Escape:
                    m_currentInsertionLine.inactive = true;
                    break;
                case sf::Keyboard::Left:
                    m_view.move(-25, 0);
                    break;
                case sf::Keyboard::Right:
                    m_view.move(25, 0);
                    break;
                case sf::Keyboard::Up:
                    m_view.move(0, -25);
                    break;
                case sf::Keyboard::Down:
                    m_view.move(0, 25);
                    break;
                default:
                    break;
            }
        }
    }
    if (event.type == sf::Event::MouseMoved) {
        if (m_currentMode == Mode::LINE || m_currentMode == Mode::BREAKABLE) {
            // Highlight nearest grid vertex
            if (m_snapMode == SnapMode::AUTO || m_snapMode == SnapMode::GRID) {
                highlightGridVertex(window, event.mouseMove.x, event.mouseMove.y);
            }
            if (m_snapMode == SnapMode::AUTO || m_snapMode == SnapMode::LINE) {
                highlightNearestLinePoint(window, event.mouseMove.x, event.mouseMove.y);
            }
            if (m_currentInsertionLine.inactive == false) {
                if (m_currentNearestGridVertex.has_value()) {
                    m_currentInsertionLine.x1 = std::get<0>(m_currentNearestGridVertex.value());
                    m_currentInsertionLine.y1 = std::get<1>(m_currentNearestGridVertex.value());
                } else {
                    auto w = window.mapPixelToCoords({ static_cast<int>(event.mouseMove.x),
                                                       static_cast<int>(event.mouseMove.y) });
                    m_currentInsertionLine.x1 = w.x;
                    m_currentInsertionLine.y1 = w.y;
                }
            }
        } else {
            m_currentNearestGridVertex = std::nullopt;
        }
    }
    if (event.type == sf::Event::MouseButtonPressed) {
        if (event.mouseButton.button == sf::Mouse::Right) {
            m_currentInsertionLine.inactive = true;
        } else if (event.mouseButton.button == sf::Mouse::Left) {
            switch (m_currentMode) {
                case Mode::LINE:
                case Mode::BREAKABLE:
                    {
                        // Insert a new line
                        if (m_currentNearestGridVertex.has_value()
                            || m_snapMode == SnapMode::NONE) {
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
                                    m_lines.push_back(m_currentInsertionLine);
                                    m_dirty = true;
                                    // next line starts at the current line's end:
                                    m_currentInsertionLine.x0 = m_currentInsertionLine.x1;
                                    m_currentInsertionLine.y0 = m_currentInsertionLine.y1;
                                }
                            } else {
                                // else this is a new line
                                unsigned x;
                                unsigned y;
                                if (!m_currentNearestGridVertex.has_value()) {
                                    auto w = window.mapPixelToCoords(
                                        { static_cast<int>(event.mouseButton.x),
                                          static_cast<int>(event.mouseButton.y) });
                                    x = w.x;
                                    y = w.y;
                                } else {
                                    x = std::get<0>(m_currentNearestGridVertex.value());
                                    y = std::get<1>(m_currentNearestGridVertex.value());
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
                        // Check to see if there is a line under the cursor
                        auto line
                            = lineUnderCursor(window, event.mouseButton.x, event.mouseButton.y);
                        if (line.has_value()) {
                            m_highlightedLineIdx = line.value();
                        }
                        break;
                    }
                case Mode::START:
                    {
                        auto w = window.mapPixelToCoords({ static_cast<int>(event.mouseButton.x),
                                                           static_cast<int>(event.mouseButton.y) });
                        // if we're within r workspace units of an existing start object,
                        // we treat additional clicks as a way to modify the object's rotation
                        unsigned r = 20;
                        if (m_startPosition.has_value()
                            && ((m_startPosition.value().x > w.x - r
                                 && m_startPosition.value().x < w.x + r)
                                && (m_startPosition.value().y > w.y - r
                                    && m_startPosition.value().y < w.y + r))) {
                            m_startPosition.value().r += 15;
                            m_dirty = true;
                            if (m_startPosition.value().r >= 360) {
                                m_startPosition.value().r = 0;
                            }
                        } else {
                            m_startPosition
                                = { static_cast<unsigned>(w.x), static_cast<unsigned>(w.y), 0 };
                            m_dirty = true;
                        }
                        break;
                    }
                case Mode::EXIT:
                    {
                        auto w = window.mapPixelToCoords({ static_cast<int>(event.mouseButton.x),
                                                           static_cast<int>(event.mouseButton.y) });
                        m_exitPosition = std::make_pair(w.x, w.y);
                        m_dirty = true;
                        break;
                    }
                case Mode::FUEL:
                    {
                        auto w = window.mapPixelToCoords({ static_cast<int>(event.mouseButton.x),
                                                           static_cast<int>(event.mouseButton.y) });
                        // if we are within r workspace units of an existing fuel object, we treat
                        // this as a request to delete it instead of placing a new one
                        unsigned r = 20;
                        std::size_t idx = 0;
                        bool erased { false };
                        for (const auto& f : m_fuelObjects) {
                            if ((f.first > w.x - r && f.first < w.x + r)
                                && (f.second > w.y - r && f.second < w.y + r)) {
                                m_fuelObjects.erase(m_fuelObjects.begin() + idx);
                                erased = true;
                                m_dirty = true;
                                break;
                            }
                            ++idx;
                        }
                        if (!erased) {
                            m_fuelObjects.push_back(std::make_pair(w.x, w.y));
                            m_dirty = true;
                        }
                        break;
                    }
                default:
                    break;
            }
        }
    }
    if (event.mouseWheelScroll.wheel == sf::Mouse::VerticalWheel) {
        float amt = event.mouseWheelScroll.delta;
        if (std::abs(amt) > 0.1 && std::abs(amt) < 10.f) {
            if (amt > 0.f) {
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::LShift)) {
                    m_view.move(0, -25);
                } else {
                    auto c = m_view.getCenter();
                    auto m = window.mapPixelToCoords(
                        { event.mouseWheelScroll.x, event.mouseWheelScroll.y });
                    float dx = (m.x - c.x) * 0.05f;
                    float dy = (m.y - c.y) * 0.05f;
                    m_view.move(dx, dy);
                    zoomIn();
                }
            } else if (amt < 0.f) {
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::LShift)) {
                    m_view.move(0, 25);
                } else {
                    zoomOut();
                }
            }
        }
    }
    if (event.mouseWheelScroll.wheel == sf::Mouse::HorizontalWheel) {
        float amt = event.mouseWheelScroll.delta;
        if (std::abs(amt) > 0.1 && std::abs(amt) < 10.f) {
            if (amt > 0.f) {
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::LShift)) {
                    m_view.move(-25, 0);
                }
            } else if (amt < 0.f) {
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::LShift)) {
                    m_view.move(25, 0);
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
    }
}

void Level::zoomIn()
{
    if (m_view.getSize().x > 100.f) {
        m_view.zoom(0.95f);
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
    sf::Text txtMode;
    txtMode.setFont(m_font);
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
        default:
            break;
    }
    window.draw(txtMode);

    // Snapping mode:
    sf::Text txtSnap;
    txtSnap.setFont(m_font);
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
    auto w = window.mapPixelToCoords({ static_cast<int>(mouseX), static_cast<int>(mouseY) });

    unsigned x = static_cast<unsigned>(static_cast<double>(w.x) / 50.0 + 0.5) * 50.0;
    unsigned y = static_cast<unsigned>(static_cast<double>(w.y) / 50.0 + 0.5) * 50.0;
    if (x > 2000 || y > 2000) {
        m_currentNearestGridVertex = std::nullopt;
    } else {
        m_currentNearestGridVertex = std::tie(x, y);
    }
}

void Level::highlightNearestLinePoint(sf::RenderWindow& window, unsigned mouseX, unsigned mouseY)
{
    auto w = window.mapPixelToCoords({ static_cast<int>(mouseX), static_cast<int>(mouseY) });
    for (const auto& l : m_lines) {
        auto nearest = helperfunctions::closestPointOnLine(l.x0, l.y0, l.x1, l.y1, w.x, w.y, 5);
        if (nearest.has_value()) {
            m_currentNearestGridVertex = std::tie(nearest.value().first, nearest.value().second);
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
        ship.setPosition({ static_cast<float>(m_startPosition.value().x),
                           static_cast<float>(m_startPosition.value().y) });
        ship.setRotation(360 - m_startPosition.value().r);
        window.draw(ship);
    }
    if (m_exitPosition.has_value()) {
        sf::CircleShape c;
        c.setFillColor(sf::Color::Red);
        float r = 20.f;
        c.setRadius(r);
        c.setOrigin({ r, r });
        c.setPosition({ static_cast<float>(m_exitPosition.value().first),
                        static_cast<float>(m_exitPosition.value().second) });
        window.draw(c);
    }
    if (!m_fuelObjects.empty()) {
        for (const auto& p : m_fuelObjects) {
            sf::CircleShape c;
            c.setFillColor(sf::Color::Yellow);
            float r = 10.f;
            c.setRadius(r);
            c.setOrigin({ r, r });
            c.setPosition(static_cast<float>(p.first), static_cast<float>(p.second));
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

    float leftBound = viewSize.x / 3;
    float rightBound = 2000 - viewSize.x / 3;
    float topBound = viewSize.y / 3;
    float bottomBound = 2000 - viewSize.y / 3;

    if (viewCenter.x < leftBound) {
        m_view.setCenter(leftBound, viewCenter.y);
    }
    if (viewCenter.x > rightBound) {
        m_view.setCenter(rightBound, viewCenter.y);
    }
    if (viewCenter.y < topBound) {
        m_view.setCenter(viewCenter.x, topBound);
    }
    if (viewCenter.y > bottomBound) {
        m_view.setCenter(viewCenter.x, bottomBound);
    }
}

void Level::changeMode(Mode mode)
{
    m_currentMode = mode;
    if (m_currentMode == Mode::LINE) {
        m_currentInsertionLine.r = 255;
        m_currentInsertionLine.g = 0;
        m_currentInsertionLine.b = 0;
    } else if (m_currentMode == Mode::BREAKABLE) {
        m_currentInsertionLine.r = 255;
        m_currentInsertionLine.g = 150;
        m_currentInsertionLine.b = 50;
    }
}

void Level::cycleMode(bool backwards)
{
    m_highlightedLineIdx = std::nullopt;
    m_currentInsertionLine.inactive = true;
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
    m_currentNearestGridVertex = std::nullopt;
}

} // namespace