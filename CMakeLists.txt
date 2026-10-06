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
option(GP_BUILD_RENDER_TESTS "Build desktop tests requiring an OpenGL context" OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(BUILD_EXAMPLES OFF CACHE BOOL "Build raylib examples" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "Build raylib games" FORCE)
if(EMSCRIPTEN)
    set(PLATFORM Web CACHE STRING "raylib platform" FORCE)
endif()
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
    src/states/StateMachine.cpp src/render/Letterbox.cpp
    src/world/TileMap.cpp src/world/LevelLoader.cpp
    src/core/Assets.cpp
    src/entities/Player.cpp src/entities/Guard.cpp src/entities/Weapon.cpp
    src/entities/GuardAI.cpp
    src/entities/SecurityCamera.cpp src/entities/Laser.cpp
    src/render/FollowCamera.cpp
    src/world/Raycast.cpp src/systems/RippleSystem.cpp
    src/world/Pathfinder.cpp
    src/systems/NoiseSystem.cpp src/systems/InteractionSystem.cpp
    src/systems/VisionSystem.cpp
    src/systems/DetectionSystem.cpp
    src/systems/AlarmDirector.cpp src/systems/PagerSystem.cpp
    src/systems/LaserSystem.cpp src/systems/CombatSystem.cpp
)
target_include_directories(ghost_core PUBLIC src)
target_link_libraries(ghost_core PRIVATE nlohmann_json::nlohmann_json)
gp_enable_warnings(ghost_core)

add_executable(ghost_game src/main.cpp src/core/Game.cpp src/render/Renderer.cpp
    src/states/BootState.cpp src/states/MenuState.cpp src/states/PlayState.cpp)
target_link_libraries(ghost_game PRIVATE ghost_core raylib)
gp_enable_warnings(ghost_game)
gp_copy_mingw_runtime(ghost_game)
if(EMSCRIPTEN)
    set_target_properties(ghost_game PROPERTIES OUTPUT_NAME index SUFFIX ".html")
    target_link_options(ghost_game PRIVATE
        "--preload-file" "${CMAKE_CURRENT_SOURCE_DIR}/assets@assets"
        "--shell-file" "${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
        "-sINITIAL_MEMORY=134217728" "-sALLOW_MEMORY_GROWTH=1"
        "-sMAXIMUM_MEMORY=536870912"
        "-sEXPORTED_RUNTIME_METHODS=HEAPF32")
    add_custom_target(ghost_web DEPENDS ghost_game)
endif()

add_custom_target(ghost_assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:ghost_game>/assets"
    COMMENT "Copying game assets"
)
add_dependencies(ghost_game ghost_assets)

if(GP_BUILD_TESTS AND NOT EMSCRIPTEN)
    enable_testing()
    FetchContent_Declare(googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
    )
    set(gtest_force_shared_crt ON CACHE BOOL "Use shared MSVC runtime" FORCE)
    FetchContent_MakeAvailable(googletest)
    add_executable(ghost_tests
        tests/ConfigTests.cpp tests/LoggerTests.cpp tests/RngTests.cpp tests/TimeTests.cpp
        tests/StateMachineTests.cpp tests/LetterboxTests.cpp
        tests/TileMapTests.cpp tests/LevelLoaderTests.cpp
        tests/AssetsTests.cpp
        tests/PlayerTests.cpp tests/GuardTests.cpp tests/WeaponTests.cpp
        tests/GuardAITests.cpp
        tests/FollowCameraTests.cpp
        tests/RaycastTests.cpp tests/RippleSystemTests.cpp
        tests/PathfinderTests.cpp
        tests/EventBusTests.cpp tests/NoiseSystemTests.cpp tests/InteractionSystemTests.cpp
        tests/VisionSystemTests.cpp
        tests/DetectionSystemTests.cpp
        tests/TakedownTests.cpp
        tests/AlarmDirectorTests.cpp tests/PagerSystemTests.cpp
        tests/CameraTests.cpp tests/LaserSystemTests.cpp tests/HazardRevealTests.cpp
        tests/StealthRunTests.cpp tests/CombatSystemTests.cpp
    )
    target_link_libraries(ghost_tests PRIVATE ghost_core GTest::gtest_main
        nlohmann_json::nlohmann_json)
    gp_enable_warnings(ghost_tests)
    add_dependencies(ghost_tests ghost_game)
    target_compile_definitions(ghost_tests PRIVATE
        GP_RUNTIME_TEST_DIRECTORY="$<TARGET_FILE_DIR:ghost_game>")
    gp_copy_mingw_runtime(ghost_tests)
    include(GoogleTest)
    gtest_discover_tests(ghost_tests
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        DISCOVERY_TIMEOUT 30
        PROPERTIES TIMEOUT 15
    )
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_test(NAME LevelValidator
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_level.py")
    add_test(NAME LevelValidatorTests
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/ValidateLevelTests.py")
    set_tests_properties(LevelValidator LevelValidatorTests PROPERTIES TIMEOUT 15)
    if(GP_BUILD_RENDER_TESTS)
        add_executable(ghost_render_tests tests/RenderTests.cpp src/render/Renderer.cpp)
        target_link_libraries(ghost_render_tests PRIVATE ghost_core raylib GTest::gtest_main)
        add_dependencies(ghost_render_tests ghost_game)
        gp_enable_warnings(ghost_render_tests)
        target_compile_definitions(ghost_render_tests PRIVATE
            GP_RENDER_OUTPUT_DIRECTORY="$<TARGET_FILE_DIR:ghost_render_tests>")
        gp_copy_mingw_runtime(ghost_render_tests)
        gtest_discover_tests(ghost_render_tests
            WORKING_DIRECTORY "$<TARGET_FILE_DIR:ghost_game>"
            DISCOVERY_TIMEOUT 30 PROPERTIES TIMEOUT 30)
    endif()
endif()
