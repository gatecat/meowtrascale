#ifndef BITSTREAM_H
#define BITSTREAM_H

#include "preface.h"
#include "chunk.h"

#include <vector>
#include <string>
#include <iostream>

MEOW_NAMESPACE_BEGIN

struct RawBitstream {
    std::vector<std::string> metadata;
    Chunkable<uint32_t> words;

    static RawBitstream read(std::istream &in);
};

struct BitstreamPacket {
    enum : uint8_t {
        NOP = 0x00,
        READ = 0x01,
        WRITE = 0x02,
    } op;

    uint16_t reg;

    Chunk<uint32_t> payload;
};

MEOW_NAMESPACE_END

#endif
