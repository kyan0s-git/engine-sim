cmake_minimum_required(VERSION 3.16)

project(csv-io LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(csv-io STATIC
    src/csv_data.cpp
    src/information.cpp

    include/csv_data.h
    include/information.h
)

target_compile_definitions(csv-io PRIVATE PROJECT_NAMESPACE=atg_csv)
target_include_directories(csv-io PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
