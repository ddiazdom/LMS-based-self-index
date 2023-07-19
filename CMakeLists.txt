cmake_minimum_required(VERSION 3.7)
project(LPG_COMPRESSOR)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules/")

find_package(LibSDSL REQUIRED)

add_executable(lpg
        main.cpp
        lib/lpg/lpg_build.cpp
        third-party/xxHash-dev/xxhash.c)

target_compile_options(lpg
        PRIVATE
        #-Werror
        -Wall -Wextra -Wno-ignored-qualifiers -pedantic -Wno-vla-extension -Wno-unused-parameter -O3 -funroll-loops -fomit-frame-pointer -ffast-math
        #-O0
        INTERFACE
        -Wshadow)

#no-stack-check only for APPLE!!!!!!
if(APPLE)
    target_compile_options(lpg PRIVATE -fno-stack-check)
endif()

#compiler-dependent flags
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(STATUS "Disabling vla-extension due to ${CMAKE_CXX_COMPILER_ID}")
    target_compile_options(lpg PUBLIC -Wno-vla-extension -Wno-undefined-var-template)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(STATUS "Adding -march=native compiler flag due to ${CMAKE_CXX_COMPILER_ID}")
    target_compile_options(lpg PUBLIC -march=native)
endif()

if(NOT CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm64")
    target_compile_options(lpg PUBLIC -msse4.2)
else()
    message(STATUS "Disabling SSE4.2 instructions due to conflict with host system processor ${CMAKE_HOST_SYSTEM_PROCESSOR}")
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(lpg LINK_PUBLIC stdc++fs)
endif()

target_link_libraries(lpg LINK_PUBLIC pthread ${LIBSDSL_LIBRARIES})

target_include_directories(lpg PRIVATE ${LIBSDSL_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/include)
target_include_directories(lpg SYSTEM PRIVATE ${LIBSDSL_INCLUDE_DIRS})

