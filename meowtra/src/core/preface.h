#ifndef PREFACE_H
#define PREFACE_H

#include <stdexcept>
#include <stdint.h>
#include <string>

#ifndef MEOW_NAMESPACE
#define MEOW_NAMESPACE Meowtra
#endif

#ifdef MEOW_NAMESPACE
#define MEOW_NAMESPACE_PREFIX MEOW_NAMESPACE::
#define MEOW_NAMESPACE_BEGIN namespace MEOW_NAMESPACE {
#define MEOW_NAMESPACE_END }
#define USING_MEOW_NAMESPACE using namespace MEOW_NAMESPACE;
#else
#define MEOW_NAMESPACE_PREFIX
#define MEOW_NAMESPACE_BEGIN
#define MEOW_NAMESPACE_END
#define USING_MEOW_NAMESPACE
#endif

#if defined(__GNUC__) || defined(__clang__)
#define MEOW_ATTRIBUTE(...) __attribute__((__VA_ARGS__))
#define MEOW_NORETURN __attribute__((noreturn))
#define MEOW_DEPRECATED __attribute__((deprecated))
#define MEOW_PACKED_STRUCT(...) __VA_ARGS__ __attribute__((packed))
#define MEOW_ALWAYS_INLINE MEOW_ATTRIBUTE(__always_inline__)
#elif defined(_MSC_VER)
#define MEOW_ATTRIBUTE(...)
#define MEOW_NORETURN __declspec(noreturn)
#define MEOW_DEPRECATED __declspec(deprecated)
#define MEOW_PACKED_STRUCT(...) __pragma(pack(push, 1)) __VA_ARGS__ __pragma(pack(pop))
#define MEOW_ALWAYS_INLINE
#else
#define MEOW_ATTRIBUTE(...)
#define MEOW_NORETURN
#define MEOW_DEPRECATED
#define MEOW_PACKED_STRUCT(...) __VA_ARGS__
#define MEOW_ALWAYS_INLINE
#endif

MEOW_NAMESPACE_BEGIN

#ifdef MEOW64
typedef int64_t index_t;
#else
typedef int32_t index_t;
#endif

class assertion_failure : public std::runtime_error
{
  public:
    assertion_failure(std::string msg, std::string expr_str, std::string filename, int line);

    std::string msg;
    std::string expr_str;
    std::string filename;
    int line;
};

MEOW_NORETURN
inline void assert_fail_impl(const char *message, const char *expr_str, const char *filename, int line)
{
    throw assertion_failure(message, expr_str, filename, line);
}

MEOW_NORETURN
inline void assert_fail_impl_str(std::string message, const char *expr_str, const char *filename, int line)
{
    throw assertion_failure(message, expr_str, filename, line);
}

#define MEOW_ASSERT(cond) (!(cond) ? assert_fail_impl(#cond, #cond, __FILE__, __LINE__) : (void)true)
#define MEOW_ASSERT_MSG(cond, msg) (!(cond) ? assert_fail_impl(msg, #cond, __FILE__, __LINE__) : (void)true)
#define MEOW_ASSERT_FALSE(msg) (assert_fail_impl(msg, "false", __FILE__, __LINE__))
#define MEOW_ASSERT_FALSE_STR(msg) (assert_fail_impl_str(msg, "false", __FILE__, __LINE__))

#define MEOW_STRINGIFY_MACRO(x) MEOW_STRINGIFY(x)
#define MEOW_STRINGIFY(x) #x

MEOW_NAMESPACE_END

#endif
