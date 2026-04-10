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

FetchContent_Declare(
    entt
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG v3.13.2
)
FetchContent_MakeAvailable(entt)

set(SOURCES
    src/main.cpp
    src/GameState.cpp
    src/ECS/Systems/TimeSystem.cpp
    src/ECS/Systems/NeedDrainSystem.cpp
    src/ECS/Systems/ConsumptionSystem.cpp
    src/ECS/Systems/AgentDecisionSystem.cpp
    src/ECS/Systems/DeathSystem.cpp
    src/ECS/Systems/MovementSystem.cpp
    src/ECS/Systems/ProductionSystem.cpp
    src/ECS/Systems/ScheduleSystem.cpp
    src/ECS/Systems/TransportSystem.cpp
    src/ECS/Systems/CameraSystem.cpp
    src/ECS/Systems/RenderSystem.cpp
    src/ECS/Systems/PlayerInputSystem.cpp
    src/World/WorldGenerator.cpp
    src/UI/HUD.cpp
)

add_executable(ReachingUniversalis ${SOURCES})

target_include_directories(ReachingUniversalis PRIVATE src)
target_link_libraries(ReachingUniversalis PRIVATE raylib EnTT::EnTT)

if(WIN32)
    target_link_libraries(ReachingUniversalis PRIVATE winmm gdi32 opengl32)
endif()
