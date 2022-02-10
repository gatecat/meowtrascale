#ifndef CONTEXT_H
#define CONTEXT_H

#include <string>
#include <unordered_map>
#include <vector>
#include "preface.h"
#include "idstring.h"

MEOW_NAMESPACE_BEGIN

struct Context {
    // ID String database.
    Context();
    mutable std::unordered_map<std::string, int> *idstring_str_to_idx;
    mutable std::vector<const std::string *> *idstring_idx_to_str;
    IdString id(const std::string &s) { return IdString(this, s); };
};

MEOW_NAMESPACE_END

#endif
