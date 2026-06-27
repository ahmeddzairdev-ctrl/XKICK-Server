cmake_minimum_required(VERSION 3.21)
project(xkick_server CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W3 /WX /utf-8)
  # offsetof() on the #pragma pack(1) wire structs is intentional and correct - it
  # is the project's byte-exact layout-verification mechanism (static_assert). The
  # structs are non-standard-layout (mixed access / CHeadPacket base), so clang
  # flags it, but the result is well-defined on every target compiler. Suppress
  # only this one diagnostic; everything else is a hard error under /WX.
  add_compile_options(-Wno-invalid-offsetof)
  add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)
  # Static CRT (/MT, /MTd) to match the static-CRT vcpkg triplet (x86-windows-static)
  # and produce a self-contained exe with no VC++ redist dependency. Requires
  # CMP0091 = NEW, which is the default at cmake_minimum_required(3.21). Harmless
  # with a dynamic triplet too (just links the CRT statically).
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
  add_compile_options(-Wall -Wno-unused-parameter -Werror -Wno-invalid-offsetof)
endif()

# Standalone Asio (no Boost) from vcpkg.
find_package(asio CONFIG REQUIRED)
add_compile_definitions(ASIO_STANDALONE)

# MySQL/MariaDB C client. We use MariaDB Connector/C (vcpkg: libmariadb) - a pure-C
# client with no Boost dependency, wire-compatible with MySQL and exposing the same
# <mysql.h> C API.
#
# Prefer the vcpkg CMake config (unofficial-libmariadb): its imported target carries
# every transitive dependency - zlib, and in a STATIC build the Windows TLS/crypto
# import libs (ws2_32, secur32, crypt32, bcrypt, shlwapi) - so the static triplet
# links into a single exe with no DLLs to ship. ${MYSQL_TARGET} is consumed by the
# server targets; it also provides the <mysql.h> include dir transitively.
find_package(unofficial-libmariadb CONFIG QUIET)
if(TARGET unofficial::libmariadb)
  set(MYSQL_TARGET unofficial::libmariadb)
  message(STATUS "MySQL: vcpkg unofficial::libmariadb (transitive deps handled)")
else()
  # Fallback for non-vcpkg environments (system libmariadb / libmysqlclient).
  find_path(MYSQL_INCLUDE_DIR NAMES mysql.h PATH_SUFFIXES mariadb mysql)
  find_library(MYSQL_LIBRARY NAMES libmariadb mariadb libmysql mysqlclient libmysqlclient)
  if(NOT MYSQL_INCLUDE_DIR OR NOT MYSQL_LIBRARY)
    message(FATAL_ERROR "MariaDB/MySQL client not found. Install via: vcpkg install libmariadb")
  endif()
  message(STATUS "MySQL include: ${MYSQL_INCLUDE_DIR}")
  message(STATUS "MySQL library: ${MYSQL_LIBRARY}")
  add_library(xkick_mysql INTERFACE)
  target_include_directories(xkick_mysql INTERFACE ${MYSQL_INCLUDE_DIR})
  target_link_libraries(xkick_mysql INTERFACE ${MYSQL_LIBRARY})
  set(MYSQL_TARGET xkick_mysql)
endif()

find_package(Threads REQUIRED)

add_subdirectory(src/common)
add_subdirectory(src/certify)
add_subdirectory(src/game)     # Phase 5 (Game server)
