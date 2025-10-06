cmake_minimum_required(VERSION 3.22)
project(CHIP8emu)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(ext/SDL)
add_subdirectory(src)

include_directories(src)

add_executable(${PROJECT_NAME}_example)
target_sources(${PROJECT_NAME}_example PRIVATE main.cpp)
target_link_libraries("${PROJECT_NAME}_example"
    PRIVATE ${PROJECT_NAME}_lib
    PRIVATE SDL3::SDL3
)