cmake_minimum_required(VERSION 3.13)

set(APP_NAME level_designer)
project(${APP_NAME})
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL3 REQUIRED)
find_package(SFML COMPONENTS Graphics Window System Audio REQUIRED)

include(FetchContent)
find_package(SFML 3.0 COMPONENTS System Window Graphics REQUIRED)
FetchContent_Declare(ImGui
    GIT_REPOSITORY https://github.com/ocornut/imgui
    GIT_TAG v1.91.1
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL
    SYSTEM)
FetchContent_MakeAvailable(ImGui)
FetchContent_GetProperties(ImGui SOURCE_DIR IMGUI_DIR)
set(IMGUI_SFML_FIND_SFML OFF)
FetchContent_Declare(ImGui-SFML
    GIT_REPOSITORY https://github.com/SFML/imgui-sfml
    GIT_TAG v3.0
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL
    SYSTEM)
FetchContent_MakeAvailable(ImGui-SFML)

# Silence warnings produced while compiling the fetched dependencies themselves.
# SYSTEM (above) only suppresses warnings when *we* include their headers;
# it doesn't touch the compilation of their own .cpp files.
foreach(dep_target IN ITEMS imgui ImGui-SFML)
    if(TARGET ${dep_target})
        target_compile_options(${dep_target} PRIVATE -w)
    endif()
endforeach()

add_executable(${APP_NAME}
    configreader.cpp
    dialog.cpp
    imgui_dialog.cpp
    level.cpp
    main.cpp
    utils.cpp
)

target_link_libraries(${APP_NAME} PRIVATE
    SFML::Graphics
    SFML::Window
    SFML::System
    SFML::Audio
    SDL3::SDL3
    ImGui-SFML::ImGui-SFML
)

target_compile_options(${APP_NAME} PRIVATE -std=c++23 -Wall -Wextra -Werror -Wpedantic)


ADD_CUSTOM_TARGET(debug
    COMMAND ${CMAKE_COMMAND} -DCMAKE_BUILD_TYPE=Debug ${CMAKE_SOURCE_DIR}
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target all
    COMMENT "Switch CMAKE_BUILD_TYPE to Debug"
)

ADD_CUSTOM_TARGET(release
    COMMAND ${CMAKE_COMMAND} -DCMAKE_BUILD_TYPE=Release ${CMAKE_SOURCE_DIR}
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target all
    COMMENT "Switch CMAKE_BUILD_TYPE to Release"
)

# Symlink to the binary
add_custom_command(
    TARGET ${APP_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        $<TARGET_FILE:${APP_NAME}>
        ${CMAKE_SOURCE_DIR}/$<TARGET_FILE_NAME:${APP_NAME}>
    COMMENT "Updating symlink: $<TARGET_FILE_NAME:${APP_NAME}> -> $<TARGET_FILE:${APP_NAME}>"
)
add_custom_command(
    TARGET ${APP_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${CMAKE_SOURCE_DIR}/compile_commands.json
    COMMENT "Updating symlink: compile_commands.json"
)