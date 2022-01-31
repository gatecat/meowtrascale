#include "bitstream.h"

#include <array>

MEOW_NAMESPACE_BEGIN

namespace {
    bool scan_preamble(std::istream &stream, uint32_t preamble) {
        index_t count = 0;
        uint32_t shiftreg = 0;
        while (!stream.eof()) {
            if (count >= 4 && shiftreg == preamble) {
                return true;
            }
            uint8_t next = 0;
            stream.read(reinterpret_cast<char*>(&next), 1);
            shiftreg = (shiftreg << 8) | uint32_t(next);
        }
        return false;
    }
}

RawBitstream RawBitstream::read(std::istream &in) {
    RawBitstream result;
    // TODO: metadata before preamble
    // Scan for preamble
    bool found = scan_preamble(in, 0xAA995566);
    MEOW_ASSERT(found);

    std::array<uint8_t, 4> buf;
    while (!in.eof()) {
        in.read(reinterpret_cast<char*>(buf.data()), buf.size());
        result.words.base.push_back(
            (uint32_t(buf[0]) << 24U) |
            (uint32_t(buf[1]) << 16U) |
            (uint32_t(buf[2]) <<  8U) |
            (uint32_t(buf[3])       )
        );
        if (in.gcount() != 4)
            break;
    }

    return result;
}

MEOW_NAMESPACE_END
