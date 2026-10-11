file(GLOB DELTA_BASIC_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
file(GLOB DELTA_BASIC_HEADERS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)

add_library(delta-basic STATIC ${DELTA_BASIC_SOURCES} ${DELTA_BASIC_HEADERS})

target_link_libraries(delta-basic PUBLIC
    delta-core
    delta-physics)
