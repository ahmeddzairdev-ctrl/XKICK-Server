# XKICK_Game - lobby/room/persistence server (executable target).
#
# Reuses the shared kicks_common static lib (secure, LogManager, database,
# AhnHS_stub, table, md5, que, util) - NO duplication of infra. Game-specific
# modules (config/init/thread/connect/sql/load/process/packet/player/server +
# domain: item/skill/ceremony/training/quest/room/lobby/rndshop) live here.
#
# Sources are globbed so the target stays configurable while modules are
# reconstructed incrementally; re-run cmake after adding a new .cpp.
file(GLOB GAME_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

if(GAME_SOURCES)
    add_executable(XKICK_Game ${GAME_SOURCES})
    target_include_directories(XKICK_Game PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(XKICK_Game PRIVATE kicks_common)
else()
    message(STATUS "XKICK_Game: no sources yet (Phase 5 in progress) - target skipped")
endif()
