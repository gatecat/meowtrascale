#include "context.h"

MEOW_NAMESPACE_BEGIN

Context::Context() {
    idstring_str_to_idx = new std::unordered_map<std::string, index_t>;
    idstring_idx_to_str = new std::vector<const std::string*>;
    IdString::initialize_add(this, "", 0);
}

MEOW_NAMESPACE_END
