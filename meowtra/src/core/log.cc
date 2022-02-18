#include "log.h"

#include <stdlib.h>
#include <string>
#include <iostream>
#include <mutex>

MEOW_NAMESPACE_BEGIN

bool verbose_flag = false;
static std::mutex log_mutex;

std::string vstringf(const char *fmt, va_list ap)
{
    std::string string;
    char *str = NULL;

#if defined(_WIN32) || defined(__CYGWIN__)
    int sz = 64 + strlen(fmt), rc;
    while (1) {
        va_list apc;
        va_copy(apc, ap);
        str = (char *)realloc(str, sz);
        rc = vsnprintf(str, sz, fmt, apc);
        va_end(apc);
        if (rc >= 0 && rc < sz)
            break;
        sz *= 2;
    }
#else
    if (vasprintf(&str, fmt, ap) < 0)
        str = NULL;
#endif

    if (str != NULL) {
        string = str;
        free(str);
    }

    return string;
}

std::string stringf(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    std::string result = vstringf(format, ap);
    va_end(ap);
    return result;
}

void logv(const char *prefix, const char *format, va_list ap)
{
    std::unique_lock lock(log_mutex);
    std::string str = vstringf(format, ap);
    if (str.empty())
        return;
    std::cerr << prefix << str;
}


void log(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    logv("", format, ap);
    va_end(ap);
}

void log_verbose(const char *format, ...) {
    if (!verbose_flag)
        return;
    va_list ap;
    va_start(ap, format);
    logv("😼: ", format, ap);
    va_end(ap);
}

void log_info(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    logv("🐱: ", format, ap);
    va_end(ap);
}

void log_warning(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    logv("😾: ", format, ap);
    va_end(ap);
}

void log_error(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    logv("😿: ", format, ap);
    va_end(ap);
    exit(1);
}

MEOW_NAMESPACE_END
