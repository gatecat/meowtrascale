#ifndef DATAFILE_H
#define DATAFILE_H

#include "preface.h"

#include <string>
#include <string_view>

MEOW_NAMESPACE_BEGIN

// Simple, easily version-controllable and mergeable, data file format
struct word_iterator {
    word_iterator(const std::string &buf, const index_t pos) : buf(buf), pos(-1) {
        if (pos != -1)
            skip();
    };
    const std::string &buf;
    index_t pos = -1; // -1 is the EOL flag
    void skip() {
        while (true) {
            if (pos >= buf.size()) { // EOF
                pos = -1;
                break;
            }
            char c = buf.at(pos);
            if (c == '\n' ||  c == '\r' || c == '#') { // EOL
                pos = -1;
                break;
            }
            if (c == ' ' || c == '\t') { // whitespace
                pos++;
                continue;
            }
            break;
        }
    }
    bool is_word_char(char c) const {
        return c != ' ' && c != '\t' && c != '#' && c != '\r' && c != '\n';
    }
    mutable index_t end_cache = -1;
    index_t get_end() const {
        if (end_cache != -1)
            return end_cache;
        index_t end = pos;
        while (end < index_t(buf.size()) && is_word_char(buf.at(end)))
            end++;
        end_cache = end;
        return end;
    }
    std::string_view operator*() const {
        return std::string_view(buf.begin() + pos, buf.begin() + get_end());
    }
    void operator++() {
        pos = get_end();
        end_cache = -1; // because we updated pos
        skip();
    }
};

struct word_range {
    const std::string &buf;
    index_t start;
    word_range(const std::string &buf, index_t start) : buf(buf), start(start) {};
    word_iterator begin() { return word_iterator(buf, start); }
    word_iterator end() { return word_iterator(buf, -1); }
};

struct line_iterator {
    line_iterator(const std::string &buf, const index_t pos) : buf(buf), pos(-1) {
        if (pos != -1)
            skip();
    };
    const std::string &buf;
    index_t pos = -1; // -1 is the EOF flag
    void skip() {
        while (true) {
            if (pos >= index_t(buf.size())) { // EOF
                pos = -1;
                break;
            }
            char c = buf.at(pos);
            if (c == '#') {
                // skip over comment-only line
                pos = get_end();
                continue;
            } else if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                // skip over whitespace
                pos++;
                continue;
            }
            break;
        }
    }
    index_t get_end() const {
        index_t end = pos;
        while (end < index_t(buf.size()) && buf.at(end) != '\n' && buf.at(end) != '\r')
            end++;
        return end;
    }
    word_range operator*() const {
        return word_range(buf, pos);
    }
    void operator++() {
        pos = get_end();
        skip();
    }
};

std::pair<std::string_view, std::string_view> split_view(std::string_view view, char delim);
int32_t parse_i32(std::string_view view);
uint32_t parse_u32(std::string_view view);

MEOW_NAMESPACE_END

#endif
