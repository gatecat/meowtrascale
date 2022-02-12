#include "bitstream.h"
#include "log.h"
#include "database.h"

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
    uint16_t slr = 0;
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
                index_t count = hdr & 0x1FFFU;
                last_reg = reg;
                log_verbose("write %04x len=%d\n", reg, count);
                if (reg == BitstreamPacket::CRC) {
                    // special case
                    MEOW_ASSERT(count > 0);
                    result.emplace_back(slr, reg, words.window(offset, count)); // have to track CRC writes as they increment FAR?
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
                    result.emplace_back(slr, reg, words.window(offset, count));
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
            result.emplace_back(slr, last_reg, words.window(offset, count));
            for (int i = 0; i < count; i++)
                        curr_crc = icap_crc(last_reg, data.at(offset++), curr_crc);
        } else {
            log_error("unknown packet type %d in header %08x\n", type, hdr);
        }
    }
    return result;
}

namespace {
index_t get_frame_range(const std::vector<FrameRange> &ranges, uint32_t slr, uint32_t frame) {
    index_t b = 0, e = index_t(ranges.size()) - 1;
    while (b <= e) {
        index_t i = (b + e) / 2;
        auto &r = ranges.at(i);
        if (slr == r.slr && frame >= r.begin && frame < (r.begin + r.count)) {
            // match
            return i;
        }
        if (r.slr > slr || ((slr == r.slr) && (r.begin + r.count) >= frame))
            e = i - 1;
        else
            b = i + 1;
    }
    return -1;
}

uint32_t get_next_frame(const std::vector<FrameRange> &ranges, uint32_t slr, uint32_t frame) {
    index_t ri = get_frame_range(ranges, slr, frame);
    if (ri == -1)
        return frame; // no match
    auto &r = ranges.at(ri);
    if (frame < (r.begin + r.count - 1))
        return frame + 1; // inside range
    else if (ri < (index_t(ranges.size()) - 1))
        return ranges.at(ri+1).begin;
    else
        return frame; // end of regions
}
}

BitstreamFrames packets_to_frames(const std::vector<BitstreamPacket> &packets) {
    BitstreamFrames res;
    uint32_t far = 0;
    std::vector<FrameRange> frame_ranges;

    const int frame_length = 93; // TODO: other devices than xcup

    for (auto &packet : packets) {
        if (packet.reg == BitstreamPacket::IDCODE && packet.payload.size() >= 1 && !res.dev) {
            uint32_t idc = packet.payload.get(0);
            res.dev = device_by_idcode(idc);
            if (!res.dev)
                log_error("no known device with IDCODE 0x%08x\n", idc);
            frame_ranges = get_device_frames(*res.dev);
        } else if (packet.reg == BitstreamPacket::FAR && packet.payload.size() >= 1) {
            far = packet.payload.get(0);
        } else if (packet.reg == BitstreamPacket::CRC) {
            // increment FAR
            far = get_next_frame(frame_ranges, packet.slr, far);
        } else if (packet.reg == BitstreamPacket::FDRI) {
            // TODO: check ECC etc
            // TODO: split frames?
            for (index_t i = 0; i < packet.payload.size(); i += 94) {
                res.frame_data.emplace(FrameKey{packet.slr, far}, packet.payload.subchunk(i, std::max(94, packet.payload.size() - i)));
                far = get_next_frame(frame_ranges, packet.slr, far);
            }
        }
    }
    return res;
}

MEOW_NAMESPACE_END
