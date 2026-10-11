# Third-party dependencies (vendored from the upstream ange-yaghi repositories
# and adapted for cross-platform builds; see dependencies/README.md)

add_subdirectory(simple-2d-constraint-solver)
add_subdirectory(csv-io)

if (ENGINE_SIM_BUILD_APP)
    add_subdirectory(delta-studio)
endif()

if (PIRANHA_ENABLED)
    add_subdirectory(piranha)
endif()

if (DTV)
    add_subdirectory(direct-to-video)
endif()

if (DISCORD_ENABLED)
    add_subdirectory(discord)
endif()
