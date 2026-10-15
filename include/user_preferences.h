#ifndef ATG_ENGINE_SIM_USER_PREFERENCES_H
#define ATG_ENGINE_SIM_USER_PREFERENCES_H

#include <string>

// Preferences edited on the settings page and stored in settings.ini in the
// user data folder. Unit names left empty follow the theme script.
struct UserPreferences {
    // Display
    bool fullscreen = false;
    bool vsync = true;
    float uiScale = 1.0f;
    std::string uiTheme = "dark";           // dark, light, classic
    bool showEngineViewFrame = false;

    // Engine view: "realistic" (shaded metal parts) or "classic" (flat theme
    // colors)
    std::string engineViewStyle = "realistic";

    // Units ("" = use the theme's units)
    std::string powerUnits;                 // hp, kW
    std::string torqueUnits;                // lb-ft, Nm
    std::string speedUnits;                 // mph, kph
    std::string pressureUnits;              // inHg, kPa, mbar, bar, psi
    std::string temperatureUnits = "C";     // C, F, K

    // Audio
    float masterVolume = 1.0f;
    bool muteWhenInactive = false;

    // Simulation (0 = the engine's own setting)
    int simulationFrequency = 0;
    int fluidSimulationSteps = 0;
    float targetAudioLatency = 0.1f;

    // Updates
    bool checkForUpdates = true;

    bool load(const std::string &path);
    bool save(const std::string &path) const;
};

#endif /* ATG_ENGINE_SIM_USER_PREFERENCES_H */
