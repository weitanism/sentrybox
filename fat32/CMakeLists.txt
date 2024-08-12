cmake_minimum_required(VERSION 3.20)

project(fat32
    VERSION 0.1.0
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(absl REQUIRED)
find_package(argparse REQUIRED)

add_library(libfat32 STATIC fat32.cc)

add_executable(fat32 fat32.cc main.cc)
target_link_libraries(fat32 PRIVATE argparse::argparse absl::strings)
target_compile_options(fat32 PRIVATE -Wall -Wextra -Wpedantic -Werror)

