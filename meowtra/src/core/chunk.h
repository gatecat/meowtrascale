#ifndef CHUNK_H
#define CHUNK_H
#include "preface.h"
#include <vector>
#include <variant>

MEOW_NAMESPACE_BEGIN

// Flexible chunk type
//   can be a refcounted window, own its own backing, or be filled with zeros
template <typename T> struct Chunkable;
template <typename T> struct Chunk {
    struct ref_window {
        Chunkable<T> &base;
        index_t offset, length;
        index_t size() const { return length; }
        T get(index_t idx) const;
        void set(index_t idx, T value);
        ~ref_window();
    };
    struct fixed_zero {
        index_t m_size;
        inline index_t size() const { return m_size; }
        inline T get(index_t idx) const { MEOW_ASSERT(idx >= 0 && idx < m_size); return T(0); }
        inline void set(index_t idx, T value) { MEOW_ASSERT_FALSE("can't set fixed_zero data"); }
    };
    struct owned_data {
        std::vector<T> data;
        inline index_t size() const { return index_t(data.size()); }
        inline T get(index_t idx) const { return data.at(idx); }
        inline void set(index_t idx, T value) { data.at(idx) = value; }
    };
    std::variant<std::vector<T>, fixed_zero, ref_window> content;
    explicit Chunk(const std::vector<T> &data) : content(owned_data{data}) {};
    explicit Chunk(Chunkable<T> &base, index_t offset, index_t length);
    static Chunk zeros(index_t size) {
        return Chunk(fixed_zero{size});
    }
    static Chunk backed(index_t size) {
        return Chunk(std::vector<T>(size));
    }
    index_t size() const { 
        return std::visit(content, [](auto &&x) { return x.size(); });
    }
    T get(index_t idx) const { 
        return std::visit(content, [&](auto &&x) { return x.get(idx); });
    }
    void set(index_t idx, T value) { 
        std::visit(content, [&](auto &&x) { x.set(idx, value); });
    }
    ~Chunk();
};

template <typename T> struct Chunkable {
    std::vector<T> base;
    int refcount = 0;
    size_t size() { return base.size; }

    std::vector<T> &data() { return base; }
    const std::vector<T> &data() const { return base; }
    ~Chunkable() {
        MEOW_ASSERT(refcount == 0);
    }
};

template <typename T> Chunk<T>::Chunk(Chunkable<T> &base, index_t offset, index_t length) : content(ref_window{base, offset, length}) {
    ++base.refcount;
}

template <typename T> T Chunk<T>::ref_window::get(index_t idx) const {
    MEOW_ASSERT(idx >= 0 && idx < length);
    return base.base.at(idx + offset);
}

template <typename T> void Chunk<T>::ref_window::set(index_t idx, T value) {
    MEOW_ASSERT(idx >= 0 && idx < length);
    base.base.at(idx + offset) = value;
}

template <typename T> Chunk<T>::ref_window::~ref_window() {
    MEOW_ASSERT(base.refcount > 0);
    --base.refcount;
}

MEOW_NAMESPACE_END

#endif
