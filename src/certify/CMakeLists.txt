# XKICK_Certify - auth/account server (executable target).
#
# Phase 3 complete: the full reconstruction links as XKICK_Certify against the
# shared kicks_common static lib (secure, LogManager, database, AhnHS_stub,
# HackShield). All Asio/OS code is confined to connect.cpp.
set(CERTIFY_SOURCES
    src/globals.cpp
    src/config.cpp
    src/thread.cpp
    src/init.cpp
    src/util.cpp           # Certify-only GetCommandString/IsCommandString
    src/sql.cpp
    src/connect.cpp
    src/HackShield.cpp     # AhnLab HackShield call sites (CInit::*Hack, Packet*Hack*)
    src/load.cpp
    src/member.cpp
    src/server.cpp
    src/packet.cpp
    src/process.cpp
    src/main.cpp
)

add_executable(XKICK_Certify ${CERTIFY_SOURCES})

target_include_directories(XKICK_Certify PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(XKICK_Certify PRIVATE kicks_common)
