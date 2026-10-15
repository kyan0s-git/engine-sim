#ifndef ATG_ENGINE_SIM_ENGINE_SIM_APPLICATION_H
#define ATG_ENGINE_SIM_ENGINE_SIM_APPLICATION_H

#include "geometry_generator.h"
#include "simulator.h"
#include "engine.h"
#include "simulation_object.h"
#include "ui_manager.h"
#include "dynamometer.h"
#include "audio_buffer.h"
#include "convolution_filter.h"
#include "shaders.h"
#include "engine_view.h"
#include "synthesizer.h"
#include "application_settings.h"
#include "transmission.h"
#include "updater.h"
#include "ui/app_ui.h"

#include "delta.h"
#include "dtv.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class EngineSimApplication {
    private:
        static std::string s_buildVersion;

    public:
        // Options that can be provided on the command line
        struct StartupOptions {
            enum class Toggle {
                Default,
                On,
                Off
            };

            std::string engineScript;
            std::string themeScript;
            std::string dataRoot;
            Toggle fullscreen = Toggle::Default;
            bool checkForUpdates = true;
        };

    public:
        EngineSimApplication();
        virtual ~EngineSimApplication();

        static std::string getBuildVersion() { return s_buildVersion; }

        void setStartupOptions(const StartupOptions &options) { m_startupOptions = options; }
        const std::string &getStartupError() const { return m_startupError; }

        // Returns false if the application could not be started; see
        // getStartupError() for details
        bool initialize(void *instance, ysContextObject::DeviceAPI api);
        void run();
        void destroy();

        // Script management
        void loadEngineScript(const std::string &path);
        void loadThemeScript(const std::string &path);
        void reloadScripts();

        // Bundled and user supplied engine and theme scripts
        std::vector<std::string> findEngineScripts() const;
        std::vector<std::string> findThemeScripts() const;
        void cycleEngineScript(int direction);
        void cycleThemeScript(int direction);

        const std::string &getEngineScriptPath() const { return m_engineScriptPath; }
        const std::string &getThemeScriptPath() const { return m_themeScriptPath; }
        const std::string &getScriptLog() const { return m_scriptLog; }
        const Updater &getUpdater() const { return m_updater; }

        // Checks for, downloads or installs an update depending on the
        // updater's state
        void requestUpdateAction();
        bool hasScriptErrors() const { return m_scriptErrors; }

        const std::string &getDataRoot() const { return m_dataRoot; }
        const std::string &getUserDataPath() const { return m_userDataPath; }

        void loadEngine(Engine *engine, Vehicle *vehicle, Transmission *transmission);
        void drawGenerated(
                const GeometryGenerator::GeometryIndices &indices,
                int layer = 0);
        void drawGeneratedUi(
                const GeometryGenerator::GeometryIndices &indices,
                int layer = 0);
        void drawGenerated(
                const GeometryGenerator::GeometryIndices &indices,
                int layer,
                dbasic::StageEnableFlags flags);
        void configure(const ApplicationSettings &settings);
        GeometryGenerator *getGeometryGenerator() { return &m_geometryGenerator; }

        Shaders *getShaders() { return &m_shaders; }
        dbasic::TextRenderer *getTextRenderer() { return &m_textRenderer; }

        void createObjects(Engine *engine);
        void destroyObjects();
        dbasic::DeltaEngine *getEngine() { return &m_engine; }

        float pixelsToUnits(float pixels) const;
        float unitsToPixels(float units) const;

        // Engine view materials: the realistic style uses metal colors and
        // shading, the classic style the theme's flat colors
        enum class Material {
            Block, Liner, Head, Piston, Rod, Crank, CrankSnout, Pin, IntakeValve, ExhaustValve, Cam, Rotor, Housing
        };
        bool isRealisticView() const;
        ysVector materialColor(Material material, const ysVector &classic) const;
        // Models are drawn in normalized units, generated shapes in metres
        // (sets the scale of the surface grain)
        void useMaterial(Material material, Shaders::Surface surface, float halfWidth = 1.0f, bool generated = false);

        ysVector getBackgroundColor() const { return m_background; }
        ysVector getForegroundColor() const { return m_foreground; }
        ysVector getHightlight1Color() const { return m_highlight1; }
        ysVector getPink() const { return m_pink; }
        ysVector getGreen() const { return m_green; }
        ysVector getYellow() const { return m_yellow; }
        ysVector getRed() const { return m_red; }
        ysVector getOrange() const { return m_orange; }
        ysVector getBlue() const { return m_blue; }

        const SimulationObject::ViewParameters &getViewParameters() const;
        void setViewLayer(int view) { m_viewParameters.Layer0 = view; }

        dbasic::AssetManager *getAssetManager() { return &m_assetManager; }

        int getScreenWidth() const { return m_screenWidth; }
        int getScreenHeight() const { return m_screenHeight; }

        Simulator *getSimulator() { return m_simulator; }
        ApplicationSettings* getAppSettings() { return &m_applicationSettings; }

        // User interface support
        AppUi &getUi() { return m_ui; }
        void logMessage(const std::string &message) { m_ui.log(message); }
        bool isPaused() const { return m_paused; }
        void setPaused(bool paused);
        double getDynoSpeed() const { return m_dynoSpeed; }
        void setDynoSpeed(double speed) { m_dynoSpeed = speed; }
        bool isFullscreen() const;
        void setFullscreen(bool fullscreen);

        // Applies the user preferences (settings page) that the application
        // handles
        void applyPreferences();

    protected:
        void loadScript();
        bool resolveDataPaths();
        std::vector<std::string> getScriptSearchPaths() const;
        bool compileScript(
            const std::string &path,
            Engine **engine,
            Vehicle **vehicle,
            Transmission **transmission,
            ApplicationSettings *settings,
            bool *settingsSet);
        void loadSession();
        void saveSession() const;
        void processEngineInput();
        void renderScene();

        void refreshUserInterface();

    protected:
        double m_speedSetting = 1.0;
        double m_targetSpeedSetting = 1.0;

        double m_clutchPressure = 1.0;
        double m_targetClutchPressure = 1.0;
        int m_lastMouseWheel = 0;

    protected:
        virtual void initialize();
        // Simulates the time since the previous call and moves the audio to
        // the output device (runs on the simulation thread)
        virtual void process(double elapsed);
        void simulationThread();
        void startSimulationThread();
        void stopSimulationThread();
        virtual void render();

        float m_displayAngle;
        float m_displayHeight;
        int m_gameWindowHeight;
        int m_screenWidth;
        int m_screenHeight;
        
        ApplicationSettings m_applicationSettings;
        dbasic::ShaderSet m_shaderSet;
        Shaders m_shaders;

        dbasic::DeltaEngine m_engine;
        dbasic::AssetManager m_assetManager;

        std::string m_assetPath;

        // Data locations
        std::string m_dataRoot;
        std::string m_deltaPath;
        std::string m_userDataPath;

        // Scripts
        StartupOptions m_startupOptions;
        std::string m_startupError;

        Updater m_updater;
        bool m_checkForUpdates;

        std::string m_engineScriptPath;
        std::string m_themeScriptPath;
        std::string m_scriptLog;
        bool m_scriptErrors = false;

        // Scratch buffer reused every frame for audio output
        std::vector<int16_t> m_audioScratch;

        ysRenderTarget *m_mainRenderTarget;
        ysGPUBuffer *m_geometryVertexBuffer;
        ysGPUBuffer *m_geometryIndexBuffer;

        GeometryGenerator m_geometryGenerator;
        dbasic::TextRenderer m_textRenderer;

        std::vector<SimulationObject *> m_objects;
        Engine *m_iceEngine;
        Vehicle *m_vehicle;
        Transmission *m_transmission;
        Simulator *m_simulator;
        double m_dynoSpeed;
        double m_torque;

        UiManager m_uiManager;
        EngineView *m_engineView;
        AppUi m_ui;
        bool m_restartRequested = false;
        double m_frameTimePeak = 1 / 60.0;

        // The simulation runs on its own thread in short slices so that the
        // frame rate doesn't depend on the cost of the physics. The main
        // thread holds the lock while it reads or changes the engine (user
        // interface, input, engine view geometry, loading scripts).
        std::mutex m_simulationLock;
        std::thread m_simulationThread;
        std::atomic<bool> m_simulationRunning{ false };
        std::atomic<bool> m_mainThreadWaiting{ false };
        bool m_stepRequested = false;
        double m_timePerStepEstimate = 0;   // [s]
        double m_requestedSimulationSpeed = 1.0;
        float m_appliedVolume = -1.0f;
        SimulationObject::ViewParameters m_viewParameters;

        bool m_paused;

    protected:
        void startRecording();
        void updateScreenSizeStability();
        bool readyToRecord();
        void stopRecording();
        void recordFrame();
        bool isRecording() const { return m_recording; }

        static constexpr int ScreenResolutionHistoryLength = 5;
        int m_screenResolution[ScreenResolutionHistoryLength][2];
        int m_screenResolutionIndex;
        bool m_recording;

        ysVector m_background;
        ysVector m_foreground;
        ysVector m_shadow;
        ysVector m_highlight1;
        ysVector m_highlight2;

        ysVector m_pink;
        ysVector m_orange;
        ysVector m_yellow;
        ysVector m_red;
        ysVector m_green;
        ysVector m_blue;

        ysAudioBuffer *m_outputAudioBuffer;
        AudioBuffer m_audioBuffer;
        ysAudioSource *m_audioSource;

        int m_oscillatorSampleOffset;

#ifdef ATG_ENGINE_SIM_VIDEO_CAPTURE
        atg_dtv::Encoder m_encoder;
#endif /* ATG_ENGINE_SIM_VIDEO_CAPTURE */
};

#endif /* ATG_ENGINE_SIM_ENGINE_SIM_APPLICATION_H */
