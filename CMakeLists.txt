cmake_minimum_required(VERSION 3.21)
project(
  cpp-io-uring-proactor
  VERSION 0.1.0
  LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(ExternalProject)

set(LIBURING_PREFIX ${CMAKE_BINARY_DIR}/liburing)
set(LIBURING_TAR ${CMAKE_SOURCE_DIR}/archives/liburing-2.9.tar.gz)

ExternalProject_Add(
  liburing
  PREFIX ${LIBURING_PREFIX}
  URL ${LIBURING_TAR}
  URL_HASH "MD5=50cbca5567878abaeb6824909b647422"
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  CONFIGURE_COMMAND cd ${LIBURING_PREFIX}/src/liburing && ./configure
                    --prefix=${LIBURING_PREFIX}
  BUILD_COMMAND make -C ${LIBURING_PREFIX}/src/liburing -j$(nproc)
  INSTALL_COMMAND make -C ${LIBURING_PREFIX}/src/liburing install
  UPDATE_DISCONNECTED 1)

file(GLOB_RECURSE SRCS src/**.cpp)

add_executable(cpp-io-uring-proactor ${SRCS})
add_dependencies(cpp-io-uring-proactor liburing)

target_compile_options(
  cpp-io-uring-proactor
  PRIVATE -Wall
          -Wextra
          -Werror
          -Wattributes
          -Wconversion
          -Wduplicated-cond
          -Wduplicated-branches
          -Wformat
          -Wimplicit-fallthrough
          -Wpedantic)

find_library(LIB_RT NAMES rt REQUIRED)
# find_library(LIB_IO_URING NAMES uring)

target_include_directories(
  cpp-io-uring-proactor
  PRIVATE src/
  PRIVATE SYSTEM ${LIBURING_PREFIX}/include)

target_link_directories(cpp-io-uring-proactor PRIVATE ${LIBURING_PREFIX}/lib)

target_link_libraries(cpp-io-uring-proactor PRIVATE ${LIB_RT} liburing.a)
