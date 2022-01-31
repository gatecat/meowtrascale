#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdarg.h>
#include <string>
#include "preface.h"

MEOW_NAMESPACE_BEGIN

extern bool verbose_flag;

std::string stringf(const char *format, ...);
void log(const char *format, ...);
void log_info(const char *format, ...);
void log_verbose(const char *format, ...);
void log_warning(const char *format, ...);
void log_error(const char *format, ...);

MEOW_NAMESPACE_END

#endif
