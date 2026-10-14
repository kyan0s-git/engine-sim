#include "../include/user_preferences.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>

namespace {

    bool parseBool(const std::string &value) {
        return value == "1" || value == "true" || value == "yes" || value == "on";
    }

} /* namespace */

bool UserPreferences::load(const std::string &path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;

        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        const std::string key = line.substr(0, eq);
        const std::string value = line.substr(eq + 1);

        if (key == "fullscreen") fullscreen = parseBool(value);
        else if (key == "vsync") vsync = parseBool(value);
        else if (key == "ui_scale") uiScale = std::clamp((float)std::atof(value.c_str()), 0.5f, 3.0f);
        else if (key == "ui_theme") uiTheme = value;
        else if (key == "show_engine_view_frame") showEngineViewFrame = parseBool(value);
        else if (key == "power_units") powerUnits = value;
        else if (key == "torque_units") torqueUnits = value;
        else if (key == "speed_units") speedUnits = value;
        else if (key == "pressure_units") pressureUnits = value;
        else if (key == "temperature_units") temperatureUnits = value;
        else if (key == "master_volume") masterVolume = std::clamp((float)std::atof(value.c_str()), 0.0f, 1.0f);
        else if (key == "mute_when_inactive") muteWhenInactive = parseBool(value);
        else if (key == "simulation_frequency") simulationFrequency = std::max(0, std::atoi(value.c_str()));
        else if (key == "fluid_simulation_steps") fluidSimulationSteps = std::max(0, std::atoi(value.c_str()));
        else if (key == "target_audio_latency") targetAudioLatency = std::clamp((float)std::atof(value.c_str()), 0.02f, 0.5f);
        else if (key == "check_for_updates") checkForUpdates = parseBool(value);
    }

    return true;
}

bool UserPreferences::save(const std::string &path) const {
    std::ofstream file(path, std::ios::out | std::ios::trunc);
    if (!file.is_open()) return false;

    file << "# Engine Simulator settings\n"
        << "fullscreen=" << (fullscreen ? 1 : 0) << "\n"
        << "vsync=" << (vsync ? 1 : 0) << "\n"
        << "ui_scale=" << uiScale << "\n"
        << "ui_theme=" << uiTheme << "\n"
        << "show_engine_view_frame=" << (showEngineViewFrame ? 1 : 0) << "\n"
        << "power_units=" << powerUnits << "\n"
        << "torque_units=" << torqueUnits << "\n"
        << "speed_units=" << speedUnits << "\n"
        << "pressure_units=" << pressureUnits << "\n"
        << "temperature_units=" << temperatureUnits << "\n"
        << "master_volume=" << masterVolume << "\n"
        << "mute_when_inactive=" << (muteWhenInactive ? 1 : 0) << "\n"
        << "simulation_frequency=" << simulationFrequency << "\n"
        << "fluid_simulation_steps=" << fluidSimulationSteps << "\n"
        << "target_audio_latency=" << targetAudioLatency << "\n"
        << "check_for_updates=" << (checkForUpdates ? 1 : 0) << "\n";

    return true;
}
