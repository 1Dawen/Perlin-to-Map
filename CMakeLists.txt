cmake_minimum_required(VERSION 3.20)
project(PerlinTerrainGenerator VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(TERRAIN_BUILD_VIEWER "Build OpenGL 3.3 viewer (requires graphical dependencies)" OFF)
option(TERRAIN_BUILD_TESTS "Build deterministic terrain-core tests" ON)

add_library(terrain_core STATIC src/terrain.cpp)
target_include_directories(terrain_core PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
if(MSVC)
    target_compile_options(terrain_core PRIVATE /W4)
else()
    target_compile_options(terrain_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(terrain_cli src/terrain_cli.cpp)
target_link_libraries(terrain_cli PRIVATE terrain_core)

if(TERRAIN_BUILD_TESTS)
    enable_testing()
    add_executable(terrain_tests tests/test_terrain.cpp)
    target_link_libraries(terrain_tests PRIVATE terrain_core)
    add_test(NAME terrain_core_tests COMMAND terrain_tests)
    add_test(NAME terrain_cli_smoke COMMAND terrain_cli --grid 8 --seed 42 --island
             --output "${CMAKE_CURRENT_BINARY_DIR}/ctest/terrain.ppm"
             --obj "${CMAKE_CURRENT_BINARY_DIR}/ctest/terrain.obj")
endif()

if(TERRAIN_BUILD_VIEWER)
    find_package(OpenGL REQUIRED)
    find_package(glad CONFIG REQUIRED)
    find_package(glfw3 CONFIG REQUIRED)
    find_package(glm CONFIG REQUIRED)
    find_package(imgui CONFIG REQUIRED)
    add_executable(terrain_viewer src/viewer.cpp)
    target_link_libraries(terrain_viewer PRIVATE terrain_core OpenGL::GL glad::glad glfw glm::glm imgui::imgui)
endif()
