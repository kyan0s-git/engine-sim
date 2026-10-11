cmake_minimum_required(VERSION 3.16)

project(piranha LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The scanner and parser are generated with Flex/Bison. Pre-generated copies
# are kept in `generated/` so that building doesn't require these tools
# (macOS ships an ancient Bison and Windows has none at all). Turn this on
# after editing anything in `flex-bison/`.
option(PIRANHA_REGENERATE_PARSER "Regenerate the scanner/parser with Flex and Bison" OFF)

if (PIRANHA_REGENERATE_PARSER)
    find_package(FLEX REQUIRED)
    find_package(BISON 3.2 REQUIRED)

    flex_target(
        lexer
        ${CMAKE_CURRENT_SOURCE_DIR}/flex-bison/scanner.l
        ${CMAKE_CURRENT_BINARY_DIR}/generated/scanner.auto.cpp
        DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/generated/scanner.auto.h)

    bison_target(
        parser
        ${CMAKE_CURRENT_SOURCE_DIR}/flex-bison/specification.y
        ${CMAKE_CURRENT_BINARY_DIR}/generated/parser.auto.cpp
        DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/generated/parser.auto.h)
    add_flex_bison_dependency(lexer parser)

    set(PIRANHA_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
    set(PIRANHA_GENERATED_SOURCES ${BISON_parser_OUTPUTS} ${FLEX_lexer_OUTPUTS})
else()
    set(PIRANHA_GENERATED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/generated)
    set(PIRANHA_GENERATED_SOURCES
        ${PIRANHA_GENERATED_DIR}/parser.auto.cpp
        ${PIRANHA_GENERATED_DIR}/scanner.auto.cpp)
endif()

file(GLOB PIRANHA_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
file(GLOB PIRANHA_HEADERS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)

add_library(piranha STATIC
    ${PIRANHA_SOURCES}
    ${PIRANHA_HEADERS}
    ${PIRANHA_GENERATED_SOURCES})

target_include_directories(piranha
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${PIRANHA_GENERATED_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/libraries/flex/include)

if (MSVC)
    target_compile_definitions(piranha PUBLIC _CRT_SECURE_NO_WARNINGS)
endif()
