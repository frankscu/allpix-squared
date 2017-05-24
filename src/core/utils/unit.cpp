/**
 * @file
 * @brief Implementation of unit system
 *
 * @copyright MIT License
 */

#include "unit.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>

#include "core/utils/string.h"

using namespace allpix;

std::map<std::string, allpix::Units::UnitType> Units::unit_map_;

/**
 * @throws std::invalid_argument If the unit already exists
 *
 * Units should consist of only alphabetical characters. Units are converted to lowercase internally. All the defined units
 * should have unique values and it is not possible to redefine them.
 *
 * @note No explicit check is done for alphabetical units
 */
void Units::add(std::string str, allpix::Units::UnitType value) {
    // Do not distinguish between different case for units
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);

    // Find the unit
    if(unit_map_.find(str) != unit_map_.end()) {
        throw std::invalid_argument("unit " + str + " already defined");
    }
    unit_map_.emplace(str, value);
}

/**
 * @throws std::invalid_argument If the requested unit does not exist
 *
 * All units are converted to lowercase before finding their value in the internal database.
 */
allpix::Units::UnitType Units::getSingle(std::string str) {
    if(allpix::trim(str).empty()) {
        throw std::invalid_argument("empty unit is not defined");
    }
    // Do not distinguish between different case for units
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);

    // Find the unit
    auto iter = unit_map_.find(str);
    if(iter == unit_map_.end()) {
        throw std::invalid_argument("unit " + str + " not found");
    }
    return iter->second;
}

/**
 * @throws invalid_argument If one tries to get the value of an empty unit
 *
 * Units are combined by applying the multiplication operator * and the division operator / linearly. The first unit is
 * always multiplied, following common sense. Grouping units together within brackets or parentheses is not supported. Thus
 * any other character then a name of a unit, * or \ should lead to an error
 */
allpix::Units::UnitType Units::get(std::string str) {
    UnitType ret_value = 1;
    if(allpix::trim(str).empty()) {
        throw std::invalid_argument("empty unit is not defined");
    }

    // Go through all units
    char lst = '*';
    std::string unit;
    for(char ch : str) {
        if(ch == '*' || ch == '/') {
            if(lst == '*') {
                ret_value = getSingle(ret_value, unit);
                unit.clear();
            } else if(lst == '/') {
                ret_value = getSingleInverse(ret_value, unit);
                unit.clear();
            }
            lst = ch;
        } else {
            unit += ch;
        }
    }

    // Apply last unit
    if(lst == '*') {
        ret_value = getSingle(ret_value, unit);
    } else if(lst == '/') {
        ret_value = getSingleInverse(ret_value, unit);
    }
    return ret_value;
}

allpix::Units::UnitType Units::convert(UnitType inp, std::string unit) {
    std::transform(unit.begin(), unit.end(), unit.begin(), ::tolower);

    auto iter = unit_map_.find(unit);
    if(iter == unit_map_.end()) {
        throw std::invalid_argument("unit " + unit + " not found");
    }

    return (1.0l / iter->second) * inp;
}

/**
 * @throws std::invalid_argument If the list of units is empty
 *
 * The best unit is determined using the two rules below:
 * - If there exists at least one unit for which the value is larger than one, the unit with value nearest to one is chosen
 *   from all units with values larger than one
 * - Otherwise the unit is chosen that has a value as close as possible to one (from below)
 */
std::string Units::display(UnitType inp, std::initializer_list<std::string> units) {
    if(units.size() == 0) {
        throw std::invalid_argument("list of possible units cannot be empty");
    }

    std::ostringstream stream;
    if(std::fabs(inp) < std::numeric_limits<Units::UnitType>::epsilon()) {
        // Zero needs no unit
        stream << inp;
    } else {
        // Find best unit
        int best_exponent = std::numeric_limits<int>::min();
        std::string best_unit;
        for(auto& unit : units) {
            Units::UnitType value = convert(inp, unit);
            int exponent = 0;
            std::frexp(value, &exponent);
            if((best_exponent <= 0 && exponent > best_exponent) || (exponent > 0 && exponent < best_exponent)) {
                best_exponent = exponent;
                best_unit = unit;
            }
        }

        // Write unit
        stream << convert(inp, best_unit);
        stream << best_unit;
    }
    return stream.str();
}
std::string Units::display(UnitType inp, std::string unit) {
    return display(inp, {unit});
}
