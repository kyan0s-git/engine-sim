# Discord Rich Presence. Only a prebuilt Windows/MSVC library is bundled.
add_library(discord STATIC
    Discord.cpp
    Discord.h
    discord_register.h
    discord_rpc.h
)

target_include_directories(discord PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Discord.h also requests the library through #pragma comment(lib, ...)
target_link_directories(discord PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/lib)
target_link_libraries(discord PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/lib/discord-rpc.lib)
