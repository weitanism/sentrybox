cmake_minimum_required(VERSION 3.20)

project(fat32
    VERSION 0.1.0
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig)
pkg_check_modules(FUSE REQUIRED fuse3)

find_package(absl REQUIRED)
find_package(argparse REQUIRED)
find_package(spdlog REQUIRED)

add_executable(fat32 fat32.cc fat32_fuse.cc main.cc)
target_include_directories(fat32 PRIVATE
  ${FUSE_INCLUDE_DIRS})
target_link_libraries(fat32 PRIVATE
  argparse::argparse
  absl::strings
  spdlog::spdlog
  ${FUSE_LIBRARIES})
target_compile_options(fat32 PRIVATE -Wall -Wextra -Wpedantic -Werror)

install(TARGETS fat32 DESTINATION bin)
