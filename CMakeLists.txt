cmake_minimum_required(VERSION 3.16)
project(ReachingUniversalis CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

set(SOURCES
    src/main.cpp
    src/GameState.cpp
    src/World.cpp
    src/NPC.cpp
    src/ResourceNode.cpp
    src/HUD.cpp
)

add_executable(ReachingUniversalis ${SOURCES})

target_include_directories(ReachingUniversalis PRIVATE src)
target_link_libraries(ReachingUniversalis PRIVATE raylib)

if(WIN32)
    target_link_libraries(ReachingUniversalis PRIVATE winmm gdi32 opengl32)
endif()
