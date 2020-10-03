/*
 * list_tag.cpp
 * Copyright (C) 2012 - 2019 David Jolly
 * ----------------------
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <sstream>
#include "../../include/byte_stream.h"
#include "../../include/tag/compound_tag.h"
#include "../../include/tag/end_tag.h"


/*
 * Compound tag assignment operator
 */
compound_tag& compound_tag::operator=(const compound_tag& other) {

    // check for self
    if (this == &other)
        return *this;

    // assign attributes
    name = other.name;
    type = other.type;
    value = other.value;
    return *this;
}

/*
 * Compound tag equals operator
 */
bool compound_tag::operator==(const generic_tag& other) {

    // check for self
    if (this == &other)
        return true;

    // convert into same type
    const compound_tag* other_tag = dynamic_cast<const compound_tag*>(&other);
    if (!other_tag)
        return false;

    // check attributes
    if (name != other.name
        || type != other.type
        || value.size() != other_tag->value.size())
        return false;
    for (unsigned int i = 0; i < value.size(); ++i) {
        if (value.at(i) != other_tag->value.at(i))
            return false;
    }
    return true;
}

/*
 * Return a compound tag's data
 */
std::vector<char> compound_tag::get_data(bool list_ele) {
    int b = byte_stream::SWAP_ENDIAN;
    byte_stream stream(b);

    get_data(list_ele, stream);

    return stream.vbuf();
}

/*
 * Save a compound tag's data to a stream
 */
void compound_tag::get_data(bool list_ele, byte_stream& stream) {
    end_tag end;

    // form data representation
    if (!list_ele) {
        stream << (char) type;
        stream << (short) name.size();
        stream << name;
    }
    for (unsigned int i = 0; i < value.size(); ++i) {
        value.at(i)->get_data(false, stream);
    }

    stream << end.get_data(false);
}


/*
 * Return a compund tag's data size, equivalent to get_data().size(), but faster.
 */
unsigned int compound_tag::get_data_size(bool list_ele) {
    unsigned int total = 0; //nothing yet

    if (!list_ele) {
        total += 1 + 2 + static_cast<unsigned int>(name.size()); //1 for type, 2 for short size, and every symbol in the name.
    }

    for (unsigned int i = 0; i < value.size(); ++i) {
        total += value.at(i)->get_data_size(false);
    }  //get individual parts

    total += 1; //end tag

    return total;
}

/*
 * Return a string representation of a compound tag
 */
std::string compound_tag::to_string(unsigned int tab) {
    std::stringstream ss;

    // form string representation
    ss << generic_tag::to_string(tab) << " (" << value.size() << ") {";
    if (!value.empty()) {
        ss << std::endl;
        for (unsigned int i = 0; i < value.size(); ++i) {
            ss << value.at(i)->to_string(tab + 1) << std::endl;
        }
        generic_tag::append_tabs(tab, ss);
    }
    ss << "}";
    return ss.str();
}

/*
 * Returns the subtag with the given name. Returns NULL if not found.
 */
generic_tag* compound_tag::get_subtag(std::string name) {

    for (unsigned int i = 0; i < value.size(); i++) { //check every subtag
        if (value[i]->name == name) { //if names match, return
            return value[i];
        }
    }

    return NULL; //not found
}
