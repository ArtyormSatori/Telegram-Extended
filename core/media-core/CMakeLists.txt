cmake_minimum_required(VERSION 3.20)
project(telegram_media_core VERSION 0.1.0 LANGUAGES CXX)

option(TELEGRAM_MEDIA_BUILD_TESTS "Build Shared Media Core tests" ON)

find_package(Threads REQUIRED)

add_library(telegram_media_core
    src/planner.cpp
    src/ffprobe_adapter.cpp
    src/process_runner.cpp
    src/capability_registry.cpp
    src/ffmpeg_command_builder.cpp
)
target_include_directories(telegram_media_core PUBLIC include)
target_compile_features(telegram_media_core PUBLIC cxx_std_20)
target_link_libraries(telegram_media_core PRIVATE Threads::Threads)
set_target_properties(telegram_media_core PROPERTIES CXX_EXTENSIONS OFF)
if(MSVC)
    target_compile_options(telegram_media_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(telegram_media_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()

if(TELEGRAM_MEDIA_BUILD_TESTS)
    enable_testing()
    add_executable(media_core_tests tests/planner_tests.cpp)
    target_link_libraries(media_core_tests PRIVATE telegram_media_core)
    add_test(NAME media_core_tests COMMAND media_core_tests)

    add_executable(process_runner_tests tests/process_runner_tests.cpp)
    target_link_libraries(process_runner_tests PRIVATE telegram_media_core)
    target_compile_definitions(process_runner_tests PRIVATE TELEGRAM_MEDIA_CMAKE_COMMAND="${CMAKE_COMMAND}")
    find_program(TELEGRAM_MEDIA_FFMPEG_EXECUTABLE ffmpeg)
    find_program(TELEGRAM_MEDIA_FFPROBE_EXECUTABLE ffprobe)
    if(TELEGRAM_MEDIA_FFMPEG_EXECUTABLE AND TELEGRAM_MEDIA_FFPROBE_EXECUTABLE)
        target_compile_definitions(process_runner_tests PRIVATE
            TELEGRAM_MEDIA_FFMPEG_EXECUTABLE="${TELEGRAM_MEDIA_FFMPEG_EXECUTABLE}"
            TELEGRAM_MEDIA_FFPROBE_EXECUTABLE="${TELEGRAM_MEDIA_FFPROBE_EXECUTABLE}")
    endif()
    add_test(NAME process_runner_tests COMMAND process_runner_tests)

    add_executable(capability_registry_tests tests/capability_registry_tests.cpp)
    target_link_libraries(capability_registry_tests PRIVATE telegram_media_core)
    find_program(TELEGRAM_MEDIA_FFMPEG_EXECUTABLE ffmpeg)
    if(TELEGRAM_MEDIA_FFMPEG_EXECUTABLE)
        target_compile_definitions(capability_registry_tests PRIVATE
            TELEGRAM_MEDIA_FFMPEG_EXECUTABLE="${TELEGRAM_MEDIA_FFMPEG_EXECUTABLE}")
    endif()
    add_test(NAME capability_registry_tests COMMAND capability_registry_tests)

    add_executable(ffmpeg_command_builder_tests tests/ffmpeg_command_builder_tests.cpp)
    target_link_libraries(ffmpeg_command_builder_tests PRIVATE telegram_media_core)
    add_test(NAME ffmpeg_command_builder_tests COMMAND ffmpeg_command_builder_tests)
endif()
