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

# ---------------------------------------------------------------------------
# macOS app bundle (Darwin only)
# ---------------------------------------------------------------------------
if(APPLE)
    find_program(DYLIBBUNDLER dylibbundler REQUIRED)

    set_target_properties(${APP_NAME} PROPERTIES
        MACOSX_BUNDLE                      TRUE
        MACOSX_BUNDLE_BUNDLE_NAME          "${APP_NAME}"
        MACOSX_BUNDLE_GUI_IDENTIFIER       "cc.marengo.${APP_NAME}"
        MACOSX_BUNDLE_BUNDLE_VERSION       "1.0.0"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
        MACOSX_BUNDLE_ICON_FILE            "ld.icns"
    )

    target_sources(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/artwork/ld.icns)
    set_source_files_properties(${CMAKE_SOURCE_DIR}/artwork/ld.icns PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )

    # Write a quiet wrapper for dylibbundler at configure time
    file(WRITE "${CMAKE_BINARY_DIR}/dylibbundler.cmake" "
        execute_process(
            COMMAND ${DYLIBBUNDLER}
                -od -b
                -x \${BINARY}
                -d \${FRAMEWORKS}
                -p @executable_path/../Frameworks
            OUTPUT_QUIET
            ERROR_QUIET
        )
    ")

    add_custom_command(TARGET ${APP_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/level_designer.cfg
            $<TARGET_BUNDLE_CONTENT_DIR:${APP_NAME}>/Resources/level_designer.cfg
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/data
            $<TARGET_BUNDLE_CONTENT_DIR:${APP_NAME}>/Resources/data
        COMMAND ${CMAKE_COMMAND}
            -DBINARY=$<TARGET_FILE:${APP_NAME}>
            -DFRAMEWORKS=$<TARGET_BUNDLE_CONTENT_DIR:${APP_NAME}>/Frameworks
            -P "${CMAKE_BINARY_DIR}/dylibbundler.cmake"
        COMMENT "Bundling app"
    )
endif()

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

# Symlink to the app bundle (macOS) or binary (other platforms)
add_custom_command(
    TARGET ${APP_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        $<IF:$<BOOL:${APPLE}>,$<TARGET_BUNDLE_DIR:${APP_NAME}>,$<TARGET_FILE:${APP_NAME}>>
        ${CMAKE_SOURCE_DIR}/$<IF:$<BOOL:${APPLE}>,$<TARGET_BUNDLE_DIR_NAME:${APP_NAME}>,$<TARGET_FILE_NAME:${APP_NAME}>>
    COMMENT "Updating symlink in source dir"
)
add_custom_command(
    TARGET ${APP_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${CMAKE_SOURCE_DIR}/compile_commands.json
    COMMENT "Updating symlink: compile_commands.json"
)