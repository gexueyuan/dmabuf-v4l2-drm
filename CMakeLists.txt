cmake_minimum_required(VERSION 3.10)
project(drm_v4l2)

set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
set(CMAKE_C_COMPILER /home/rlk/toolchains/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-gcc)
set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_CXX_COMPILER_WORKS 1)

set(CMAKE_C_FLAGS "-Wall -O0 -g -I/usr/local/include -I/usr/local/include/libdrm -I/home/rlk/toolchains/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc/usr/include/drm/ -L/usr/local/lib/aarch64-linux-gnu -ldrm")
#include_directories(/usr/src/linux-headers-5.15.0-107-generic/include) 
set(CMAKE_CXX_STANDARD C14)

aux_source_directory(. SRC)
include_directories(./)
add_executable(drm_v4l2 ${SRC})

