cmake_minimum_required(VERSION 3.13)

project (level_designer)

add_executable(level_designer
    configreader.cpp
    helperfunctions.cpp
    level.cpp
    main.cpp
)

target_link_libraries(level_designer PRIVATE
    sfml-graphics
    sfml-window
    sfml-system
    sfml-audio
)

target_compile_options(level_designer PRIVATE -std=c++2b -Wall -Wextra -Werror -Wpedantic)


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
