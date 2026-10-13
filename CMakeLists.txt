cmake_minimum_required(VERSION 3.16)

project(engine-sim
    VERSION 0.2.0
    DESCRIPTION "Real-time internal combustion engine simulator"
    LANGUAGES C CXX)

# ========================================================
# Options

option(ENGINE_SIM_BUILD_APP "Build the graphical application" ON)
option(ENGINE_SIM_BUILD_TESTS "Build the unit tests (downloads GoogleTest)" ON)
option(PIRANHA_ENABLED "Enable scripting input" ON)
option(DTV "Enable video output (requires FFmpeg)" OFF)
option(ENGINE_SIM_BUNDLED_SDL "Download SDL2 and link it statically (self-contained release builds)" OFF)

# Discord Rich Presence is only available through the bundled prebuilt library
# which requires MSVC with the release (/MD) runtime
if (MSVC)
    option(DISCORD_ENABLED "Enable Discord Rich Presence (MSVC release builds only)" OFF)
else()
    set(DISCORD_ENABLED OFF CACHE BOOL "Enable Discord Rich Presence (MSVC release builds only)" FORCE)
endif()

# Default to an optimized build; the simulation is far too slow in Debug
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "cmake")

if (MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX _USE_MATH_DEFINES)
    add_compile_options(/MP /utf-8)
endif()

# ========================================================
# Dependencies

if (ENGINE_SIM_BUILD_APP AND ENGINE_SIM_BUNDLED_SDL)
    include(FetchContent)

    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_STATIC_PIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
    set(SDL2_DISABLE_UNINSTALL ON CACHE BOOL "" FORCE)

    FetchContent_Declare(
        SDL2
        URL https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-2.32.10.tar.gz
        URL_HASH SHA256=5f5993c530f084535c65a6879e9b26ad441169b3e25d789d83287040a9ca5165
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(SDL2)
endif()

add_subdirectory(dependencies)

# ========================================================
# Simulation core (no UI or platform dependencies)

add_library(engine-sim STATIC
    # Source files
    src/audio_buffer.cpp
    src/camshaft.cpp
    src/crankshaft.cpp
    src/combustion_chamber.cpp
    src/connecting_rod.cpp
    src/convolution_filter.cpp
    src/cylinder_bank.cpp
    src/cylinder_head.cpp
    src/delay_filter.cpp
    src/derivative_filter.cpp
    src/direct_throttle_linkage.cpp
    src/drivetrain_spring.cpp
    src/dynamometer.cpp
    src/engine.cpp
    src/engine_friction.cpp
    src/engine_metrics.cpp
    src/exhaust_system.cpp
    src/feedback_comb_filter.cpp
    src/filter.cpp
    src/fuel.cpp
    src/function.cpp
    src/gas_system.cpp
    src/gaussian_filter.cpp
    src/governor.cpp
    src/ignition_module.cpp
    src/impulse_response.cpp
    src/intake.cpp
    src/jitter_filter.cpp
    src/leveling_filter.cpp
    src/low_pass_filter.cpp
    src/part.cpp
    src/piston.cpp
    src/piston_engine_simulator.cpp
    src/wankel_combustion_chamber.cpp
    src/wankel_engine.cpp
    src/wankel_engine_simulator.cpp
    src/wankel_rotor.cpp
    src/simulator.cpp
    src/standard_valvetrain.cpp
    src/starter_motor.cpp
    src/synthesizer.cpp
    src/thermodynamics.cpp
    src/throttle.cpp
    src/transmission.cpp
    src/utilities.cpp
    src/valvetrain.cpp
    src/vehicle.cpp
    src/vehicle_drag_constraint.cpp
    src/vtec_valvetrain.cpp

    # Include files
    include/audio_buffer.h
    include/application_settings.h
    include/camshaft.h
    include/crankshaft.h
    include/combustion_chamber.h
    include/connecting_rod.h
    include/convolution_filter.h
    include/cylinder_bank.h
    include/cylinder_head.h
    include/delay_filter.h
    include/derivative_filter.h
    include/direct_throttle_linkage.h
    include/drivetrain_spring.h
    include/dynamometer.h
    include/engine.h
    include/engine_friction.h
    include/engine_metrics.h
    include/exhaust_system.h
    include/feedback_comb_filter.h
    include/filter.h
    include/fuel.h
    include/function.h
    include/gas_system.h
    include/gaussian_filter.h
    include/governor.h
    include/ignition_module.h
    include/impulse_response.h
    include/intake.h
    include/jitter_filter.h
    include/leveling_filter.h
    include/low_pass_filter.h
    include/part.h
    include/piston.h
    include/piston_engine_simulator.h
    include/wankel_combustion_chamber.h
    include/wankel_engine.h
    include/wankel_engine_simulator.h
    include/wankel_rotor.h
    include/simulator.h
    include/standard_valvetrain.h
    include/starter_motor.h
    include/synthesizer.h
    include/thermodynamics.h
    include/throttle.h
    include/transmission.h
    include/units.h
    include/utilities.h
    include/valvetrain.h
    include/vehicle.h
    include/vehicle_drag_constraint.h
    include/vtec_valvetrain.h
)

target_link_libraries(engine-sim PUBLIC
    simple-2d-constraint-solver
    csv-io)

target_include_directories(engine-sim PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/dependencies)

if (NOT MSVC)
    find_package(Threads REQUIRED)
    target_link_libraries(engine-sim PUBLIC Threads::Threads)
endif()

# ========================================================
# Scripting

if (PIRANHA_ENABLED)
    add_library(engine-sim-script-interpreter STATIC
        # Source files
        scripting/src/channel_types.cpp
        scripting/src/compiler.cpp
        scripting/src/engine_context.cpp
        scripting/src/language_rules.cpp

        # Include files
        scripting/include/actions.h
        scripting/include/camshaft_node.h
        scripting/include/channel_types.h
        scripting/include/compiler.h
        scripting/include/connecting_rod_node.h
        scripting/include/crankshaft_node.h
        scripting/include/cylinder_bank_node.h
        scripting/include/cylinder_head_node.h
        scripting/include/engine_context.h
        scripting/include/engine_node.h
        scripting/include/engine_sim.h
        scripting/include/exhaust_system_node.h
        scripting/include/function_node.h
        scripting/include/ignition_module_node.h
        scripting/include/ignition_wire_node.h
        scripting/include/impulse_response_node.h
        scripting/include/intake_node.h
        scripting/include/language_rules.h
        scripting/include/node.h
        scripting/include/object_reference_node.h
        scripting/include/object_reference_node_output.h
        scripting/include/piranha.h
        scripting/include/piston_node.h
        scripting/include/rod_journal_node.h
        scripting/include/standard_valvetrain_node.h
        scripting/include/transmission_node.h
        scripting/include/valvetrain_node.h
        scripting/include/vehicle_node.h
        scripting/include/vtec_valvetrain_node.h
    )

    target_compile_definitions(engine-sim-script-interpreter PUBLIC ATG_ENGINE_SIM_PIRANHA_ENABLED)

    target_link_libraries(engine-sim-script-interpreter PUBLIC
        engine-sim
        piranha)
endif()

# ========================================================
# Tools

if (PIRANHA_ENABLED)
    # Compiles and simulates engine scripts without a window (used by CI)
    add_executable(engine-sim-script-check tools/script_check.cpp)
    target_link_libraries(engine-sim-script-check PRIVATE engine-sim-script-interpreter)

    add_executable(engine-sim-dyno tools/dyno.cpp)
    target_link_libraries(engine-sim-dyno PRIVATE engine-sim-script-interpreter)
endif()

# ========================================================
# Application

if (ENGINE_SIM_BUILD_APP)
    add_executable(engine-sim-app WIN32
        # Source files
        src/main.cpp
        src/engine_sim_application.cpp
        src/geometry_generator.cpp
        src/simulation_object.cpp
        src/piston_object.cpp
        src/connecting_rod_object.cpp
        src/ui_element.cpp
        src/ui_manager.cpp
        src/cylinder_pressure_gauge.cpp
        src/ui_math.cpp
        src/gauge.cpp
        src/crankshaft_object.cpp
        src/cylinder_bank_object.cpp
        src/cylinder_head_object.cpp
        src/ui_button.cpp
        src/ui_utilities.cpp
        src/combustion_chamber_object.cpp
        src/console_overlay.cpp
        src/wankel_rotor_object.cpp
        src/oscilloscope.cpp
        src/shaders.cpp
        src/engine_view.cpp
        src/right_gauge_cluster.cpp
        src/cylinder_temperature_gauge.cpp
        src/labeled_gauge.cpp
        src/throttle_display.cpp
        src/afr_cluster.cpp
        src/fuel_cluster.cpp
        src/oscilloscope_cluster.cpp
        src/performance_cluster.cpp
        src/firing_order_display.cpp
        src/load_simulation_cluster.cpp
        src/mixer_cluster.cpp
        src/info_cluster.cpp

        # Include files
        include/delta.h
        include/dtv.h
        include/engine_sim_application.h
        include/geometry_generator.h
        include/simulation_object.h
        include/piston_object.h
        include/connecting_rod_object.h
        include/ui_element.h
        include/ui_manager.h
        include/cylinder_pressure_gauge.h
        include/ui_math.h
        include/crankshaft_object.h
        include/cylinder_bank_object.h
        include/cylinder_head_object.h
        include/ui_button.h
        include/ui_utilities.h
        include/combustion_chamber_object.h
        include/console_overlay.h
        include/wankel_rotor_object.h
        include/oscilloscope.h
        include/shaders.h
        include/engine_view.h
        include/right_gauge_cluster.h
        include/cylinder_temperature_gauge.h
        include/labeled_gauge.h
        include/throttle_display.h
        include/afr_cluster.h
        include/fuel_cluster.h
        include/oscilloscope_cluster.h
        include/performance_cluster.h
        include/firing_order_display.h
        include/load_simulation_cluster.h
        include/mixer_cluster.h
        include/info_cluster.h
    )

    target_link_libraries(engine-sim-app PRIVATE
        engine-sim
        delta-basic)

    if (WIN32)
        # SDL2main provides WinMain when building a GUI subsystem executable
        if (TARGET SDL2::SDL2main)
            target_link_libraries(engine-sim-app PRIVATE SDL2::SDL2main)
        elseif (TARGET SDL2main)
            target_link_libraries(engine-sim-app PRIVATE SDL2main)
        endif()
    endif()

    if (DTV)
        target_compile_definitions(engine-sim-app PRIVATE ATG_ENGINE_SIM_VIDEO_CAPTURE)
        target_link_libraries(engine-sim-app PRIVATE direct-to-video)
    endif()

    if (PIRANHA_ENABLED)
        target_link_libraries(engine-sim-app PRIVATE engine-sim-script-interpreter)
    endif()

    if (DISCORD_ENABLED)
        target_compile_definitions(engine-sim-app PRIVATE ATG_ENGINE_SIM_DISCORD_ENABLED)
        target_link_libraries(engine-sim-app PRIVATE discord)
    endif()

    # Build information
    configure_file(
        ${PROJECT_SOURCE_DIR}/include/build_info.h.in
        ${PROJECT_BINARY_DIR}/generated/build_info.h
        @ONLY)
    target_include_directories(engine-sim-app PRIVATE ${PROJECT_BINARY_DIR}/generated)

    if (APPLE)
        set_target_properties(engine-sim-app PROPERTIES
            MACOSX_BUNDLE TRUE
            OUTPUT_NAME "Engine Simulator"
            MACOSX_BUNDLE_INFO_PLIST ${PROJECT_SOURCE_DIR}/packaging/macos/Info.plist.in
            MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.engine-sim.community"
            MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
            MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION})
    endif()

    # Put the executable in <build>/bin so that the data directory layout
    # matches the packaged release (bin/, assets/, es/, delta/)
    set_target_properties(engine-sim-app PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${PROJECT_BINARY_DIR}/bin
        RUNTIME_OUTPUT_DIRECTORY_RELEASE ${PROJECT_BINARY_DIR}/bin
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${PROJECT_BINARY_DIR}/bin
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${PROJECT_BINARY_DIR}/bin)

    # ====================================================
    # Installation / packaging layout
    #
    # Windows and Linux:              macOS:
    #   <prefix>/bin/engine-sim-app     <prefix>/Engine Simulator.app/
    #   <prefix>/assets/...               Contents/MacOS/Engine Simulator
    #   <prefix>/es/...                   Contents/Resources/{assets,es,delta}
    #   <prefix>/delta/{fonts,shaders}

    if (APPLE)
        set(ENGINE_SIM_DATA_DESTINATION "Engine Simulator.app/Contents/Resources")
    else()
        set(ENGINE_SIM_DATA_DESTINATION ".")
    endif()

    install(TARGETS engine-sim-app
        RUNTIME DESTINATION bin
        BUNDLE DESTINATION .)
    install(DIRECTORY assets/ DESTINATION ${ENGINE_SIM_DATA_DESTINATION}/assets
        PATTERN "*.dia" EXCLUDE)
    install(DIRECTORY es/ DESTINATION ${ENGINE_SIM_DATA_DESTINATION}/es)
    install(DIRECTORY dependencies/delta-studio/engines/basic/fonts
        DESTINATION ${ENGINE_SIM_DATA_DESTINATION}/delta)
    install(DIRECTORY dependencies/delta-studio/engines/basic/shaders/glsl
        DESTINATION ${ENGINE_SIM_DATA_DESTINATION}/delta/shaders)
    install(FILES LICENSE README.md CHANGELOG.md DESTINATION .)
    install(FILES docs/scripting.md docs/engineering.md DESTINATION docs)

    # Command line dynamometer (see docs/engineering.md)
    if (TARGET engine-sim-dyno)
        if (APPLE)
            install(TARGETS engine-sim-dyno RUNTIME DESTINATION .)
        else()
            install(TARGETS engine-sim-dyno RUNTIME DESTINATION bin)
        endif()
    endif()

    if (WIN32)
        # Ship the SDL DLL (when linked dynamically) and the MSVC runtime
        if (NOT ENGINE_SIM_BUNDLED_SDL AND TARGET SDL2::SDL2 AND CMAKE_VERSION VERSION_GREATER_EQUAL 3.21)
            install(IMPORTED_RUNTIME_ARTIFACTS SDL2::SDL2 DESTINATION bin)
        endif()

        set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION bin)
        set(CMAKE_INSTALL_UCRT_LIBRARIES FALSE)
        include(InstallRequiredSystemLibraries)

        # Copy the SDL DLL next to the executable for development builds
        if (NOT ENGINE_SIM_BUNDLED_SDL AND CMAKE_VERSION VERSION_GREATER_EQUAL 3.21)
            add_custom_command(TARGET engine-sim-app POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_RUNTIME_DLLS:engine-sim-app> $<TARGET_FILE_DIR:engine-sim-app>
                COMMAND_EXPAND_LISTS)
        endif()
    endif()

    if (UNIX AND NOT APPLE)
        install(FILES packaging/linux/engine-sim.desktop DESTINATION .)
    endif()
endif()

# ========================================================
# Tests

if (ENGINE_SIM_BUILD_TESTS)
    # Prefer a system installation of GoogleTest, otherwise download it
    find_package(GTest QUIET)
    if (NOT GTest_FOUND)
        include(FetchContent)
        FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
        set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
        add_library(GTest::gtest_main ALIAS gtest_main)
    endif()

    enable_testing()

    add_executable(engine-sim-test
        test/gas_system_tests.cpp
        test/engine_model_tests.cpp
        test/function_test.cpp
        test/synthesizer_tests.cpp
        test/thermodynamics_tests.cpp
        test/wankel_tests.cpp
    )

    target_link_libraries(engine-sim-test PRIVATE
        GTest::gtest_main
        engine-sim)

    include(GoogleTest)
    gtest_discover_tests(engine-sim-test DISCOVERY_TIMEOUT 60)
endif()
