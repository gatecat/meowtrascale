#include "idstring.h"
#include "context.h"
#include <mutex>

MEOW_NAMESPACE_BEGIN

void IdString::set(const Context *ctx, const std::string &s)
{
    std::unique_lock lock(ctx->idstring_mutex);
    auto it = ctx->idstring_str_to_idx->find(s);
    if (it == ctx->idstring_str_to_idx->end()) {
        index = ctx->idstring_idx_to_str->size();
        auto insert_rc = ctx->idstring_str_to_idx->insert({s, index});
        ctx->idstring_idx_to_str->push_back(&insert_rc.first->first);
    } else {
        index = it->second;
    }
}

const std::string &IdString::str(const Context *ctx) const {
    std::shared_lock lock(ctx->idstring_mutex);
    return *ctx->idstring_idx_to_str->at(index); 
}

const char *IdString::c_str(const Context *ctx) const {
    std::shared_lock lock(ctx->idstring_mutex);
    return str(ctx).c_str();
}

void IdString::initialize_add(const Context *ctx, const char *s, index_t idx)
{
    std::unique_lock lock(ctx->idstring_mutex);
    MEOW_ASSERT(ctx->idstring_str_to_idx->count(s) == 0);
    MEOW_ASSERT(index_t(ctx->idstring_idx_to_str->size()) == idx);
    auto insert_rc = ctx->idstring_str_to_idx->insert({s, idx});
    ctx->idstring_idx_to_str->push_back(&insert_rc.first->first);
}

MEOW_NAMESPACE_END
