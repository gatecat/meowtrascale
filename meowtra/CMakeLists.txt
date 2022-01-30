cmake_minimum_required(VERSION 3.5)
project(meowtra CXX C)

set(CMAKE_CXX_STANDARD 20)


if (MSVC)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /D_DEBUG /W4 /wd4100 /wd4244 /wd4125 /wd4800 /wd4456 /wd4458 /wd4305 /wd4459 /wd4121 /wd4996")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /W4 /wd4100 /wd4244 /wd4125 /wd4800 /wd4456 /wd4458 /wd4305 /wd4459 /wd4121 /wd4996 /wd4127")
else()
    # N.B. the -Wno-array-bounds is to work around a false positive in GCC 9
    set(WARN_FLAGS "-Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wno-array-bounds")
    if (WERROR)
        set(WARN_FLAGS "${WARN_FLAGS} -Werror")
    endif()
    set(CMAKE_CXX_FLAGS_DEBUG "${WARN_FLAGS} -fPIC -ggdb -pipe")
    if (USE_OPENMP)
        set(CMAKE_CXX_FLAGS_RELEASE "${WARN_FLAGS} -fPIC -O3 -g -pipe -fopenmp")
    else()
        set(CMAKE_CXX_FLAGS_RELEASE "${WARN_FLAGS} -fPIC -O3 -g -pipe")
    endif()
endif()

include_directories(src/ src/core/ src/bit/)
aux_source_directory(src/ SRC_FILES)
aux_source_directory(src/core/ CORE_SRC_FILES)
aux_source_directory(src/bit/ BIT_SRC_FILES)

if( NOT CMAKE_BUILD_TYPE )
    set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(meowtra ${SRC_FILES} ${CORE_SRC_FILES} ${BIT_SRC_FILES})
