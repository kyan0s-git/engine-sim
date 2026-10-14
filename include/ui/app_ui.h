#ifndef ATG_ENGINE_SIM_APP_UI_H
#define ATG_ENGINE_SIM_APP_UI_H

#include "../engine_design.h"
#include "../engine_metrics.h"
#include "../user_preferences.h"

#include <SDL.h>

#include <deque>
#include <string>
#include <vector>

class EngineSimApplication;
class Engine;
class Simulator;
struct ImFont;

// The application's user interface: menus, panels, instruments, plots and
// windows, drawn with Dear ImGui and ImPlot on top of the engine view.
class AppUi {
    public:
        // Rectangle in window pixels (top-left origin)
        struct Rect {
            float x = 0, y = 0, width = 0, height = 0;
        };

        // Inputs from the user interface that the application applies every
        // frame together with the keyboard shortcuts
        struct Controls {
            double throttle = 0.0;          // persistent throttle setting
            bool starterHeld = false;
            bool brakeHeld = false;
            double clutch = 1.0;            // engagement
            double simulationSpeed = 1.0;   // slow motion factor
            bool reloadRequested = false;
            bool quitRequested = false;
            bool stepRequested = false;
            int cycleEngine = 0;            // previous (-1) or next (1) engine
            std::string loadScript;         // engine script to load
            std::string loadTheme;          // theme script to load ("-" = none)
        };

        // Panel arrangement
        enum class Layout {
            Full,               // controls, instruments and plots
            EngineOnly,
            Instruments         // engine view and instruments
        };

    public:
        AppUi();
        ~AppUi();

        bool initialize(EngineSimApplication *app, SDL_Window *window);
        void destroy();

        // Input; returns true if the user interface wants the event
        bool processEvent(const SDL_Event &event);

        // Builds the frame's user interface; call once per frame after the
        // window's events have been processed
        void build();

        // Draws the user interface; called right before the frame is presented
        void render();

        // Area left for the engine view
        Rect getEngineViewRect() const { return m_engineViewRect; }

        Controls &controls() { return m_controls; }
        UserPreferences &preferences() { return m_preferences; }
        void savePreferences();

        void log(const std::string &message);
        void onEngineLoaded(Engine *engine, Simulator *simulator);

        // Data collection
        void sampleSimulationStep();
        void addAudioSample(float sample);
        void addFrameTiming(double timePerStep, double audioLatency, double inputBufferUsage);

        bool wantsKeyboard() const;
        void toggleConsole() { m_showConsole = !m_showConsole; }
        void cycleLayout();
        void openSettings() { m_showSettings = true; }

    protected:
        void applyStyle();
        void rebuildFonts();

        void buildMenuBar();
        void buildStatusBar();
        void buildControlPanel();
        void buildInstrumentPanel();
        void buildPlotPanel();
        void buildConsoleWindow();
        void buildSettingsWindow();
        void buildDesignerWindow();
        void buildTuningWindow();
        void buildHelpWindow();
        void buildAboutWindow();
        void buildEngineViewOverlay();

        void buildEngineMenu();
        void buildCylinderSelector();
        void buildCylinderPlot();
        void buildPvPlot();
        void buildFlowPlot();
        void buildDynoPlot();
        void buildTrendPlot();
        void buildAudioPlot();
        void buildPerformanceTable();

        void collectFrameData();
        void resetAngleData();
        void resetDynoData();
        void refreshScriptLists();
        bool openDesignFromScript(const std::string &path);
        void generateDesign(bool load);

        // Unit conversions following the preferences or the theme
        std::string powerUnits() const;
        std::string torqueUnits() const;
        std::string speedUnits() const;
        std::string pressureUnits() const;
        double toPower(double watts) const;
        double toTorque(double newtonMeters) const;
        double toSpeed(double metersPerSecond) const;
        double toPressure(double pascal) const;     // absolute
        double toTemperature(double kelvin) const;
        const char *temperatureUnits() const;

    protected:
        EngineSimApplication *m_app;
        SDL_Window *m_window;
        bool m_initialized;
        std::string m_iniPath;
        std::string m_preferencesPath;
        bool m_preferencesDirty;

        ImFont *m_font;
        ImFont *m_monoFont;
        float m_appliedScale;
        float m_appliedDpi;
        std::string m_appliedTheme;

        UserPreferences m_preferences;
        Controls m_controls;
        Rect m_engineViewRect;
        Layout m_layout;

        // Panel sizes (unscaled points)
        float m_leftWidth;
        float m_rightWidth;
        float m_bottomHeight;

        // Windows
        bool m_showConsole;
        bool m_showSettings;
        bool m_showDesigner;
        bool m_showTuning;
        bool m_showHelp;
        bool m_showAbout;

        // Log
        struct LogEntry {
            double time;
            std::string text;
        };
        std::deque<LogEntry> m_log;
        std::string m_lastMessage;
        double m_lastMessageTime;
        bool m_scrollLogToBottom;

        // Scripts available in the menus (refreshed when a menu opens)
        std::vector<std::string> m_engineScripts;
        std::vector<std::string> m_themeScripts;
        bool m_scriptListsValid;

        // Engine being displayed
        Engine *m_engine;
        Simulator *m_simulator;
        int m_plotCylinder;
        int m_sampledCylinder;

        // Per crank-angle data of the selected cylinder (0.5 degree bins
        // over a four-stroke cycle; the bins are spread over the cycle of
        // other engines)
        static constexpr int AngleBins = 1440;
        std::vector<float> m_angle;
        std::vector<float> m_cylinderPressure;
        std::vector<float> m_intakeLift;
        std::vector<float> m_exhaustLift;
        std::vector<float> m_cylinderVolume;
        std::vector<float> m_cylinderTemperature;
        std::vector<float> m_intakeFlow;            // [scfm]
        std::vector<float> m_exhaustFlow;
        std::vector<float> m_totalExhaustFlow;
        int m_lastAngleBin;
        double m_angleCycleLength;

        // Time series (per frame)
        static constexpr int FrameHistory = 600;
        std::vector<float> m_frameTime;
        std::vector<float> m_rpmHistory;
        std::vector<float> m_torqueHistory;
        std::vector<float> m_mapHistory;
        int m_frameHistoryIndex;
        double m_clock;

        // Audio output
        static constexpr int AudioHistory = 4096;
        std::vector<float> m_audio;
        int m_audioIndex;
        int m_audioDecimation;

        // Dyno curve: brake torque and power by speed
        static constexpr int DynoBins = 200;     // 100 rpm each
        std::vector<float> m_dynoTorque;
        std::vector<float> m_dynoPower;
        std::vector<char> m_dynoValid;

        // Performance
        double m_timePerStep;
        double m_audioLatency;
        double m_inputBufferUsage;

        EngineMetrics m_metrics;

        // Values of the tunable parameters when the engine was loaded
        struct TuningDefaults {
            double timingOffset = 0;
            double revLimit = 0;
            double lambda = 1;
            double frictionMultiplier = 1;
            double wallTemperature = 0;
            double heatTransferMultiplier = 1;
            double combustionEfficiency = 1;
            int simulationFrequency = 10000;
            float volume = 1, convolution = 1, highFrequencyGain = 0.01f;
            float airNoise = 1, inputNoise = 0.5f;
        };
        TuningDefaults m_tuningDefaults;

        // Engine designer
        EngineDesign m_design;
        std::string m_designStatus;
        bool m_designStatusError;
        std::string m_designPath;
};

#endif /* ATG_ENGINE_SIM_APP_UI_H */
