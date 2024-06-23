#include "level.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

namespace mgo {

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
        // When we get here, vec is a vector of all the items on the current
        // line.
        if (vec.size() == 0) {
            continue;
        }
        char c = vec[0][0];
        long bestTime;
        switch (c) {
        case '!': // timelimit, fuel, ship x, ship y, description
            // m_timeLimit = stoi(vec[1]);
            // bestTime = static_cast<long>(timeGetTenthBest(levelNum));
            // if (m_timeLimit > bestTime) {
            //     m_timeLimit = bestTime;
            // }
            // m_shipModel->setFuel(stod(vec[2]));
            // m_shipModel->setShipX(stod(vec[3]));
            // m_shipModel->setShipY(stod(vec[4]));
            // m_levelDescription = vec[5];
            break;
        case 'N': // New object, parameter 1 is type, parameter 2 appears unused
            // if (obj->GetGameShapeType() != GameShapeType::UNINITIALISED) {
            //     m_allDynamicGameShapes.push_back(std::move(obj));
            // }
            // obj = std::unique_ptr<GameShape>(new GameShape);
            // obj->SetGameShapeType(GameShapeType::NEUTRAL);
            // if (vec.size() > 1) {
            //     if (vec[1] == "OBSTRUCTION") {
            //         obj->SetGameShapeType(GameShapeType::OBSTRUCTION);
            //     } else if (vec[1] == "FUEL") {
            //         obj->SetGameShapeType(GameShapeType::FUEL);
            //     } else if (vec[1] == "PRISONER") {
            //         obj->SetGameShapeType(GameShapeType::PRISONER);
            //     } else if (vec[1] == "KEY") {
            //         obj->SetGameShapeType(GameShapeType::KEY);
            //     } else if (vec[1] == "EXIT") {
            //         obj->SetGameShapeType(GameShapeType::EXIT);
            //     }
            // }
            break;
        case 'L':
            // if (obj != nullptr && (vec.size() == 8 || vec.size() == 9)) {
            //     ShapeLine sl1 {
            //         stod(vec[1]), // x0
            //         stod(vec[2]), // y0
            //         stod(vec[3]), // x1
            //         stod(vec[4]), // y1
            //         static_cast<uint8_t>(stoi(vec[5])), // r
            //         static_cast<uint8_t>(stoi(vec[6])), // g
            //         static_cast<uint8_t>(stoi(vec[7])), // b
            //         255, // alpha
            //         1 // thickness
            //     };
            //     if (vec.size() == 9) {
            //         sl1.lineThickness = stoi(vec[8]);
            //     }
            //     obj->AddShapeLine(sl1);
            // }
            break;
        case 'P':
            // if (obj != nullptr && vec.size() == 3) {
            //     obj->SetPos(stoi(vec[1]), stoi(vec[2]));
            // }
            break;
        case 'T':
            // if (obj != nullptr && vec.size() == 5) {
            //     obj->makeFromText(vec[1], stoi(vec[2]), stoi(vec[3]), stoi(vec[4]), 255, 1);
            // }
            // if (obj != nullptr && vec.size() == 6) {
            //     obj->makeFromText(
            //         vec[1], stoi(vec[2]), stoi(vec[3]), stoi(vec[4]), 255, stoi(vec[5]));
            // }
            break;
        default:
            break;
        }
        currentLine.clear();
    }
    in.close();
    //if (obj->GetGameShapeType() != GameShapeType::UNINITIALISED) {
    //    m_allDynamicGameShapes.push_back(std::move(obj));
    //}
}

} // namespace