#include "levelformat.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QString>

#include <cctype>
#include <format>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace mgo {
namespace levelformat {

namespace {

std::vector<std::string> split(const std::string& s, char delimiter)
{
    std::vector<std::string> result;
    std::string::size_type start = 0;
    for (;;) {
        const auto end = s.find(delimiter, start);
        result.push_back(s.substr(start, end - start));
        if (end == std::string::npos) {
            return result;
        }
        start = end + 1;
    }
}

// ---------------------------------------------------------------------------
// Reading JSON
// ---------------------------------------------------------------------------

[[noreturn]] void fail(const std::string& context, const std::string& message)
{
    throw std::runtime_error(context.empty() ? message : context + ": " + message);
}

double getNumber(const QJsonObject& obj, const char* key, const std::string& context)
{
    const QJsonValue value = obj.value(key);
    if (!value.isDouble()) {
        fail(context, value.isUndefined() ? std::format("missing \"{}\"", key)
                                          : std::format("\"{}\" must be a number", key));
    }
    return value.toDouble();
}

double getNumber(const QJsonObject& obj, const char* key, double defaultValue, const std::string& context)
{
    return obj.contains(key) ? getNumber(obj, key, context) : defaultValue;
}

std::optional<double>
getOptionalNumber(const QJsonObject& obj, const char* key, const std::string& context)
{
    if (!obj.contains(key)) {
        return std::nullopt;
    }
    return getNumber(obj, key, context);
}

QJsonObject getObject(const QJsonValue& value, const std::string& context)
{
    if (!value.isObject()) {
        fail(context, "must be an object");
    }
    return value.toObject();
}

// An optional array: missing means empty
QJsonArray getArray(const QJsonObject& obj, const char* key, const std::string& context)
{
    const QJsonValue value = obj.value(key);
    if (value.isUndefined()) {
        return {};
    }
    if (!value.isArray()) {
        fail(context, std::format("\"{}\" must be an array", key));
    }
    return value.toArray();
}

Polyline readPolyline(const QJsonValue& value, const std::string& context)
{
    const QJsonObject obj = getObject(value, context);
    if (!obj.value("points").isArray()) {
        fail(context, "missing \"points\" array");
    }
    Polyline polyline;
    const QJsonArray points = obj.value("points").toArray();
    for (qsizetype i = 0; i < points.size(); ++i) {
        const QJsonArray point = points[i].toArray();
        if (!points[i].isArray() || point.size() != 2 || !point[0].isDouble()
            || !point[1].isDouble()) {
            fail(context, std::format("points[{}] must be [x, y]", i));
        }
        polyline.points.push_back({ point[0].toDouble(), point[1].toDouble() });
    }
    if (polyline.points.size() < 2) {
        fail(context, "a polyline needs at least two points");
    }
    if (obj.contains("closed")) {
        if (!obj.value("closed").isBool()) {
            fail(context, "\"closed\" must be true or false");
        }
        polyline.closed = obj.value("closed").toBool();
    }
    polyline.thickness = getOptionalNumber(obj, "thickness", context);
    return polyline;
}

std::vector<Polyline> readPolylines(const QJsonObject& obj, const char* key, const std::string& context)
{
    std::vector<Polyline> polylines;
    const QJsonArray array = getArray(obj, key, context);
    for (qsizetype i = 0; i < array.size(); ++i) {
        polylines.push_back(readPolyline(array[i], std::format("{}{}[{}]", context, key, i)));
    }
    return polylines;
}

// ---------------------------------------------------------------------------
// Writing JSON
// ---------------------------------------------------------------------------

std::string number(double value)
{
    if (value == 0.0) {
        return "0"; // not "-0"
    }
    // Without a precision, std::format gives the shortest representation which
    // reads back as the same value, e.g. "706" or "0.3"
    return std::format("{}", value);
}

std::string quoted(const std::string& s)
{
    std::string result = "\"";
    for (const char c : s) {
        switch (c) {
            case '"':
                result += "\\\"";
                break;
            case '\\':
                result += "\\\\";
                break;
            case '\n':
                result += "\\n";
                break;
            case '\r':
                result += "\\r";
                break;
            case '\t':
                result += "\\t";
                break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    result += std::format("\\u{:04x}", static_cast<unsigned char>(c));
                } else {
                    result += c; // including UTF-8 multi-byte sequences
                }
        }
    }
    return result + "\"";
}

std::string polylineJson(const Polyline& polyline)
{
    std::string s = "{ \"points\": [";
    for (std::size_t i = 0; i < polyline.points.size(); ++i) {
        s += std::format(
            "{}[{}, {}]",
            i == 0 ? "" : ", ",
            number(polyline.points[i].x),
            number(polyline.points[i].y));
    }
    s += "]";
    if (polyline.closed) {
        s += ", \"closed\": true";
    }
    if (polyline.thickness.has_value()) {
        s += ", \"thickness\": " + number(*polyline.thickness);
    }
    return s + " }";
}

// Writes a list of items (already formatted, one per line) as the value of
// "key", at the given indentation
void writeList(
    std::ostringstream& out,
    const std::string& indent,
    const std::string& key,
    const std::vector<std::string>& items,
    bool& first)
{
    if (items.empty()) {
        return;
    }
    out << (first ? "" : ",\n") << indent << quoted(key) << ": [\n";
    for (std::size_t i = 0; i < items.size(); ++i) {
        out << indent << "  " << items[i] << (i + 1 < items.size() ? ",\n" : "\n");
    }
    out << indent << "]";
    first = false;
}

// ---------------------------------------------------------------------------
// The old format
// ---------------------------------------------------------------------------

double toDouble(const std::string& s, int lineNumber)
{
    try {
        std::size_t used = 0;
        const double value = std::stod(s, &used);
        if (used != s.size()) {
            throw std::invalid_argument(s);
        }
        return value;
    } catch (const std::logic_error&) {
        throw std::runtime_error(std::format("line {}: \"{}\" isn't a number", lineNumber, s));
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Public functions
// ---------------------------------------------------------------------------

LevelData read(const std::string& filename)
{
    std::ifstream in(filename, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Could not open " + filename);
    }
    std::ostringstream contents;
    contents << in.rdbuf();
    return parse(contents.str());
}

void write(const std::string& filename, const LevelData& level)
{
    std::ofstream out(filename, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw std::runtime_error("Could not open " + filename + " for writing");
    }
    out << toJson(level);
    out.close();
    if (!out) {
        throw std::runtime_error("Failed writing to " + filename);
    }
}

LevelData parse(const std::string& contents)
{
    // The formats are told apart by their first non-whitespace character
    for (const char c : contents) {
        if (c == '{') {
            return parseJson(contents);
        }
        if (c == '!') {
            return parseOldFormat(contents);
        }
        if (!std::isspace(static_cast<unsigned char>(c))) {
            break;
        }
    }
    throw std::runtime_error("This isn't a level file (or it's empty)");
}

LevelData parseJson(const std::string& contents)
{
    QJsonParseError error;
    const QJsonDocument doc
        = QJsonDocument::fromJson(QByteArray::fromStdString(contents), &error);
    if (doc.isNull()) {
        throw std::runtime_error(std::format(
            "Invalid JSON at character {}: {}", error.offset, error.errorString().toStdString()));
    }
    if (!doc.isObject()) {
        throw std::runtime_error("The level must be a JSON object");
    }
    const QJsonObject root = doc.object();
    if (!root.value("formatVersion").isDouble()) {
        throw std::runtime_error("Missing \"formatVersion\"");
    }
    const int version = root.value("formatVersion").toInt();
    if (version > FORMAT_VERSION) {
        throw std::runtime_error(std::format(
            "This level is in a newer format (version {}) than this program supports "
            "(version {})",
            version,
            FORMAT_VERSION));
    }

    LevelData level;
    if (root.contains("title")) {
        if (!root.value("title").isString()) {
            throw std::runtime_error("\"title\" must be a string");
        }
        level.title = root.value("title").toString().toStdString();
    }
    if (root.contains("ship")) {
        const QJsonObject obj = getObject(root.value("ship"), "ship");
        level.ship = Ship { getNumber(obj, "x", "ship"),
                            getNumber(obj, "y", "ship"),
                            getNumber(obj, "rotation", 0, "ship") };
    }
    if (root.contains("exit")) {
        const QJsonObject obj = getObject(root.value("exit"), "exit");
        level.exit = Exit { getNumber(obj, "x", "exit"),
                            getNumber(obj, "y", "exit"),
                            getNumber(obj, "rotation", 0, "exit"),
                            getOptionalNumber(obj, "thickness", "exit") };
    }
    const QJsonArray fuel = getArray(root, "fuel", "");
    for (qsizetype i = 0; i < fuel.size(); ++i) {
        const std::string context = std::format("fuel[{}]", i);
        const QJsonObject obj = getObject(fuel[i], context);
        level.fuel.push_back({ getNumber(obj, "x", context),
                               getNumber(obj, "y", context),
                               getOptionalNumber(obj, "thickness", context) });
    }
    level.walls = readPolylines(root, "walls", "");
    const QJsonArray breakables = getArray(root, "breakables", "");
    for (qsizetype i = 0; i < breakables.size(); ++i) {
        const std::string context = std::format("breakables[{}]", i);
        const QJsonObject obj = getObject(breakables[i], context);
        Breakable breakable { readPolylines(obj, "lines", context + ".") };
        if (breakable.lines.empty()) {
            fail(context, "missing \"lines\"");
        }
        level.breakables.push_back(std::move(breakable));
    }
    level.decorations = readPolylines(root, "decorations", "");
    const QJsonArray movingObjects = getArray(root, "movingObjects", "");
    for (qsizetype i = 0; i < movingObjects.size(); ++i) {
        const std::string context = std::format("movingObjects[{}]", i);
        const QJsonObject obj = getObject(movingObjects[i], context);
        MovingObject m;
        m.lines = readPolylines(obj, "lines", context + ".");
        if (m.lines.empty()) {
            fail(context, "missing \"lines\"");
        }
        m.speedX = getNumber(obj, "speedX", 0, context);
        m.rangeX = getNumber(obj, "rangeX", 0, context);
        m.speedY = getNumber(obj, "speedY", 0, context);
        m.rangeY = getNumber(obj, "rangeY", 0, context);
        m.spin = getNumber(obj, "spin", 0, context);
        m.gravity = getNumber(obj, "gravity", 0, context);
        if (obj.value("name").isString()) {
            m.name = obj.value("name").toString().toStdString();
        }
        if (obj.contains("path")) {
            const std::string pathContext = context + ".path";
            const QJsonObject pathObj = getObject(obj.value("path"), pathContext);
            Path path;
            const QJsonArray offsets = getArray(pathObj, "offsets", pathContext);
            for (qsizetype j = 0; j < offsets.size(); ++j) {
                const QJsonArray offset = offsets[j].toArray();
                if (!offsets[j].isArray() || offset.size() != 2 || !offset[0].isDouble()
                    || !offset[1].isDouble()) {
                    fail(pathContext, std::format("offsets[{}] must be [x, y]", j));
                }
                path.offsets.push_back({ offset[0].toDouble(), offset[1].toDouble() });
            }
            if (path.offsets.empty()) {
                fail(pathContext, "needs at least one offset");
            }
            path.speed = getNumber(pathObj, "speed", 0, pathContext);
            if (path.speed < 0) {
                fail(pathContext, "\"speed\" can't be negative");
            }
            if (pathObj.contains("loop")) {
                if (!pathObj.value("loop").isBool()) {
                    fail(pathContext, "\"loop\" must be true or false");
                }
                path.loop = pathObj.value("loop").toBool();
            }
            if (m.speedX != 0 || m.rangeX != 0 || m.speedY != 0 || m.rangeY != 0) {
                fail(context, "can't have both a \"path\" and speedX/rangeX/speedY/rangeY");
            }
            m.path = std::move(path);
        }
        level.movingObjects.push_back(std::move(m));
    }
    return level;
}

std::string toJson(const LevelData& level)
{
    std::ostringstream out;
    out << "{\n  \"formatVersion\": " << FORMAT_VERSION;
    if (!level.title.empty()) {
        out << ",\n  \"title\": " << quoted(level.title);
    }
    if (level.ship.has_value()) {
        out << std::format(
            ",\n  \"ship\": {{ \"x\": {}, \"y\": {}", number(level.ship->x), number(level.ship->y));
        if (level.ship->rotation != 0) {
            out << ", \"rotation\": " << number(level.ship->rotation);
        }
        out << " }";
    }
    if (level.exit.has_value()) {
        out << std::format(
            ",\n  \"exit\": {{ \"x\": {}, \"y\": {}", number(level.exit->x), number(level.exit->y));
        if (level.exit->rotation != 0) {
            out << ", \"rotation\": " << number(level.exit->rotation);
        }
        if (level.exit->thickness.has_value()) {
            out << ", \"thickness\": " << number(*level.exit->thickness);
        }
        out << " }";
    }
    bool first = false; // "formatVersion" has been written
    std::vector<std::string> items;
    for (const Fuel& f : level.fuel) {
        std::string item = std::format("{{ \"x\": {}, \"y\": {}", number(f.x), number(f.y));
        if (f.thickness.has_value()) {
            item += ", \"thickness\": " + number(*f.thickness);
        }
        items.push_back(item + " }");
    }
    writeList(out, "  ", "fuel", items, first);

    items.clear();
    for (const Polyline& p : level.walls) {
        items.push_back(polylineJson(p));
    }
    writeList(out, "  ", "walls", items, first);

    items.clear();
    for (const Breakable& b : level.breakables) {
        std::string item = "{ \"lines\": [";
        for (std::size_t i = 0; i < b.lines.size(); ++i) {
            item += (i == 0 ? "" : ", ") + polylineJson(b.lines[i]);
        }
        items.push_back(item + "] }");
    }
    writeList(out, "  ", "breakables", items, first);

    items.clear();
    for (const Polyline& p : level.decorations) {
        items.push_back(polylineJson(p));
    }
    writeList(out, "  ", "decorations", items, first);

    if (!level.movingObjects.empty()) {
        out << ",\n  \"movingObjects\": [\n";
        for (std::size_t i = 0; i < level.movingObjects.size(); ++i) {
            const MovingObject& m = level.movingObjects[i];
            out << "    {\n";
            bool firstField = true;
            std::vector<std::string> lines;
            for (const Polyline& p : m.lines) {
                lines.push_back(polylineJson(p));
            }
            writeList(out, "      ", "lines", lines, firstField);
            auto field = [&](const char* key, double value) {
                if (value != 0) {
                    out << ",\n      " << quoted(key) << ": " << number(value);
                }
            };
            field("speedX", m.speedX);
            field("rangeX", m.rangeX);
            field("speedY", m.speedY);
            field("rangeY", m.rangeY);
            if (m.path.has_value()) {
                out << ",\n      \"path\": { \"offsets\": [";
                for (std::size_t j = 0; j < m.path->offsets.size(); ++j) {
                    out << std::format(
                        "{}[{}, {}]",
                        j == 0 ? "" : ", ",
                        number(m.path->offsets[j].x),
                        number(m.path->offsets[j].y));
                }
                out << "], \"speed\": " << number(m.path->speed);
                if (m.path->loop) {
                    out << ", \"loop\": true";
                }
                out << " }";
            }
            field("spin", m.spin);
            field("gravity", m.gravity);
            if (m.name.has_value()) {
                out << ",\n      \"name\": " << quoted(*m.name);
            }
            out << "\n    }" << (i + 1 < level.movingObjects.size() ? ",\n" : "\n");
        }
        out << "  ]";
    }
    out << "\n}\n";
    return out.str();
}

LevelData parseOldFormat(const std::string& contents)
{
    // See amaze's levelformat.h for the format. Each "N~<type>~..." line starts
    // a new object, to which the following lines apply.
    LevelData level;
    enum class Type {
        NONE, // before the first object, or an unsupported type
        OBSTRUCTION,
        BREAKABLE,
        MOVING,
        EXIT,
        FUEL
    };
    Type type = Type::NONE;
    std::vector<Segment> segments; // the current object's lines
    MovingObject moving;
    std::optional<Point> position;
    std::optional<double> textThickness;
    std::vector<Segment> walls;

    // Adds the current object to the level
    auto finishObject = [&]() {
        switch (type) {
            case Type::OBSTRUCTION:
                walls.insert(walls.end(), segments.begin(), segments.end());
                break;
            case Type::BREAKABLE:
                if (!segments.empty()) {
                    level.breakables.push_back({ toPolylines(segments) });
                }
                break;
            case Type::MOVING:
                if (!segments.empty()) {
                    moving.lines = toPolylines(segments);
                    level.movingObjects.push_back(moving);
                }
                break;
            case Type::EXIT:
                if (position.has_value()) {
                    level.exit = Exit { position->x, position->y, 0, textThickness };
                }
                break;
            case Type::FUEL:
                if (position.has_value()) {
                    level.fuel.push_back({ position->x, position->y, textThickness });
                }
                break;
            case Type::NONE:
                break;
        }
        segments.clear();
        position.reset();
        textThickness.reset();
    };

    std::istringstream in(contents);
    std::string line;
    int lineNumber = 0;
    bool haveHeader = false;
    while (std::getline(in, line)) {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }
        const std::vector<std::string> f = split(line, '~');
        auto num = [&](std::size_t i) { return toDouble(f[i], lineNumber); };
        auto need = [&](std::size_t count) {
            if (f.size() < count) {
                throw std::runtime_error(std::format("line {}: too few fields", lineNumber));
            }
        };
        switch (line[0]) {
            case '!': // !~<time limit>~<fuel>~<x>~<y>~<angle>~<title>
                need(6);
                level.ship = Ship { num(3), num(4), num(5) };
                if (f.size() > 6) {
                    level.title = f[6];
                }
                haveHeader = true;
                break;
            case 'N': // N~<type>~<name>[~<moving object parameters>]
                need(2);
                finishObject();
                type = Type::NONE;
                if (f[1] == "OBSTRUCTION") {
                    type = Type::OBSTRUCTION;
                } else if (f[1] == "BREAKABLE") {
                    type = Type::BREAKABLE;
                } else if (f[1] == "EXIT") {
                    type = Type::EXIT;
                } else if (f[1] == "FUEL") {
                    type = Type::FUEL;
                } else if (f[1] == "MOVING") {
                    type = Type::MOVING;
                    need(8); // N~MOVING~name~dx~xmax~dy~ymax~rotation[~gravity]
                    moving = MovingObject {};
                    moving.speedX = num(3);
                    moving.rangeX = num(4);
                    moving.speedY = num(5);
                    moving.rangeY = num(6);
                    moving.spin = num(7);
                    moving.gravity = f.size() > 8 ? num(8) : 0;
                }
                // Other types (e.g. PRISONER, KEY) aren't used, and are dropped
                break;
            case 'L': // L~x0~y0~x1~y1~r~g~b[~thickness]
                {
                    need(8);
                    // A missing thickness means 1 (as in amaze); store it if
                    // it isn't the default for the object's type
                    const double thickness = f.size() > 8 ? num(8) : 1;
                    const double defaultThickness = type == Type::OBSTRUCTION
                        ? DEFAULT_WALL_THICKNESS
                        : type == Type::BREAKABLE ? DEFAULT_BREAKABLE_THICKNESS
                                                  : DEFAULT_MOVING_OBJECT_THICKNESS;
                    segments.push_back(
                        { { num(1), num(2) },
                          { num(3), num(4) },
                          thickness == defaultThickness ? std::nullopt
                                                        : std::optional(thickness) });
                    break;
                }
            case 'P': // P~x~y
                need(3);
                position = Point { num(1), num(2) };
                break;
            case 'T': // T~text~r~g~b[~thickness]
                {
                    need(5);
                    // A missing thickness means 6 (as in amaze)
                    const double thickness = f.size() > 5 ? num(5) : 6;
                    const double defaultThickness
                        = type == Type::FUEL ? DEFAULT_FUEL_THICKNESS : DEFAULT_EXIT_THICKNESS;
                    if (thickness != defaultThickness) {
                        textThickness = thickness;
                    }
                    break;
                }
            default:
                break;
        }
    }
    finishObject();
    if (!haveHeader) {
        throw std::runtime_error("Missing header (\"!...\") line");
    }
    level.walls = toPolylines(walls);
    return level;
}

std::vector<Polyline> toPolylines(const std::vector<Segment>& segments)
{
    std::vector<Polyline> polylines;
    std::vector<bool> used(segments.size(), false);
    for (std::size_t i = 0; i < segments.size(); ++i) {
        if (used[i]) {
            continue;
        }
        used[i] = true;
        const auto thickness = segments[i].thickness;
        std::vector<Point> points { segments[i].start, segments[i].end };
        // Finds an unused segment (of the same thickness) with an end at p,
        // and returns its other end
        auto takeSegmentAt = [&](const Point& p) -> std::optional<Point> {
            for (std::size_t j = 0; j < segments.size(); ++j) {
                if (used[j] || segments[j].thickness != thickness) {
                    continue;
                }
                if (segments[j].start == p) {
                    used[j] = true;
                    return segments[j].end;
                }
                if (segments[j].end == p) {
                    used[j] = true;
                    return segments[j].start;
                }
            }
            return std::nullopt;
        };
        while (auto next = takeSegmentAt(points.back())) {
            points.push_back(*next);
        }
        while (auto previous = takeSegmentAt(points.front())) {
            points.insert(points.begin(), *previous);
        }
        Polyline polyline { std::move(points), false, thickness };
        if (polyline.points.size() >= 4 && polyline.points.front() == polyline.points.back()) {
            polyline.points.pop_back();
            polyline.closed = true;
        }
        polylines.push_back(std::move(polyline));
    }
    return polylines;
}

std::vector<Segment> toSegments(const Polyline& polyline)
{
    std::vector<Segment> segments;
    const auto& p = polyline.points;
    for (std::size_t i = 0; i + 1 < p.size(); ++i) {
        segments.push_back({ p[i], p[i + 1], polyline.thickness });
    }
    if (polyline.closed && p.size() > 2) {
        segments.push_back({ p.back(), p.front(), polyline.thickness });
    }
    return segments;
}

} // namespace levelformat
} // namespace mgo
