#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include <vector>

#include "preface.h"
#include "idstring.h"

MEOW_NAMESPACE_BEGIN

const std::string &get_db_root();

struct Device {
    std::string family;
    std::string name;
    uint32_t idcode;
    // ...
};

extern const std::vector<Device> all_devices;

const Device *device_by_idcode(uint32_t idcode);

MEOW_NAMESPACE_END

#endif
