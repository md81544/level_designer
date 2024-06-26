#include "level.h"
#include "helperfunctions.h"
#include <cstdint>
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
mgo::Level::Level()
{
    if (!m_font.loadFromFile("DroidSansMono.ttf")) {
        throw std::runtime_error("Could not load font file");
    }
    m_currentInsertionLine.inactive = true; // denotes "not active" in this case
    m_currentInsertionLine.r = 255;
    m_currentInsertionLine.g = 0;
    m_currentInsertionLine.b = 0;
}

void Level::load(const std::string& filename)
{
    std::ifstream in(filename);
    if (!in) {
        throw(std::runtime_error("Failed to load Level file " + filename));
    }
    std::string currentLine;
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
        case '!': // timelimit, fuel, ship x, ship y, description
            break;
        case 'N': // New object, parameter 1 is type, parameter 2 appears unused
            // We don't care about this
            break;
        case 'L': {
            unsigned int x0 = std::stoi(vec[1]);
            unsigned int y0 = std::stoi(vec[2]);
            unsigned int x1 = std::stoi(vec[3]);
            unsigned int y1 = std::stoi(vec[4]);
            uint8_t r = std::stoi(vec[5]);
            uint8_t g = std::stoi(vec[6]);
            uint8_t b = std::stoi(vec[7]);
            m_lines.push_back({ x0, y0, x1, y1, r, g, b, 1 });
            break;
        }
        case 'P': // position
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
    msgbox("Save File",
        "Do you want to save (to stdout) now?",
        [&](bool okPressed, const std::string&) {
            if (okPressed) {
                // Header
                // time limit, fuel, startX, startY, title
                std::cout << "!~0~0~125~1902~Title\n";
                std::cout << "N~OBSTRUCTION~foo\n";
                for (const auto& l : m_lines) {
                    if (!l.inactive) {
                        std::cout << "L~" << l.x0 << "~" << l.y0 << "~" << l.x1 << "~" << l.y1
                                  << "~255~0~0~2\n";
                    }
                }
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
        c.setFillColor(sf::Color::Yellow);
        c.setRadius(3.f);
        c.setOrigin({ 3.f, 3.f });
        float x = std::get<0>(m_currentNearestGridVertex.value());
        float y = std::get<1>(m_currentNearestGridVertex.value());
        auto [windowX, windowY] = convertWorkspaceToWindowCoords(x, y);
        c.setPosition(windowX, windowY);
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
    // clang-format off
    sf::Vertex line[] = {
        sf::Vertex(sf::Vector2f(l.x0 * m_zoomLevel - m_originX, l.y0 * m_zoomLevel - m_originY)),
        sf::Vertex(sf::Vector2f(l.x1 * m_zoomLevel - m_originX, l.y1 * m_zoomLevel - m_originY))
        };
    // clang-format on
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
    for (unsigned int n = 0; n <= 2000; n += 50) {
        drawLine(window, { n, 0, n, 2000, 0, 100, 0, 1 }, std::nullopt);
        drawLine(window, { 0, n, 2000, n, 0, 100, 0, 1 }, std::nullopt);
    }
}

std::optional<std::size_t> mgo::Level::lineUnderCursor(unsigned int mouseX, unsigned int mouseY)
{
    // Note mouseX and mouseY are *window* coordinates, these need to be converted into
    // workspace coordinates.
    // We define four lines which bound the cursor in a diamond shape, and then check each
    // of these to see if they intersect any line on the workspace.
    // A positive origin e.g. 10,10 means the top left of the window is at, say, 10,10 on
    // the workspace, i.e. the workspace is slightly off screen to the left.
    auto [wx, wy] = convertWindowToWorkspaceCoords(mouseX, mouseY);
    std::size_t idx = 0;
    for (const auto& l : m_lines) {
        if (helperfunctions::doLinesIntersect(wx - 10, wy, wx, wy - 10, l.x0, l.y0, l.x1, l.y1)) {
            return idx;
        }
        if (helperfunctions::doLinesIntersect(wx, wy - 10, wx + 10, wy, l.x0, l.y0, l.x1, l.y1)) {
            return idx;
        }
        if (helperfunctions::doLinesIntersect(wx + 10, wy, wx, wy + 10, l.x0, l.y0, l.x1, l.y1)) {
            return idx;
        }
        if (helperfunctions::doLinesIntersect(wx, wy + 10, wx - 10, wy, l.x0, l.y0, l.x1, l.y1)) {
            return idx;
        }
        ++idx;
    }
    return std::nullopt;
}

std::tuple<unsigned int, unsigned int>
mgo::Level::convertWindowToWorkspaceCoords(unsigned int windowX, unsigned int windowY)
{
    unsigned int x = (windowX + m_originX) / m_zoomLevel;
    unsigned int y = (windowY + m_originY) / m_zoomLevel;
    return { x, y };
}

std::tuple<unsigned int, unsigned int>
mgo::Level::convertWorkspaceToWindowCoords(unsigned int workspaceX, unsigned int workspaceY)
{
    unsigned int x = workspaceX * m_zoomLevel - m_originX;
    unsigned int y = workspaceY * m_zoomLevel - m_originY;
    return { x, y };
}

void mgo::Level::processEvent(sf::RenderWindow& window, const sf::Event& event)
{
    if (event.type == sf::Event::Closed) {
        window.close();
    }
    if (m_isDialogActive) {
        // if a dialog is active then we respond differently to events:
        // TODO
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
        switch (event.key.code) {
        case sf::Keyboard::Equal:
            m_zoomLevel *= 1.05f;
            break;
        case sf::Keyboard::Hyphen:
            m_zoomLevel *= 0.95f;
            break;
        case sf::Keyboard::Q:
            msgbox("Quit", "Are you sure?", [&window](bool yes, const std::string) {
                if (yes) {
                    window.close();
                }
            });
            break;
        case sf::Keyboard::S:
            save();
            break;
        case sf::Keyboard::I:
            m_insertMode = !m_insertMode;
            if (m_insertMode) {
                m_highlightedLineIdx = std::nullopt;
            } else {
                m_currentInsertionLine.inactive = true;
            }
            break;
        case sf::Keyboard::BackSpace:
        case sf::Keyboard::Delete:
        case sf::Keyboard::X:
            if (m_highlightedLineIdx.has_value()) {
                m_lines[m_highlightedLineIdx.value()].inactive = true;
                m_highlightedLineIdx = std::nullopt;
            }
            break;
        case sf::Keyboard::Escape:
            m_currentInsertionLine.inactive = true;
            break;
        default:
            break;
        }
    }
    if (event.type == sf::Event::MouseMoved) {
        if (m_insertMode) {
            // Highlight nearest grid vertex
            highlightGridVertex(event.mouseMove.x, event.mouseMove.y);
            if (m_currentInsertionLine.inactive == false) {
                if (m_currentNearestGridVertex.has_value()) {
                    m_currentInsertionLine.x1 = std::get<0>(m_currentNearestGridVertex.value());
                    m_currentInsertionLine.y1 = std::get<1>(m_currentNearestGridVertex.value());
                } else {
                    auto [toX, toY]
                        = convertWindowToWorkspaceCoords(event.mouseMove.x, event.mouseMove.y);
                    m_currentInsertionLine.x1 = toX;
                    m_currentInsertionLine.y1 = toY;
                }
            }
        } else {
            m_currentNearestGridVertex = std::nullopt;
        }
    }
    if (event.type == sf::Event::MouseButtonPressed) {
        if (event.mouseButton.button == sf::Mouse::Left) {
            if (m_insertMode) {
                // Insert a new line
                if (m_currentNearestGridVertex.has_value()) {
                    // Are we in the middle of drawing a line? If so, add the current
                    // insertion line into the vector
                    if (!m_currentInsertionLine.inactive) {
                        if (m_currentInsertionLine.x0 != m_currentInsertionLine.x1
                            || m_currentInsertionLine.y0 != m_currentInsertionLine.y1) {
                            m_lines.push_back(m_currentInsertionLine);
                            // next line starts at the current line's end:
                            m_currentInsertionLine.x0 = m_currentInsertionLine.x1;
                            m_currentInsertionLine.y0 = m_currentInsertionLine.y1;
                        }
                    } else {
                        // else this is a new line
                        unsigned int x = std::get<0>(m_currentNearestGridVertex.value());
                        unsigned int y = std::get<1>(m_currentNearestGridVertex.value());
                        m_currentInsertionLine.x0 = x;
                        m_currentInsertionLine.y0 = y;
                        m_currentInsertionLine.x1 = x;
                        m_currentInsertionLine.y1 = y;
                        m_currentInsertionLine.inactive = false;
                    }
                }
            } else {
                // Check to see if there is a line under the cursor
                auto line = lineUnderCursor(event.mouseButton.x, event.mouseButton.y);
                if (line.has_value()) {
                    m_highlightedLineIdx = line.value();
                } else {
                    // Else the user has clicked in an empty space so we can offer the
                    // option to place an object (start, exit, fuel) here
                    msgbox("Place Object",
                        "Do you want to place an object?\n"
                        "Press S for Start, E for End, F for fuel",
                        [](bool, const std::string) {
                            // TODO: stuff
                        });
                }
            }
        }
    }
    if (event.type == sf::Event::MouseButtonReleased) {
        if (event.mouseButton.button == sf::Mouse::Left) {
            // std::cout << "The left button was released" << std::endl;
            // std::cout << "  mouse x: " << event.mouseButton.x << std::endl;
            // std::cout << "  mouse y: " << event.mouseButton.y << std::endl;
        }
    }
    if (event.mouseWheelScroll.wheel == sf::Mouse::HorizontalWheel) {
        float amt = event.mouseWheelScroll.delta;
        if (std::abs(amt) > 0.1 && std::abs(amt) < 10.f) {
            if (amt < 0.f) {
                m_originX += 10;
            } else if (amt > 0.f) {
                m_originX -= 10;
            }
        }
    }
    if (event.mouseWheelScroll.wheel == sf::Mouse::VerticalWheel) {
        float amt = event.mouseWheelScroll.delta;
        if (std::abs(amt) > 0.1 && std::abs(amt) < 10.f) {
            if (amt < 0.f) {
                m_originY += 10;
            } else if (amt > 0.f) {
                m_originY -= 10;
            }
        }
    }
}

bool mgo::Level::msgbox(const std::string& title,
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

std::string mgo::Level::inputbox(const std::string& /* title */, const std::string& /* message */)
{
    m_isDialogActive = true;
    return std::string("TODO");
}

void mgo::Level::displayMode(sf::RenderWindow& window)
{
    sf::Text t;
    t.setFont(m_font);
    t.setFillColor(sf::Color::Cyan);
    t.setCharacterSize(14);
    t.setPosition({ 5.f, 5.f });
    if (m_insertMode) {
        t.setString("INSERT");
    } else {
        t.setString("EDIT");
    }
    window.draw(t);
}

void mgo::Level::highlightGridVertex(unsigned int mouseX, unsigned int mouseY)
{
    auto [wx, wy] = convertWindowToWorkspaceCoords(mouseX, mouseY);
    unsigned int x = static_cast<unsigned int>(static_cast<double>(wx) / 50.0 + 0.5) * 50.0;
    unsigned int y = static_cast<unsigned int>(static_cast<double>(wy) / 50.0 + 0.5) * 50.0;
    if (x > 2000 || y > 2000) {
        m_currentNearestGridVertex = std::nullopt;
    } else {
        m_currentNearestGridVertex = std::tie(x, y);
    }
}

} // namespace