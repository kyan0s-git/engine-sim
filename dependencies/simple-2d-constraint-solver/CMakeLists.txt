cmake_minimum_required(VERSION 3.16)

project(simple-2d-constraint-solver LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB SCS_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
file(GLOB SCS_HEADERS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)

add_library(simple-2d-constraint-solver STATIC ${SCS_SOURCES} ${SCS_HEADERS})
target_include_directories(simple-2d-constraint-solver PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
