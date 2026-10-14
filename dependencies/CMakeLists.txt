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

# Dear ImGui and ImPlot (user interface); SDL2 comes through delta-core
if (ENGINE_SIM_BUILD_APP)
    add_library(imgui STATIC
        imgui/imgui.cpp
        imgui/imgui_draw.cpp
        imgui/imgui_tables.cpp
        imgui/imgui_widgets.cpp
        imgui/backends/imgui_impl_sdl2.cpp
        imgui/backends/imgui_impl_opengl3.cpp
        implot/implot.cpp
        implot/implot_items.cpp)

    target_include_directories(imgui PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/imgui
        ${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends
        ${CMAKE_CURRENT_SOURCE_DIR}/implot)

    # Use 32-bit indices so large plots don't overflow the index buffers
    target_compile_definitions(imgui PUBLIC
        ImDrawIdx=unsigned\ int)

    target_link_libraries(imgui PUBLIC delta-core)
endif()
