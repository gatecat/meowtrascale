#ifndef BITSTREAM_H
#define BITSTREAM_H

#include "preface.h"
#include "chunk.h"

#include <vector>
#include <string>
#include <iostream>

MEOW_NAMESPACE_BEGIN

struct BitstreamPacket;

struct RawBitstream {
    std::vector<std::string> metadata;
    Chunkable<uint32_t> words;

    static RawBitstream read(std::istream &in);
    std::vector<BitstreamPacket> packetise();
};

struct BitstreamPacket {
    uint16_t slr;

    enum Reg : uint16_t {
        CRC     = 0b00000,
        FAR     = 0b00001,
        FDRI    = 0b00010,
        FDRO    = 0b00011,
        CMD     = 0b00100,
        CTL0    = 0b00101,
        MASK    = 0b00110,
        STAT    = 0b00111,
        LOUT    = 0b01000,
        COR0    = 0b01001,
        MFWR    = 0b01010,
        CBC     = 0b01011,
        IDCODE  = 0b01100,
        AXSS    = 0b01101,
        COR1    = 0b01110,
        WBSTAR  = 0b10000,
        BOOTSTS = 0b10110,
        CTL1    = 0b11000,
        BSPI    = 0b11111,
    } reg;

    Chunk<uint32_t> payload;

    BitstreamPacket(uint16_t slr, uint16_t reg, Chunk<uint32_t> payload) : slr(slr), reg(Reg(reg)), payload(payload) {};

    std::string header_str();
};

MEOW_NAMESPACE_END

#endif
