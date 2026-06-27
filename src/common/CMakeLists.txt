# Shared infrastructure used by both Certify and Game.
# Sources that are reconstructed/ported and compile cross-platform.
add_library(kicks_common STATIC
    src/secure.cpp
    src/LogManager.cpp
    src/database.cpp
    src/AhnHS_stub.cpp     # dummy HackShield SDK (compiled out if XKICK_USE_REAL_HACKSHIELD)
    src/table.cpp          # portable CSV table loader (Table_* API)
    src/md5.cpp            # portable MD5 (password hashing) - shared by both servers
    src/que.cpp            # CQue/CBufferPool packet ring (Que.h) - shared by both servers
    src/util.cpp           # project-agnostic helpers (Util.h): Tokener/IPTokener/ExitMessage
    # NOTE: HackShield.cpp (the call sites) is per-server (touches CInit/CMember/
    # CConfig), so it lives in each server target, not here. Per-server util.cpp
    # holds only GetCommandString/IsCommandString (command tables differ).
)

target_include_directories(kicks_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ${MYSQL_TARGET} (vcpkg unofficial::libmariadb, or the xkick_mysql fallback) brings
# its own <mysql.h> include dir and all transitive link deps with it.
target_link_libraries(kicks_common PUBLIC
    asio::asio
    ${MYSQL_TARGET}
    Threads::Threads
)

if(WIN32)
    target_link_libraries(kicks_common PUBLIC ws2_32 mswsock)
endif()
