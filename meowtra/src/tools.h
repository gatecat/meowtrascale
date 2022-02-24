#ifndef TOOLS_H
#define TOOLS_H

#include "preface.h"

MEOW_NAMESPACE_BEGIN

int subcmd_unpack(int argc, const char *argv[]);
// int subcmd_pack(int argc, const char *argv[]);
// ...
int subcmd_correlate(int argc, const char *argv[]);
int subcmd_fuzztools(int argc, const char *argv[]);

MEOW_NAMESPACE_END

#endif
