#include "bitstream.h"
#include "log.h"

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
            ++count;
        }
        return false;
    }

    constexpr uint32_t kCrc32CastagnoliPolynomial = 0x82F63B78;

    // From prjxray
    // The CRC is calculated from each written data word and the current
    // register address the data is written to.

    // Extend the current CRC value with one register address (5bit) and
    // frame data (32bit) pair and return the newly computed CRC value.

    inline uint32_t icap_crc(uint32_t addr, uint32_t data, uint32_t prev) {
        constexpr int kAddressBitWidth = 5;
        constexpr int kDataBitWidth = 32;

        uint64_t poly = static_cast<uint64_t>(kCrc32CastagnoliPolynomial) << 1;
        uint64_t val = (static_cast<uint64_t>(addr) << 32) | data;
        uint64_t crc = prev;

        for (int i = 0; i < kAddressBitWidth + kDataBitWidth; i++) {
            if ((val & 1) != (crc & 1))
                crc ^= poly;

            val >>= 1;
            crc >>= 1;
        }
        return crc;
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

std::vector<BitstreamPacket> RawBitstream::packetise() {
    std::vector<BitstreamPacket> result;
    uint32_t curr_crc = 0;

    index_t offset = 0;
    auto &data = words.base;
    uint16_t last_reg = 0;
    while (offset < int(data.size())) {
        uint32_t hdr = data.at(offset++);
        if (hdr == 0xFFFFFFFF) {
            // desync
            // TODO: what if stuff follows?
            break;
        }
        uint8_t type = (hdr >> 29U) & 0x07U;
        if (type == 0b001) {
            // short packet
            uint8_t op = (hdr >> 27U) & 0x03U;
            if (op == 0x00 || op == 0x01) {
                // NOP/READ (skipped)
            } else if (op == 0x02) {
                // WRITE
                uint16_t reg = (hdr >> 13U) & 0x3FFFU;
                index_t count = hdr & 0x3FFU;
                last_reg = reg;
                log_verbose("write %04x len=%d\n", reg, count);
                if (reg == BitstreamPacket::CRC) {
                    // special case
                    MEOW_ASSERT(count > 0);
                    offset += (count - 1);
                    uint32_t expected_crc = data.at(offset++);
                    if (expected_crc != curr_crc)
                        log_warning("CRC mismatch at %d: calculated %08x, read %08x\n", offset, curr_crc, expected_crc);
                    curr_crc = 0;
                } else if (reg == BitstreamPacket::CMD && count == 1 && data.at(offset) == 0x7) {
                    // reset CRC
                    ++offset;
                    curr_crc = 0;
                } else if (count > 0) {
                    // create a packet
                    result.emplace_back(reg, words.window(offset, count));
                    // update calculated CRC
                    for (int i = 0; i < count; i++) {
                        curr_crc = icap_crc(reg, data.at(offset++), curr_crc);
                    }
                }

            } else {
                log_error("unknown op %d in header %08x\n", op, hdr);
            }
        } else if (type == 0b010) {
            // long packet
            index_t count = hdr & 0x3FFFFFF;
            log_verbose("long write %04x len=%d\n", last_reg, count);
            result.emplace_back(last_reg, words.window(offset, count));
            for (int i = 0; i < count; i++)
                        curr_crc = icap_crc(last_reg, data.at(offset++), curr_crc);
        } else {
            log_error("unknown packet type %d in header %08x\n", type, hdr);
        }
    }
    return result;
}

MEOW_NAMESPACE_END
