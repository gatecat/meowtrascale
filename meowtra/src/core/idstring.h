#ifndef IDSTRING_H
#define IDSTRING_H

#include <string>
#include "preface.h"

MEOW_NAMESPACE_BEGIN

struct Context;

struct IdString
{
    index_t index;

    static void initialize_add(const Context *ctx, const char *s, index_t idx);

    constexpr IdString() : index(0) {}
    explicit constexpr IdString(index_t index) : index(index) {}

    void set(const Context *ctx, const std::string &s);

    IdString(const Context *ctx, const std::string &s) { set(ctx, s); }

    IdString(const Context *ctx, const char *s) { set(ctx, s); }

    const std::string &str(const Context *ctx) const;

    const char *c_str(const Context *ctx) const;

    bool operator<(const IdString &other) const { return index < other.index; }

    bool operator==(const IdString &other) const { return index == other.index; }

    bool operator!=(const IdString &other) const { return index != other.index; }

    bool empty() const { return index == 0; }

    unsigned int hash() const { return index; }

    template <typename... Args> bool in(Args... args) const
    {
        // Credit: https://articles.emptycrate.com/2016/05/14/folds_in_cpp11_ish.html
        bool result = false;
        (void)std::initializer_list<int>{(result = result || in(args), 0)...};
        return result;
    }

    bool in(const IdString &rhs) const { return *this == rhs; }
};

MEOW_NAMESPACE_END
#endif
