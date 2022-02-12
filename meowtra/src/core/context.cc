#include "context.h"
#include "constids.h"

MEOW_NAMESPACE_BEGIN

Context::Context() {
    idstring_str_to_idx = new std::unordered_map<std::string, index_t>;
    idstring_idx_to_str = new std::vector<const std::string*>;
    IdString::initialize_add(this, "", 0);
#define X(t) IdString::initialize_add(this, #t, ID_##t);
#include "constids.inc"
#undef X
}

MEOW_NAMESPACE_END
