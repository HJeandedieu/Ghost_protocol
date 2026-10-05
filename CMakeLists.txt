cmake_minimum_required(VERSION 3.20)
project(GhostProtocol VERSION 0.1.0 LANGUAGES C CXX)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
option(GP_BUILD_TESTS "Build desktop logic tests" ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(BUILD_EXAMPLES OFF CACHE BOOL "Build raylib examples" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "Build raylib games" FORCE)
FetchContent_Declare(raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
)
FetchContent_MakeAvailable(raylib)

FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(nlohmann_json)

function(gp_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

# Resolve the selected MinGW runtime beside each executable, rather than relying
# on another toolchain's DLLs appearing first in the launching process's PATH.
function(gp_copy_mingw_runtime target)
    if(MINGW)
        get_filename_component(compiler_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
        file(GLOB runtime_dlls
            "${compiler_bin}/libstdc++-6.dll"
            "${compiler_bin}/libgcc_s_*-1.dll"
            "${compiler_bin}/libwinpthread-1.dll"
        )
        foreach(runtime_dll IN LISTS runtime_dlls)
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${runtime_dll}" "$<TARGET_FILE_DIR:${target}>"
                VERBATIM
            )
        endforeach()
    endif()
endfunction()

add_library(ghost_core STATIC
    src/core/Config.cpp src/core/Logger.cpp src/core/Rng.cpp src/core/Time.cpp
)
target_include_directories(ghost_core PUBLIC src)
target_link_libraries(ghost_core PRIVATE nlohmann_json::nlohmann_json)
gp_enable_warnings(ghost_core)

add_executable(ghost_game src/main.cpp src/core/Game.cpp src/render/Renderer.cpp)
target_link_libraries(ghost_game PRIVATE ghost_core raylib)
gp_enable_warnings(ghost_game)
gp_copy_mingw_runtime(ghost_game)

add_custom_command(TARGET ghost_game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:ghost_game>/assets"
    COMMENT "Copying game assets"
)

if(GP_BUILD_TESTS AND NOT EMSCRIPTEN)
    enable_testing()
    FetchContent_Declare(googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
    )
    set(gtest_force_shared_crt ON CACHE BOOL "Use shared MSVC runtime" FORCE)
    FetchContent_MakeAvailable(googletest)
    add_executable(ghost_tests
        tests/ConfigTests.cpp tests/LoggerTests.cpp tests/RngTests.cpp tests/TimeTests.cpp
    )
    target_link_libraries(ghost_tests PRIVATE ghost_core GTest::gtest_main
        nlohmann_json::nlohmann_json)
    gp_enable_warnings(ghost_tests)
    gp_copy_mingw_runtime(ghost_tests)
    include(GoogleTest)
    gtest_discover_tests(ghost_tests
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        DISCOVERY_TIMEOUT 30
        PROPERTIES TIMEOUT 15
    )
endif()
