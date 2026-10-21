cmake_minimum_required(VERSION 3.20)
project(chairs CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(SKEIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../skein" CACHE PATH "Path to a Skein checkout")
add_subdirectory(${SKEIN_DIR} skein)

add_library(chairs_world STATIC src/kit.cpp src/meadow.cpp src/stage.cpp src/shots.cpp src/game.cpp src/bots.cpp src/fx.cpp src/view.cpp src/take.cpp src/brain.cpp)
target_include_directories(chairs_world PUBLIC src)
target_link_libraries(chairs_world PUBLIC skein)
target_compile_options(chairs_world PRIVATE -Wall -Wextra -Wno-unused-parameter)

add_executable(chairs src/main.cpp)
target_link_libraries(chairs PRIVATE chairs_world skein_gl)
target_compile_options(chairs PRIVATE -Wall -Wextra -Wno-unused-parameter)

add_executable(chairs_sim tools/sim.cpp)
target_link_libraries(chairs_sim PRIVATE chairs_world)
target_compile_options(chairs_sim PRIVATE -Wall -Wextra -Wno-unused-parameter)

add_library(chairs_env SHARED src/env.cpp)
target_link_libraries(chairs_env PRIVATE chairs_world)
target_compile_options(chairs_env PRIVATE -Wall -Wextra -Wno-unused-parameter)
