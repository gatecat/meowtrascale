#include "database.h"
#include "log.h"

#include <filesystem>
#include <vector>

// From Yosys
#if defined(_WIN32)
#  include <windows.h>
#  include <io.h>
#elif defined(__APPLE__)
#  include <mach-o/dyld.h>
#  include <unistd.h>
#  include <dirent.h>
#  include <sys/stat.h>
#else
#  include <unistd.h>
#  include <dirent.h>
#  include <sys/types.h>
#  include <sys/stat.h>
#endif


MEOW_NAMESPACE_BEGIN

namespace {
// From Yosys
// Copyright (C) 2012  Claire Xenia Wolf <claire@yosyshq.com>
#if defined(__linux__) || defined(__CYGWIN__)
std::string proc_self_dirname()
{
    char path[PATH_MAX];
    ssize_t buflen = readlink("/proc/self/exe", path, sizeof(path));
    if (buflen < 0) {
        log_error("readlink(\"/proc/self/exe\") failed\n");
    }
    while (buflen > 0 && path[buflen-1] != '/')
        buflen--;
    return std::string(path, buflen);
}
#elif defined(__FreeBSD__)
std::string proc_self_dirname()
{
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
    size_t buflen;
    char *buffer;
    std::string path;
    if (sysctl(mib, 4, NULL, &buflen, NULL, 0) != 0)
        log_error("sysctl failed: %s\n", strerror(errno));
    buffer = (char*)malloc(buflen);
    if (buffer == NULL)
        log_error("malloc failed: %s\n", strerror(errno));
    if (sysctl(mib, 4, buffer, &buflen, NULL, 0) != 0)
        log_error("sysctl failed: %s\n", strerror(errno));
    while (buflen > 0 && buffer[buflen-1] != '/')
        buflen--;
    path.assign(buffer, buflen);
    free(buffer);
    return path;
}
#elif defined(__APPLE__)
std::string proc_self_dirname()
{
    char *path = NULL;
    uint32_t buflen = 0;
    while (_NSGetExecutablePath(path, &buflen) != 0)
        path = (char *) realloc((void *) path, buflen);
    while (buflen > 0 && path[buflen-1] != '/')
        buflen--;
    std::string str(path, buflen);
    free(path);
    return str;
}
#elif defined(_WIN32)
std::string proc_self_dirname()
{
    int i = 0;
#  ifdef __MINGW32__
    char longpath[MAX_PATH + 1];
    char shortpath[MAX_PATH + 1];
#  else
    WCHAR longpath[MAX_PATH + 1];
    TCHAR shortpath[MAX_PATH + 1];
#  endif
    if (!GetModuleFileName(0, longpath, MAX_PATH+1))
        log_error("GetModuleFileName() failed.\n");
    if (!GetShortPathName(longpath, shortpath, MAX_PATH+1))
        log_error("GetShortPathName() failed.\n");
    while (shortpath[i] != 0)
        i++;
    while (i > 0 && shortpath[i-1] != '/' && shortpath[i-1] != '\\')
        shortpath[--i] = 0;
    std::string path;
    for (i = 0; shortpath[i]; i++)
        path += char(shortpath[i]);
    return path;
}
#elif defined(EMSCRIPTEN) || defined(__wasm)
std::string proc_self_dirname()
{
    return "/";
}
#else
    #error "Don't know how to determine process executable base path!"
#endif

// relative to executable
static const std::vector<std::string> db_paths = {
    "../../database",
    "../share/meowtrascale/database",
};

}

const std::string &get_db_root() {
    static std::string db_root;
    if (db_root.empty()) {
        std::string bin_dir = proc_self_dirname();
        for (auto &path : db_paths) {
            std::string result = bin_dir + path;
            if (std::filesystem::exists(result)) {
                db_root = result;
                break;
            }
        }
        if (db_root.empty()) {
            log_error("Unable to find database!\n");
        }
    }
    return db_root;
}

MEOW_NAMESPACE_END
