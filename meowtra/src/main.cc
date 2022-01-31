#include "bitstream.h"
#include "log.h"

#include <fstream>

USING_MEOW_NAMESPACE;

int main(int argc, char *argv[]) {
    MEOW_ASSERT(argc == 2);
    std::ifstream in(argv[1], std::ios::binary);
    verbose_flag = true;
    auto bit = RawBitstream::read(in);
    {
        auto packets = bit.packetise();
    }
    return 0;
}
