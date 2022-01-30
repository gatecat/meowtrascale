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

MEOW_NAMESPACE_END

#endif
