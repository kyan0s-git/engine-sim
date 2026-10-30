#ifndef ATG_ENGINE_SIM_APP_UI_H
#define ATG_ENGINE_SIM_APP_UI_H

#include "../ecu.h"
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
        const UserPreferences &preferencesConst() const { return m_preferences; }
        void savePreferences();

        void log(const std::string &message);
        void onEngineLoaded(Engine *engine, Simulator *simulator);

        // Data collection
        void sampleSimulationStep();
        void updateLaunchTest(double dt);    // simulation thread, every step
        void addAudioSample(float sample);
        void addFrameTiming(double timePerStep, double audioLatency, double inputBufferUsage);

        // Time spent in each part of a frame [s]
        struct FrameProfile {
            double ui = 0, simulation = 0, scene = 0, present = 0, total = 0;
        };
        void addFrameProfile(const FrameProfile &profile);

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
        void buildLibraryWindow();
        void buildTuningWindow();
        void buildHelpWindow();
        void buildAboutWindow();
        void buildEngineViewOverlay();

        void buildEngineMenu();

        // Beginner / advanced interface, toolbar, ECU and launch test
        // (app_ui_tools.cpp)
        bool isBeginner() const { return m_preferences.uiMode != "advanced"; }
        void buildToolbar(float y, float height);
        void buildBeginnerControls();
        void buildEcuWindow();
        void buildMapEditor(EcuTable &table, EcuTable *stock, float height, double cursorX, double cursorY,
            double minValue, double maxValue, double step, const char *format);
        void buildLaunchTestWindow();
        void buildWelcomeWindow();
        void drawDesignDiagrams(const EngineDesign &design, float width);
        void autoStart(double dt);
        std::string tunePath() const;
        void saveTune();
        bool loadTune(bool quiet);
        void buildCylinderSelector();
        void buildCylinderPlot();
        void buildPvPlot();
        void buildFlowPlot();
        void buildDynoPlot();
        void buildTrendPlot();
        void buildAudioPlot();
        void buildPerformanceTable();

        void collectFrameData();
        void averageMetrics(double dt);
        void resetAngleData();
        void resetDynoData();
        void refreshScriptLists();
        bool openDesignFromScript(const std::string &path);
        void generateDesign(bool load);
        void loadDesign(const EngineDesign &design);
        void refreshSavedDesigns();

        // Unit conversions following the preferences or the theme
        std::string powerUnits() const;
        std::string torqueUnits() const;
        std::string speedUnits() const;
        std::string pressureUnits() const;
        double toPower(double watts) const;
        double toTorque(double newtonMeters) const;
        double toSpeed(double metersPerSecond) const;
        double toPressure(double pressure) const;     // absolute
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
        bool m_showLibrary;
        bool m_showEcu = false;
        bool m_showLaunchTest = false;
        bool m_showWelcome = false;

        // ECU window
        int m_ecuTab = 0;
        std::string m_ecuStatus;
        int m_mapSelectRow = -1, m_mapSelectCol = -1;

        // One-click start: ignition on, crank until the engine runs
        bool m_autoStarting = false;
        double m_autoStartTimer = 0;

        // Launch test (timed in simulation time on the simulation thread)
        struct LaunchResult {
            std::string engine;
            std::string setup;
            double t60 = -1, t100 = -1, tQuarter = -1, trap = 0, t200 = -1;
        };
        enum class LaunchState { Idle, Staging, Running, Done };
        LaunchState m_launchState = LaunchState::Idle;
        double m_launchTime = 0;
        double m_launchStartDistance = 0;
        double m_launchStageTimer = 0;
        bool m_launchAutomatic = true;
        bool m_launchWasAutomatic = false;
        LaunchResult m_launchCurrent;
        std::vector<LaunchResult> m_launchResults;
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
        FrameProfile m_profile;         // averaged
        FrameProfile m_profilePeak;     // worst frame in the last second
        FrameProfile m_profilePeakRunning;
        double m_profilePeakTime = 0;
        double m_audioLatency;
        double m_inputBufferUsage;

        EngineMetrics m_metrics;            // last complete cycle of each cylinder
        EngineMetrics m_shownMetrics;       // averaged over about half a second
        double m_firingFraction;            // share of recent cycles that burned

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
            double boostTarget = 0, driveRatio = 1, intercoolerEffectiveness = 0;
            bool blowoffValve = true;
        };
        TuningDefaults m_tuningDefaults;

        // Engine designer
        EngineDesign m_design;
        std::string m_designStatus;
        bool m_designStatusError;
        std::string m_designPath;
        char m_saveAsName[128];

        // Engine library
        struct SavedDesign {
            std::string path;
            EngineDesign design;
            bool designed = false;      // has a design header
        };
        std::vector<SavedDesign> m_savedDesigns;
        bool m_savedDesignsValid;
        std::string m_pendingDelete;
};

#endif /* ATG_ENGINE_SIM_APP_UI_H */
