cmake_minimum_required(VERSION 3.20)
project(GhostProtocol VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "Build raylib examples" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "Build raylib games" FORCE)
FetchContent_Declare(raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
)
FetchContent_MakeAvailable(raylib)

add_executable(ghost_game src/main.cpp)
target_link_libraries(ghost_game PRIVATE raylib)
if(MSVC)
    target_compile_options(ghost_game PRIVATE /W4)
else()
    target_compile_options(ghost_game PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_custom_command(TARGET ghost_game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:ghost_game>/assets"
    COMMENT "Copying game assets"
)
