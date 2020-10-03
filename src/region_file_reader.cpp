/*
* region_file_reader.cpp
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
#include <vector>
#include <iostream>
#include "../include/chunk_info.h"
#include "../include/chunk_tag.h"
#include "../include/compression.h"
#include "../include/region_dim.h"
#include "../include/region_file_reader.h"
#include "../include/tag/byte_tag.h"
#include "../include/tag/byte_array_tag.h"
#include "../include/tag/compound_tag.h"
#include "../include/tag/double_tag.h"
#include "../include/tag/end_tag.h"
#include "../include/tag/float_tag.h"
#include "../include/tag/generic_tag.h"
#include "../include/tag/int_tag.h"
#include "../include/tag/int_array_tag.h"
#include "../include/tag/list_tag.h"
#include "../include/tag/long_tag.h"
#include "../include/tag/long_array_tag.h"
#include "../include/tag/short_tag.h"
#include "../include/tag/string_tag.h"
#include <cassert>

/*
 * Region file reader assignment operator
 */
region_file_reader& region_file_reader::operator=(const region_file_reader& other) {

    // check for self
    if (this == &other)
        return *this;

    // assign attributes
    path = other.path;
    reg = other.reg;
    return *this;
}

/*
 * Region file reader equals operator
 */
bool region_file_reader::operator==(const region_file_reader& other) {

    // check for self
    if (this == &other)
        return true;

    // check attributes
    return path == other.path
           && reg == other.reg;
}

/*
 * Returns a region biome value at a given x, z & b coord
 */
char region_file_reader::get_biome_at(unsigned int x, unsigned int z, unsigned int b_x, unsigned int b_z) {
    std::vector<generic_tag*> biome;
    unsigned int pos = z * region_dim::CHUNK_WIDTH + x,
        b_pos = b_z * region_dim::BLOCK_WIDTH + b_x;

    // check coordinates
    if (pos >= region_dim::CHUNK_COUNT
        || b_pos >= region_dim::BLOCK_COUNT)
        throw std::out_of_range("coordinates out-of-range");

    // collect biome tags
    biome = reg.get_tag_at(pos).get_sub_tag_by_name("Biomes");
    if (biome.empty())
        return 0;
    return static_cast<byte_array_tag*>(biome.at(0))->at(b_pos);
}

/*
 * Returns a region's biomes at a given x, z coord
 */
std::vector<char> region_file_reader::get_biomes_at(unsigned int x, unsigned int z) {
    std::vector<char> biomes;
    std::vector<generic_tag*> biome;
    unsigned int pos = z * region_dim::CHUNK_WIDTH + x;

    // check coordinates
    if (pos >= region_dim::CHUNK_COUNT)
        throw std::out_of_range("coordinates out-of-range");

    // collect biome tags
    biome = reg.get_tag_at(pos).get_sub_tag_by_name("Biomes");
    if (biome.empty())
        return biomes;
    return static_cast<byte_array_tag*>(biome.at(0))->get_value();
}

/*
 * Returns a region block value at given x, z & b coord
 */
int
region_file_reader::get_block_at(unsigned int x, unsigned int z, unsigned int b_x, unsigned int b_y, unsigned int b_z) {
    std::vector<char> sect_blocks;
    std::vector<generic_tag*> section;
    unsigned int pos = z * region_dim::CHUNK_WIDTH + x, sect;

    // check coordinates
    if (pos >= region_dim::CHUNK_COUNT)
        throw std::out_of_range("coordinates out-of-range");
    section = reg.get_tag_at(pos).get_sub_tag_by_name("Blocks");

    // return an air block if no blocks exists in a given chunk
    if (section.empty())
        return 0;

    // check if y coord exists in file
    if ((sect = b_y / region_dim::BLOCK_WIDTH) >= section.size())
        return 0;

    // retrieve block from the appropriate section
    sect_blocks = static_cast<byte_array_tag*>(section.at(sect))->get_value();

    // TODO: check for "AddBlock" tag and apply to block id

    return sect_blocks.at(
        ((b_y % region_dim::BLOCK_WIDTH) * region_dim::BLOCK_WIDTH + b_z) * region_dim::BLOCK_WIDTH + b_x);
}

std::shared_ptr<Chunk> region_file_reader::getChunkAt(int32_t chunkX, int32_t chunkZ) {
    std::array<int32_t, 2> pos{chunkX, chunkZ};
    std::shared_ptr<Chunk> chunk = std::make_shared<Chunk>(pos);
    readChunk(chunk);
    return chunk;
}

void region_file_reader::readChunk(std::shared_ptr<Chunk> chunk) {
    std::vector<generic_tag*> sections;
    int32_t x = chunk->getPos()[0];
    int32_t z = chunk->getPos()[1];
    unsigned int pos = z * region_dim::CHUNK_WIDTH + x;

    // For debug
    std::vector<generic_tag*> xPosEntries = reg.get_tag_at(pos).get_sub_tag_by_name("xPos");
    std::vector<generic_tag*> zPosEntries = reg.get_tag_at(pos).get_sub_tag_by_name("zPos");

    if (xPosEntries.empty() || zPosEntries.empty()) {
        read_chunk(x, z);
        xPosEntries = reg.get_tag_at(pos).get_sub_tag_by_name("xPos");
        zPosEntries = reg.get_tag_at(pos).get_sub_tag_by_name("zPos");
        if (xPosEntries.empty() || zPosEntries.empty()) {
            throw std::out_of_range("Could not loat chunk at " + std::to_string(x) + "|" + std::to_string(z));
        }
    }

    sections = reg.get_tag_at(pos).get_sub_tag_by_name("Sections");

    if (sections.size() != 1) {
        throw std::out_of_range("Number of sections is not equal to 1");
    }

    int xPos = static_cast<int_tag*>(xPosEntries.at(0))->get_value();
    int32_t blockOffsetX = xPos * 16;
    int zPos = static_cast<int_tag*>(zPosEntries.at(0))->get_value();
    int32_t blockOffsetZ = zPos * 16;

    list_tag* subChunk = static_cast<list_tag*>(sections[0]);
    for (int i = 0; i < subChunk->size(); ++i) {
        for (int x = 0; x < 16; ++x) {
            for (int z = 0; z < 16; ++z) {
                compound_tag* subChunkEntry = static_cast<compound_tag*>(subChunk->at(i));
                addSubchunk(subChunkEntry, x, z,{blockOffsetX, blockOffsetZ}, chunk);
            }
        }


    }//for sectionEntries (aka subchunks)
}

void region_file_reader::addSubchunk(compound_tag* sectionEntry, int32_t chunkX, int32_t chunkZ, std::array<int32_t, 2> blockOffset,
                                     std::shared_ptr<Chunk> chunk) {

    byte_tag* yValue = static_cast<byte_tag*>(sectionEntry->get_subtag("Y"));
    int yPos = yValue->get_value();

    generic_tag* blockStates = sectionEntry->get_subtag("BlockStates");
    if (!blockStates) {
        return; // air-only subchunks are empty
    }
    generic_tag* palette = sectionEntry->get_subtag("Palette");

    std::vector<int64_t> blockStateEntries = static_cast<long_array_tag*>(blockStates)->get_value();
    std::vector<generic_tag*> paletteEntries = static_cast<list_tag*>(palette)->get_value();

    // Iterate through block states, calculate palette indices and query indices value

    // Calculate the number of required bits. Depends on the size of the palette
    int bitPerIndex = ceil(log2(paletteEntries.size()));
    if (bitPerIndex < 4) {
        bitPerIndex = 4; // 4 is the minimal size
    }

    for (int subChunkCounter = 0; subChunkCounter < 16; ++subChunkCounter) { // From bottom to top
        uint64_t blockNumber = 16 * 16 * subChunkCounter + 16 * chunkZ + chunkX;

        uint64_t paletteIndex = getPaletteIndex(blockStateEntries, blockNumber, bitPerIndex);

        if (paletteIndex >= paletteEntries.size()) {
            throw std::out_of_range("Palette index out-of-range");
        }
        generic_tag* compountEntry = static_cast<compound_tag*>(paletteEntries[paletteIndex])->get_subtag("Name");
        std::string name = static_cast<string_tag*>(compountEntry)->get_value();
        name.erase(0, 10); // Erase minecraft:

        int realY = yPos * 16 + subChunkCounter;
        Block block{name, {chunkX + blockOffset[0], realY, chunkZ + blockOffset[1]}};
        chunk->addBlock(block);

    }
}
// ###############################################################################################################################

std::vector<Block> region_file_reader::get_blocks_at(unsigned int chunkX, unsigned int chunkZ, unsigned int blockX, unsigned int BlockZ) {
    std::vector<generic_tag*> sections;
    unsigned int pos = chunkZ * region_dim::CHUNK_WIDTH + chunkX;

    std::vector<Block> foundBlocks;

    // For debug
    std::vector<generic_tag*> xPosEntries = reg.get_tag_at(pos).get_sub_tag_by_name("xPos");
    int xPos = static_cast<int_tag*>(xPosEntries.at(0))->get_value();
    int blockIdX = xPos * 16;
    std::vector<generic_tag*> zPosEntries = reg.get_tag_at(pos).get_sub_tag_by_name("zPos");
    int zPos = static_cast<int_tag*>(zPosEntries.at(0))->get_value();
    int blockIdZ = zPos * 16;

    //std::cout << "In " << xPos << " " << zPos << std::endl;

    sections = reg.get_tag_at(pos).get_sub_tag_by_name("Sections");

    if (sections.size() != 1) {
        throw std::out_of_range("Number of sections is not equal to 1");
    }


    list_tag* subChunk = static_cast<list_tag*>(sections[0]);
    for (int i = 0; i < subChunk->size(); ++i) {
        compound_tag* subChunkEntry = static_cast<compound_tag*>(subChunk->at(i));
        // std::vector<Block> subchunkBlocks = get_blocks_from_subchunk(subChunkEntry, blockX, BlockZ);

        //for (Block& b : subchunkBlocks) {
        //    std::array<int, 3> blockPosInChunk = b.getPos();
        //    b.setPos(blockPosInChunk[0] + blockIdX, blockPosInChunk[1], blockPosInChunk[2] + blockIdZ);
        //}
        //foundBlocks.insert(foundBlocks.end(), subchunkBlocks.begin(), subchunkBlocks.end());
    }//for sectionEntries (aka subchunks)

    return foundBlocks;
}


std::vector<Block> region_file_reader::get_blocks_in_range(unsigned int lowerX, unsigned int upperX, unsigned int lowerZ, unsigned int upperZ) {
    std::vector<Block> foundBlocks;
    foundBlocks.reserve((upperX - lowerX) * (upperZ - lowerZ) * 16 * 16 * 16);
    for (unsigned int x = lowerX; x < upperX; ++x) {
        for (unsigned int z = lowerZ; z < upperZ; ++z) {
            get_blocks_at(x, z, foundBlocks);
        }
    }
    //foundBlocks.shrink_to_fit();
    return foundBlocks;
}

/*
 * Returns a region's blocks at a given x, z coord
 */
void region_file_reader::get_blocks_at(unsigned int x, unsigned int z, std::vector<Block>& foundBlocks) {
    std::vector<generic_tag*> sections;
    unsigned int pos = z * region_dim::CHUNK_WIDTH + x;

    // For debug
    std::vector<generic_tag*> xPosEntries = reg.get_tag_at(pos).get_sub_tag_by_name("xPos");
    std::vector<generic_tag*> zPosEntries = reg.get_tag_at(pos).get_sub_tag_by_name("zPos");

    if (xPosEntries.empty() || zPosEntries.empty()) {
        read_chunk(x, z);
        xPosEntries = reg.get_tag_at(pos).get_sub_tag_by_name("xPos");
        zPosEntries = reg.get_tag_at(pos).get_sub_tag_by_name("zPos");
        if (xPosEntries.empty() || zPosEntries.empty()) {
            throw std::out_of_range("Could not loat chunk at " + std::to_string(x) + "|" + std::to_string(z));
        }
    }

    int xPos = static_cast<int_tag*>(xPosEntries.at(0))->get_value();
    int blockIdX = xPos * 16;
    int zPos = static_cast<int_tag*>(zPosEntries.at(0))->get_value();
    int blockIdZ = zPos * 16;

    //std::cout << "In " << xPos  << " " << zPos << std::endl;

    sections = reg.get_tag_at(pos).get_sub_tag_by_name("Sections");

    if (sections.size() != 1) {
        throw std::out_of_range("Number of sections is not equal to 1");
    }


    list_tag* subChunk = static_cast<list_tag*>(sections[0]);
    for (int i = 0; i < subChunk->size(); ++i) {
        for (int x = 0; x < 16; ++x) {
            for (int z = 0; z < 16; ++z) {
                compound_tag* subChunkEntry = static_cast<compound_tag*>(subChunk->at(i));
                get_blocks_from_subchunk(subChunkEntry, x, z, blockIdX, blockIdZ, foundBlocks);
            }
        }


    }//for sectionEntries (aka subchunks)
}

void region_file_reader::get_blocks_from_subchunk(compound_tag* sectionEntry, uint64_t chunkX, uint64_t chunkZ, unsigned int blockX,
                                                  unsigned int blockZ, std::vector<Block>& blockList) {

    byte_tag* yValue = static_cast<byte_tag*>(sectionEntry->get_subtag("Y"));
    int yPos = yValue->get_value();

    generic_tag* blockStates = sectionEntry->get_subtag("BlockStates");
    if (!blockStates) {
        return; // air-only subchunks are empty
    }
    generic_tag* palette = sectionEntry->get_subtag("Palette");

    std::vector<int64_t> blockStateEntries = static_cast<long_array_tag*>(blockStates)->get_value();
    std::vector<generic_tag*> paletteEntries = static_cast<list_tag*>(palette)->get_value();

    // Iterate through block states, calculate palette indices and query indices value
    int bitPerIndex = static_cast<int>(blockStateEntries.size() * 64 / 4096);

    for (uint64_t y = 0; y < 16; ++y) {
        uint64_t blockNumber = 16 * 16 * y + 16 * chunkZ + chunkX;
        uint64_t indexOffset = blockNumber * bitPerIndex;

        uint64_t paletteIndex = getPaletteIndex(blockStateEntries, indexOffset, bitPerIndex);

        if (paletteIndex >= paletteEntries.size()) {
            throw std::out_of_range("Palette index out-of-range");
        }
        generic_tag* compountEntry = static_cast<compound_tag*>(paletteEntries[paletteIndex])->get_subtag("Name");
        std::string name = static_cast<string_tag*>(compountEntry)->get_value();
        name.erase(0, 10); // Erase minecraft:

        int realY = yPos * 16 + y;

        int blockPosX = chunkX + blockX;
        int blockPosZ = chunkZ + blockZ;
        Block block(name, { blockPosX, realY, blockPosZ });
        blockList.push_back(block);
    }
}

uint64_t region_file_reader::getPaletteIndex(std::vector<int64_t> const& blockStateEntries, uint64_t blockNumber, unsigned int bitPerIndex) {
    uint64_t paletteIndex = 0;

    // Slight changes in 1.16:
    if (bitPerIndex % 2 != 0) {
        // Highest bits will not be used, eg at 5 bits the highest 4 bit wont be used.
        uint8_t numberOfStatesPerEntry = 64 / bitPerIndex;
        uint16_t vectorIndexForBlockOfInterest = (blockNumber / numberOfStatesPerEntry);

        uint16_t stateNumberInEntry = blockNumber - vectorIndexForBlockOfInterest * numberOfStatesPerEntry;
        uint16_t bitOffsetInEntry = stateNumberInEntry * bitPerIndex;
        unsigned int lowerBound = bitOffsetInEntry;
        unsigned int upperBound = lowerBound + bitPerIndex;

        paletteIndex = getBits(blockStateEntries[vectorIndexForBlockOfInterest], lowerBound, upperBound);
    }
    else {
        unsigned int indexOfInterest = (blockNumber * bitPerIndex) / 64;
        unsigned int lowerBound = (blockNumber * bitPerIndex) % 64;
        unsigned int upperBound = lowerBound + bitPerIndex;
       
        if (upperBound > 64) {
            // The upper bound is in the next index
            uint64_t lowerBits = getBits(blockStateEntries[indexOfInterest], lowerBound,
                64); // Get remaining bits in current index
            uint64_t higherBits = getBits(blockStateEntries[indexOfInterest + 1], 0,
                upperBound - 64); // And the one from the next entry

            paletteIndex = (higherBits << (64 - lowerBound) | lowerBits); // Combine values
        }
        else {
            paletteIndex = getBits(blockStateEntries[indexOfInterest], lowerBound, upperBound);
        }
    }

    return paletteIndex;
}

/*
 * Returns a region's chunk tag at a given x, z coord
 */
chunk_tag& region_file_reader::get_chunk_tag_at(unsigned int x, unsigned int z) {
    unsigned int pos = z * region_dim::CHUNK_WIDTH + x;

    // check coordinates
    if (pos >= region_dim::CHUNK_COUNT)
        throw std::out_of_range("coordinates out-of-range");
    return reg.get_tag_at(pos);
}

/*
 * Returns a region height value at a given x, z & b coord
 */
int region_file_reader::get_height_at(unsigned int x, unsigned int z, unsigned int b_x, unsigned int b_z) {
    std::vector<generic_tag*> height;
    unsigned int pos = z * region_dim::CHUNK_WIDTH + x,
        b_pos = b_z * region_dim::BLOCK_WIDTH + b_x;

    // check coordinates
    if (pos >= region_dim::CHUNK_COUNT
        || b_pos >= region_dim::BLOCK_COUNT)
        throw std::out_of_range("coordinates out-of-range");

    // collect biome tags
    height = reg.get_tag_at(pos).get_sub_tag_by_name("HeightMap");
    if (height.empty())
        return 0;
    return static_cast<int_array_tag*>(height.at(0))->at(b_pos);
}

/*
 * Returns a region's height map at a given x, z coord
 */
std::vector<int> region_file_reader::get_heightmap_at(unsigned int x, unsigned int z) {
    std::vector<int> heights;
    std::vector<generic_tag*> height;
    unsigned int pos = z * region_dim::CHUNK_WIDTH + x;

    // check coordinates
    if (pos >= region_dim::CHUNK_COUNT)
        throw std::out_of_range("coordinates out-of-range");

    // collect biome tags
    height = reg.get_tag_at(pos).get_sub_tag_by_name("HeightMap");
    if (height.empty())
        return heights;
    return static_cast<int_array_tag*>(height.at(0))->get_value();
}

/*
 * Return a region's filled status
 */
bool region_file_reader::is_filled(unsigned int x, unsigned int z) {
    unsigned int pos = z * region_dim::CHUNK_WIDTH + x;

    // check coordinates
    if (pos >= region_dim::CHUNK_COUNT)
        throw std::out_of_range("coordinates out-of-range");
    return reg.is_filled(pos);
}

/*
 * Read a tag from data
 */
generic_tag* region_file_reader::parse_tag(byte_stream& stream, bool is_list, char list_type) {
    char type;
    short name_len;
    std::string name;
    generic_tag* tag = NULL, * sub_tag = NULL;

    // check if stream is good
    if (!stream.good())
        throw std::runtime_error("Unexpected end of stream");

    // parse tag header
    if (is_list)
        type = list_type;
    else {
        type = read_value<char>(stream);
        if (type != generic_tag::END) {
            name_len = read_value<short>(stream);
            for (short i = 0; i < name_len; ++i) {
                name += read_value<char>(stream);
            }
        }
    }

    // parse tag based off type
    switch (type) {
        case generic_tag::END:
            tag = new end_tag;
            break;
        case generic_tag::BYTE:
            tag = new byte_tag(name, read_value<char>(stream));
            break;
        case generic_tag::SHORT:
            tag = new short_tag(name, read_value<short>(stream));
            break;
        case generic_tag::INT:
            tag = new int_tag(name, read_value<int>(stream));
            break;
        case generic_tag::LONG:
            tag = new long_tag(name, read_long_value<int64_t>(stream));
            break;
        case generic_tag::FLOAT:
            tag = new float_tag(name, read_value<float>(stream));
            break;
        case generic_tag::DOUBLE:
            tag = new double_tag(name, read_value<double>(stream));
            break;
        case generic_tag::BYTE_ARRAY:
            tag = new byte_array_tag(name, read_array_value<char>(stream));
            break;
        case generic_tag::STRING:
            tag = new string_tag(name, read_string_value(stream));
            break;
        case generic_tag::LIST: {
            char ele_type = read_value<char>(stream);
            int ele_len = read_value<int>(stream);
            list_tag* lst_tag = new list_tag(name, ele_type);

            // parse all subtags and add to list
            for (int i = 0; i < ele_len; ++i) {
                sub_tag = parse_tag(stream, true, ele_type);
                lst_tag->push_back(sub_tag);
            }
            tag = lst_tag;
        }
            break;
        case generic_tag::COMPOUND: {
            compound_tag* cmp_tag = new compound_tag(name);

            // parse all sub_tags and add to compound
            do {
                sub_tag = parse_tag(stream, false, 0);
                if (!sub_tag)
                    throw std::runtime_error("Failed to parse tag");
                if (sub_tag->get_type() != generic_tag::END)
                    cmp_tag->push_back(sub_tag);
            } while (sub_tag->get_type() != generic_tag::END);
            delete sub_tag;
            tag = cmp_tag;
        }
            break;
        case generic_tag::INT_ARRAY:
            tag = new int_array_tag(name, read_array_value<int>(stream));
            break;
        case generic_tag::LONG_ARRAY:
            tag = new long_array_tag(name, read_array_value<int64_t>(stream));
            break;
        default:
            throw std::runtime_error("Unknown tag type: " + std::to_string(type));
            break;
    }
    return tag;
}

/*
 * Read a chunk tag from data
 */
void region_file_reader::parse_chunk_tag(std::vector<char>& data, chunk_tag& tag) {
    char type;
    std::string name;
    generic_tag* sub_tag = NULL;

    // setup bytestream
    byte_stream bstream(data);
    bstream.set_swap(byte_stream::NO_SWAP_ENDIAN);

    // parse tags from root
    type = read_value<char>(bstream);
    if (type == generic_tag::END)
        return;
    else {
        short name_len = read_value<short>(bstream);
        for (short i = 0; i < name_len; ++i) {
            name += read_value<char>(bstream);
        }
        tag.get_root_tag().set_name(name);
        do {

            //parse subtag
            sub_tag = parse_tag(bstream, false, 0);
            if (!sub_tag)
                throw std::runtime_error("Failed to parse tag");
            if (sub_tag->get_type() != generic_tag::END)
                tag.get_root_tag().push_back(sub_tag);
        } while (sub_tag->get_type() != generic_tag::END);
        delete sub_tag;
    }
}

/*
 * Reads a file into region_file
 */
void region_file_reader::read(bool lazy) {
    int x, z;

    // attempt to open file
    file.open(path.c_str(), std::ios::in | std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("Failed to open input file: " + path);

    // parse the filename for coordinants
    if (!is_region_file(path, x, z))
        throw std::runtime_error("Malformated region filename");
    reg.set_x(x);
    reg.set_z(z);

    // read header data
    read_header();

    if (!lazy) {
        // read chunk data
        read_chunks();
    }

    // close file
    file.close();
}

/*
 * Reads chunk data from a file
 */
void region_file_reader::read_chunks() {
    chunk_info info;

    // check if file is open
    if (!file.is_open())
        throw std::runtime_error("Failed to read chunk data");

    // iterate though header entries, reading in chunks if they exist
    for (unsigned int i = 0; i < region_dim::CHUNK_COUNT; ++i) {
        info = reg.get_header().get_info_at(i);

        // skip empty chunks
        if (info.empty())
            continue;

        // Retrieve raw data
        size_t length = info.get_length();
		std::vector<char> raw_data, raw_vec;
		raw_data.resize(length, 0);
		file.clear();
        file.seekg(info.get_offset(), std::ios::beg);
        file.read((char *)&raw_data[0], length);
		raw_vec.clear();
		raw_vec.insert(raw_vec.begin(), raw_data.begin(), raw_data.end());

        // check for compression type
        switch (info.get_type()) {
            case chunk_info::GZIP:
                throw std::runtime_error("Unsupported compression type");
                break;
            case chunk_info::ZLIB:
                if(compression::inflate_(raw_vec) == false) {
				    throw std::runtime_error("Failed to uncompress chunk");
			    }
                break;
            default:
                throw std::runtime_error("Unknown compression type");
                break;
        }

        // use data to fill chunk tag
        parse_chunk_tag(raw_vec, reg.get_tag_at(i));
    }
}


void region_file_reader::read_chunk(uint16_t x, uint16_t z) {
    // attempt to open file
    file.open(path.c_str(), std::ios::in | std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("Failed to open input file");

    chunk_info info;

    uint16_t chunkToRead = z * region_dim::CHUNK_WIDTH + x;


    info = reg.get_header().get_info_at(chunkToRead);

    // skip empty chunks
    if (info.empty())
        throw std::out_of_range("Chunk at " + std::to_string(x) + "|" + std::to_string(z) + " is empty");

    // Retrieve raw data
    char* raw_data = new char[info.get_length()];
    std::vector<char> raw_vec;
    file.seekg(info.get_offset(), std::ios::beg);
    file.read((char*) raw_data, info.get_length());
    raw_vec.assign(raw_data, raw_data + info.get_length());

    // check for compression type
    switch (info.get_type()) {
        case chunk_info::GZIP:
            throw std::runtime_error("Unsupported compression type");
            break;
        case chunk_info::ZLIB:
            compression::inflate_(raw_vec);
            break;
        default:
            throw std::runtime_error("Unknown compression type");
            break;
    }

    // use data to fill chunk tag
    parse_chunk_tag(raw_vec, reg.get_tag_at(chunkToRead));

    file.close();
}


/*
 * Reads header data from a file
 */
void region_file_reader::read_header(void) {
    char type;
    int value;
    size_t offset;

    // check if file is open
    if (!file.is_open())
        throw std::runtime_error("Failed to read header data");

    // read position data into header
    for (unsigned int i = 0; i < region_dim::CHUNK_COUNT; ++i) {
        file.read(reinterpret_cast<char*>(&value), sizeof(value));
        convert_endian(value);
        reg.get_header().get_info_at(i).set_offset(value);
    }

    // read timestamp data into header
    for (unsigned int i = 0; i < region_dim::CHUNK_COUNT; ++i) {
        file.read(reinterpret_cast<char*>(&value), sizeof(value));
        convert_endian(value);
        reg.get_header().get_info_at(i).set_modified(value);
    }

    // read length and compression type data into header
    for (unsigned int i = 0; i < region_dim::CHUNK_COUNT; ++i) {
        offset = reg.get_header().get_info_at(i).get_offset();

        // skip all empty chunks
        if (!offset)
            continue;

        // collect length and compression data
        file.seekg((offset >> 8) * region_dim::SECTOR_SIZE, std::ios::beg);
        file.read(reinterpret_cast<char*>(&value), sizeof(value));
        convert_endian(value);
        reg.get_header().get_info_at(i).set_length(value);
        file.read(&type, sizeof(type));
        reg.get_header().get_info_at(i).set_type(type);
        reg.get_header().get_info_at(i).set_offset((unsigned int) file.tellg());
    }
}

/*
 * Reads a string tag value from stream
 */
std::string region_file_reader::read_string_value(byte_stream& stream) {
    short str_len;
    std::string value;

    // check stream status
    if (!stream.good())
        throw std::runtime_error("Unexpected end of stream");

    // retrieve value
    str_len = read_value<short>(stream);
    for (short i = 0; i < str_len; ++i) {
        value += read_value<char>(stream);
    }
    return value;
}

uint64_t region_file_reader::getBits(uint64_t val, unsigned int start, unsigned int end) {
    val >>= start;
    unsigned int relevantBits = static_cast<unsigned int>(pow(2, end - start) - 1);
    val &= relevantBits;
    return val;
}










