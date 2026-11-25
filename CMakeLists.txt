cmake_minimum_required(VERSION 3.20)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(APP_NAME level_designer)
project(${APP_NAME})
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Gui)

add_executable(${APP_NAME}
    configreader.cpp
    level.cpp
    levelfile.cpp
    levelwidget.cpp
    main.cpp
    mainwindow.cpp
    utils.cpp
)

# ---------------------------------------------------------------------------
# macOS app bundle (Darwin only)
# ---------------------------------------------------------------------------
if(APPLE)
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

    add_custom_command(TARGET ${APP_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/level_designer.cfg
            $<TARGET_BUNDLE_CONTENT_DIR:${APP_NAME}>/Resources/level_designer.cfg
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/data
            $<TARGET_BUNDLE_CONTENT_DIR:${APP_NAME}>/Resources/data
        COMMENT "Copying resources into app bundle"
    )

    # macdeployqt copies Qt and other dylibs into the bundle. It's slow, so
    # only do it for release builds; debug builds run against the installed
    # libraries.
    #
    # By default macdeployqt copies every plugin for the Qt modules used, many
    # of which we don't need, so we copy in just the plugins we need, and have
    # macdeployqt deploy their dependencies too.
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        find_program(MACDEPLOYQT macdeployqt REQUIRED
            HINTS "${Qt6_DIR}/../../../bin")
        # Pairs of plugin type (directory) and plugin target
        set(BUNDLED_QT_PLUGINS
            platforms Qt6::QCocoaIntegrationPlugin
            styles Qt6::QMacStylePlugin
        )
        set(PLUGINS_DIR $<TARGET_BUNDLE_CONTENT_DIR:${APP_NAME}>/PlugIns)
        set(COPY_PLUGINS)
        set(DEPLOY_PLUGINS)
        list(LENGTH BUNDLED_QT_PLUGINS count)
        math(EXPR last "${count} - 1")
        foreach(i RANGE 0 ${last} 2)
            math(EXPR j "${i} + 1")
            list(GET BUNDLED_QT_PLUGINS ${i} type)
            list(GET BUNDLED_QT_PLUGINS ${j} plugin)
            list(APPEND COPY_PLUGINS
                COMMAND ${CMAKE_COMMAND} -E make_directory ${PLUGINS_DIR}/${type}
                COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${plugin}> ${PLUGINS_DIR}/${type}/
            )
            list(APPEND DEPLOY_PLUGINS
                -executable=${PLUGINS_DIR}/${type}/$<TARGET_FILE_NAME:${plugin}>
            )
        endforeach()
        add_custom_command(TARGET ${APP_NAME} POST_BUILD
            # Start afresh, so nothing is left over from a previous deployment
            COMMAND ${CMAKE_COMMAND} -E rm -rf
                ${PLUGINS_DIR} $<TARGET_BUNDLE_CONTENT_DIR:${APP_NAME}>/Frameworks
            ${COPY_PLUGINS}
            COMMAND ${MACDEPLOYQT} $<TARGET_BUNDLE_DIR:${APP_NAME}> -no-plugins ${DEPLOY_PLUGINS}
            # macdeployqt's own (ad hoc) signing leaves libbrotlicommon
            # unsigned (see the rpath note above), and macOS kills the app at
            # launch if any of its code has an invalid signature, so re-sign
            # the whole bundle
            COMMAND codesign --force --deep --sign - $<TARGET_BUNDLE_DIR:${APP_NAME}>
            COMMENT "Bundling app"
        )
    endif()
endif()

target_compile_options(${APP_NAME} PRIVATE
    -Wall
    -Wextra
    -Werror
    -Wpedantic
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O3>
)
target_link_libraries(${APP_NAME} PRIVATE Qt6::Widgets)

# ---------------------------------------------------------------------------
# Unit tests (built if Catch2 is available; run with "make test")
# ---------------------------------------------------------------------------
find_package(Catch2 3 QUIET)
if(Catch2_FOUND)
    add_executable(${APP_NAME}_tests
        tests/levelfile_tests.cpp
        tests/level_tests.cpp
        level.cpp
        levelfile.cpp
        utils.cpp
    )
    target_include_directories(${APP_NAME}_tests PRIVATE ${CMAKE_SOURCE_DIR})
    target_compile_definitions(${APP_NAME}_tests PRIVATE
        SOURCE_DIR="${CMAKE_SOURCE_DIR}"
        # amaze's levels, if amaze is alongside (tests using them are skipped otherwise)
        AMAZE_DATA_DIR="${CMAKE_SOURCE_DIR}/../amaze/data"
    )
    # Tests use brief initialisers, relying on fields' defaults
    target_compile_options(${APP_NAME}_tests PRIVATE
        -Wall -Wextra -Werror -Wpedantic -Wno-missing-field-initializers)
    target_link_libraries(${APP_NAME}_tests PRIVATE Catch2::Catch2WithMain Qt6::Gui)
else()
    message(STATUS "Catch2 not found: unit tests won't be built")
endif()

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
