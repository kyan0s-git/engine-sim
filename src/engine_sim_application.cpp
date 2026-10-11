#include "../include/engine_sim_application.h"

#include "../include/piston_object.h"
#include "../include/connecting_rod_object.h"
#include "../include/constants.h"
#include "../include/units.h"
#include "../include/crankshaft_object.h"
#include "../include/cylinder_bank_object.h"
#include "../include/cylinder_head_object.h"
#include "../include/ui_button.h"
#include "../include/combustion_chamber_object.h"
#include "../include/csv_io.h"
#include "../include/exhaust_system.h"
#include "../include/feedback_comb_filter.h"
#include "../include/utilities.h"
#include "../include/piston_engine_simulator.h"
#include "../include/wankel_engine.h"
#include "../include/wankel_rotor_object.h"

#ifdef ATG_ENGINE_SIM_PIRANHA_ENABLED
#include "../scripting/include/compiler.h"
#endif /* ATG_ENGINE_SIM_PIRANHA_ENABLED */

#include "build_info.h"

#include <delta-studio/include/yds_error_handler.h>
#include <delta-studio/include/yds_error_system.h>

#include <SDL.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#ifdef ATG_ENGINE_SIM_DISCORD_ENABLED
#include <Discord.h>
#endif

std::string EngineSimApplication::s_buildVersion = ENGINE_SIM_VERSION;

namespace {

    namespace fs = std::filesystem;

    bool isDataRoot(const fs::path &path) {
        std::error_code ec;
        return fs::is_directory(path / "assets", ec) && fs::is_directory(path / "es", ec);
    }

    std::string normalizePath(const fs::path &path) {
        std::error_code ec;
        const fs::path canonical = fs::weakly_canonical(path, ec);
        return (ec ? path : canonical).lexically_normal().string();
    }

    bool fileExists(const std::string &path) {
        std::error_code ec;
        return !path.empty() && fs::is_regular_file(fs::path(path), ec);
    }

    // Reports errors raised inside of the rendering/platform layer
    class LoggingErrorHandler : public ysErrorHandler {
    public:
        static std::string s_lastError;

        virtual void OnErrorMessage(
            ysError error,
            unsigned int line,
            ysObject *object,
            const char *file,
            const char *msg) override
        {
            (void)object;

            std::stringstream ss;
            ss << "error " << static_cast<int>(error)
                << " (" << fs::path(file).filename().string() << ":" << line << ")";
            if (msg != nullptr && msg[0] != '\0') {
                ss << ": " << msg;
            }

            s_lastError = ss.str();
            std::fprintf(stderr, "[engine-sim] %s\n", s_lastError.c_str());
        }
    };

    std::string LoggingErrorHandler::s_lastError;

} /* namespace */

EngineSimApplication::EngineSimApplication() {
    m_assetPath = "";

    m_geometryVertexBuffer = nullptr;
    m_geometryIndexBuffer = nullptr;

    m_paused = false;
    m_recording = false;
    m_screenResolutionIndex = 0;
    for (int i = 0; i < ScreenResolutionHistoryLength; ++i) {
        m_screenResolution[i][0] = m_screenResolution[i][1] = 0;
    }

    m_background = ysColor::srgbiToLinear(0x0E1012);
    m_foreground = ysColor::srgbiToLinear(0xFFFFFF);
    m_shadow = ysColor::srgbiToLinear(0x0E1012);
    m_highlight1 = ysColor::srgbiToLinear(0xEF4545);
    m_highlight2 = ysColor::srgbiToLinear(0xFFFFFF);
    m_pink = ysColor::srgbiToLinear(0xF394BE);
    m_red = ysColor::srgbiToLinear(0xEE4445);
    m_orange = ysColor::srgbiToLinear(0xF4802A);
    m_yellow = ysColor::srgbiToLinear(0xFDBD2E);
    m_blue = ysColor::srgbiToLinear(0x77CEE0);
    m_green = ysColor::srgbiToLinear(0xBDD869);

    m_displayHeight = (float)units::distance(2.0, units::foot);
    m_outputAudioBuffer = nullptr;
    m_audioSource = nullptr;

    m_torque = 0;
    m_dynoSpeed = 0;

    m_simulator = nullptr;
    m_engineView = nullptr;
    m_rightGaugeCluster = nullptr;
    m_temperatureGauge = nullptr;
    m_oscCluster = nullptr;
    m_performanceCluster = nullptr;
    m_loadSimulationCluster = nullptr;
    m_mixerCluster = nullptr;
    m_infoCluster = nullptr;
    m_console = nullptr;
    m_showConsole = false;
    m_iceEngine = nullptr;
    m_mainRenderTarget = nullptr;

    m_vehicle = nullptr;
    m_transmission = nullptr;

    m_oscillatorSampleOffset = 0;
    m_gameWindowHeight = 256;
    m_screenWidth = 256;
    m_screenHeight = 256;
    m_screen = 0;
    m_viewParameters.Layer0 = 0;
    m_viewParameters.Layer1 = 0;

    m_displayAngle = 0.0f;
}

EngineSimApplication::~EngineSimApplication() {
    /* void */
}

bool EngineSimApplication::initialize(void *instance, ysContextObject::DeviceAPI api) {
    static LoggingErrorHandler *errorHandler = nullptr;
    if (errorHandler == nullptr) {
        ysErrorSystem::GetInstance()->AttachErrorHandler(&errorHandler);
    }

    if (!resolveDataPaths()) {
        return false;
    }

    m_engine.GetConsole()->SetDefaultFontDirectory(m_deltaPath + "/fonts/");

    const std::string shaderPath = m_deltaPath + "/shaders/";
    const std::string winTitle = "Engine Simulator | v" + s_buildVersion;
    dbasic::DeltaEngine::GameEngineSettings settings;
    settings.API = api;
    settings.DepthBuffer = false;
    settings.Instance = instance;
    settings.ShaderDirectory = shaderPath.c_str();
    settings.WindowTitle = winTitle.c_str();
    settings.WindowPositionX = 0;
    settings.WindowPositionY = 0;
    settings.WindowStyle = ysWindow::WindowStyle::Windowed;

    // Size the window to comfortably fit on the primary display
    SDL_InitSubSystem(SDL_INIT_VIDEO);
    SDL_Rect usableBounds;
    if (SDL_GetDisplayUsableBounds(0, &usableBounds) == 0 && usableBounds.w > 0 && usableBounds.h > 0) {
        settings.WindowWidth = std::min(1920, static_cast<int>(usableBounds.w * 0.9));
        settings.WindowHeight = std::min(1080, static_cast<int>(usableBounds.h * 0.9));
    }
    else {
        settings.WindowWidth = 1600;
        settings.WindowHeight = 900;
    }

    const ysError windowError = m_engine.CreateGameWindow(settings);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    if (windowError != ysError::None || m_engine.GetGameWindow() == nullptr) {
        m_startupError =
            "Could not create the application window or OpenGL 4.1+ context.\n\n"
            + LoggingErrorHandler::s_lastError + "\n" + std::string(SDL_GetError())
            + "\n\nMake sure your graphics drivers are up to date.";
        return false;
    }

    m_engine.GetDevice()->CreateSubRenderTarget(
        &m_mainRenderTarget,
        m_engine.GetScreenRenderTarget(),
        0,
        0,
        0,
        0);

    m_engine.InitializeShaderSet(&m_shaderSet);
    m_shaders.Initialize(
        &m_shaderSet,
        m_mainRenderTarget,
        m_engine.GetScreenRenderTarget(),
        m_engine.GetDefaultShaderProgram(),
        m_engine.GetDefaultInputLayout());
    m_engine.InitializeConsoleShaders(&m_shaderSet);
    m_engine.SetShaderSet(&m_shaderSet);

    m_shaders.SetClearColor(ysColor::srgbiToLinear(0x34, 0x98, 0xdb));

    m_assetManager.SetEngine(&m_engine);

    m_engine.GetDevice()->CreateIndexBuffer(
        &m_geometryIndexBuffer, sizeof(unsigned short) * 200000, nullptr);
    m_engine.GetDevice()->CreateVertexBuffer(
        &m_geometryVertexBuffer, sizeof(dbasic::Vertex) * 100000, nullptr);

    m_geometryGenerator.initialize(100000, 200000);

    initialize();

    return true;
}

bool EngineSimApplication::resolveDataPaths() {
    const fs::path modulePath = fs::path(dbasic::GetModulePath().ToString());

    // Candidate locations for the directory containing assets/ and es/, in
    // order of priority
    std::vector<fs::path> candidates;
    if (!m_startupOptions.dataRoot.empty()) {
        candidates.push_back(m_startupOptions.dataRoot);
    }

    const char *envDataRoot = std::getenv("ENGINE_SIM_DATA_ROOT");
    if (envDataRoot != nullptr && envDataRoot[0] != '\0') {
        candidates.push_back(envDataRoot);
    }

    // Legacy configuration file placed next to the executable. The second
    // line contains the asset directory relative to the executable.
    const fs::path confPath = modulePath / "delta.conf";
    if (fileExists(confPath.string())) {
        std::ifstream confFile(confPath);
        std::string legacyEnginePath, legacyAssetPath;
        std::getline(confFile, legacyEnginePath);
        std::getline(confFile, legacyAssetPath);
        if (!legacyAssetPath.empty()) {
            candidates.push_back((modulePath / legacyAssetPath).parent_path());
        }
    }

    candidates.push_back(modulePath.parent_path());                 // <root>/bin/app
    candidates.push_back(modulePath);                               // <root>/app
    candidates.push_back(modulePath.parent_path() / "Resources");   // macOS bundle
    candidates.push_back(fs::current_path().parent_path());
    candidates.push_back(fs::current_path());
    candidates.push_back(ENGINE_SIM_SOURCE_DIR);                     // Development build

    for (const fs::path &candidate : candidates) {
        if (isDataRoot(candidate)) {
            m_dataRoot = normalizePath(candidate);
            break;
        }
    }

    if (m_dataRoot.empty()) {
        m_startupError =
            "Could not find the Engine Simulator data files (the 'assets' and 'es' folders). "
            "Keep the executable inside the 'bin' folder of the extracted release, or pass "
            "--data <directory> on the command line.";
        return false;
    }

    // Fonts and shaders used by the rendering engine
    const fs::path deltaCandidates[] = {
        fs::path(m_dataRoot) / "delta",
        fs::path(m_dataRoot) / "dependencies" / "delta-studio" / "engines" / "basic",
        fs::path(ENGINE_SIM_SOURCE_DIR) / "dependencies" / "delta-studio" / "engines" / "basic"
    };

    for (const fs::path &candidate : deltaCandidates) {
        std::error_code ec;
        if (fs::is_directory(candidate / "shaders", ec) && fs::is_directory(candidate / "fonts", ec)) {
            m_deltaPath = normalizePath(candidate);
            break;
        }
    }

    if (m_deltaPath.empty()) {
        m_startupError = "Could not find the rendering engine's fonts and shaders under '" + m_dataRoot + "'.";
        return false;
    }

    m_assetPath = (fs::path(m_dataRoot) / "assets").string();

    std::printf("Engine Simulator %s\n  data: %s\n  rendering engine: %s\n",
        s_buildVersion.c_str(), m_dataRoot.c_str(), m_deltaPath.c_str());

    // Per-user writable location for logs and session information
    char *prefPath = SDL_GetPrefPath("EngineSimulator", "engine-sim");
    if (prefPath != nullptr) {
        m_userDataPath = prefPath;
        SDL_free(prefPath);
    }
    else {
        m_userDataPath = m_dataRoot;
    }

    std::printf("  user data: %s\n", m_userDataPath.c_str());
    std::fflush(stdout);

    return true;
}

std::vector<std::string> EngineSimApplication::getScriptSearchPaths() const {
    std::vector<std::string> paths;

    if (!m_engineScriptPath.empty()) {
        paths.push_back(fs::path(m_engineScriptPath).parent_path().string() + "/");
    }

    paths.push_back((fs::path(m_userDataPath) / "assets").string() + "/");
    paths.push_back((fs::path(m_dataRoot) / "assets").string() + "/");
    paths.push_back((fs::path(m_dataRoot) / "es").string() + "/");

    return paths;
}

void EngineSimApplication::loadSession() {
    const fs::path sessionPath = fs::path(m_userDataPath) / "session.ini";

    std::ifstream file(sessionPath);
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();

        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        const std::string key = line.substr(0, eq);
        const std::string value = line.substr(eq + 1);

        if (key == "engine" && fileExists(value)) m_engineScriptPath = value;
        else if (key == "theme" && fileExists(value)) m_themeScriptPath = value;
    }
}

void EngineSimApplication::saveSession() const {
    if (m_userDataPath.empty()) return;

    const fs::path sessionPath = fs::path(m_userDataPath) / "session.ini";
    std::ofstream file(sessionPath, std::ios::out | std::ios::trunc);
    if (!file.is_open()) return;

    file << "engine=" << m_engineScriptPath << "\n";
    file << "theme=" << m_themeScriptPath << "\n";
}

void EngineSimApplication::initialize() {
    m_shaders.SetClearColor(ysColor::srgbiToLinear(0x34, 0x98, 0xdb));
    // The compiled scene (.ysce) is shipped with the application; only rebuild
    // it from the interchange file (.dia) when it is missing. Recompiling on
    // every launch was slow and required write access to the data folder.
    const std::string sceneFile = m_assetPath + "/assets";
    if (!fileExists(sceneFile + ".ysce") && fileExists(sceneFile + ".dia")) {
        m_assetManager.CompileInterchangeFile(sceneFile.c_str(), 1.0f, true);
    }
    m_assetManager.LoadSceneFile(sceneFile.c_str(), true);

    m_textRenderer.SetEngine(&m_engine);
    m_textRenderer.SetRenderer(m_engine.GetUiRenderer());
    m_textRenderer.SetFont(m_engine.GetConsole()->GetFont());

    // Pick the scripts to start with: command line, then the previous
    // session, then the legacy main.mr entry point
    loadSession();

    if (!m_startupOptions.engineScript.empty()) {
        m_engineScriptPath = normalizePath(fs::absolute(m_startupOptions.engineScript));
    }

    if (!m_startupOptions.themeScript.empty()) {
        m_themeScriptPath = normalizePath(fs::absolute(m_startupOptions.themeScript));
    }

    if (m_engineScriptPath.empty()) {
        m_engineScriptPath = normalizePath(fs::path(m_assetPath) / "main.mr");
    }

    loadScript();

    if (m_startupOptions.fullscreen == StartupOptions::Toggle::On) {
        m_engine.GetGameWindow()->SetWindowStyle(ysWindow::WindowStyle::Fullscreen);
    }
    else if (m_startupOptions.fullscreen == StartupOptions::Toggle::Off) {
        m_engine.GetGameWindow()->SetWindowStyle(ysWindow::WindowStyle::Windowed);
    }

    m_audioBuffer.initialize(44100, 44100);
    m_audioBuffer.m_writePointer = (int)(44100 * 0.1);

    ysAudioParameters params;
    params.m_bitsPerSample = 16;
    params.m_channelCount = 1;
    params.m_sampleRate = 44100;
    m_outputAudioBuffer =
        m_engine.GetAudioDevice()->CreateBuffer(&params, 44100);

    m_audioSource = m_engine.GetAudioDevice()->CreateSource(m_outputAudioBuffer);
    m_audioSource->SetMode((m_simulator != nullptr && m_simulator->getEngine() != nullptr)
        ? ysAudioSource::Mode::Loop
        : ysAudioSource::Mode::Stop);
    m_audioSource->SetPan(0.0f);
    m_audioSource->SetVolume(1.0f);

#ifdef ATG_ENGINE_SIM_DISCORD_ENABLED
    // Create a global instance of discord-rpc
    CDiscord::CreateInstance();

    // Enable it, this needs to be set via a config file of some sort. 
    GetDiscordManager()->SetUseDiscord(true);
    DiscordRichPresence passMe = { 0 };

    std::string engineName = (m_iceEngine != nullptr)
        ? m_iceEngine->getName()
        : "Broken Engine";

    GetDiscordManager()->SetStatus(passMe, engineName, s_buildVersion);
#endif /* ATG_ENGINE_SIM_DISCORD_ENABLED */
}

void EngineSimApplication::process(float frame_dt) {
    frame_dt = static_cast<float>(clamp(frame_dt, 1 / 200.0f, 1 / 30.0f));

    double speed = 1.0 / 1.0;
    if (m_engine.IsKeyDown(ysKey::Code::N1)) {
        speed = 1 / 10.0;
    }
    else if (m_engine.IsKeyDown(ysKey::Code::N2)) {
        speed = 1 / 100.0;
    }
    else if (m_engine.IsKeyDown(ysKey::Code::N3)) {
        speed = 1 / 200.0;
    }
    else if (m_engine.IsKeyDown(ysKey::Code::N4)) {
        speed = 1 / 500.0;
    }
    else if (m_engine.IsKeyDown(ysKey::Code::N5)) {
        speed = 1 / 1000.0;
    }

    if (m_engine.IsKeyDown(ysKey::Code::F1)) {
        m_displayAngle += frame_dt * 1.0f;
    }
    else if (m_engine.IsKeyDown(ysKey::Code::F2)) {
        m_displayAngle -= frame_dt * 1.0f;
    }
    else if (m_engine.ProcessKeyDown(ysKey::Code::F3)) {
        m_displayAngle = 0.0f;
    }

    m_simulator->setSimulationSpeed(speed);

    const double avgFramerate = clamp(m_engine.GetAverageFramerate(), 30.0f, 1000.0f);
    m_simulator->startFrame(1 / avgFramerate);

    auto proc_t0 = std::chrono::steady_clock::now();
    const int iterationCount = m_simulator->getFrameIterationCount();
    while (m_simulator->simulateStep()) {
        m_oscCluster->sample();
    }

    auto proc_t1 = std::chrono::steady_clock::now();

    m_simulator->endFrame();

    auto duration = proc_t1 - proc_t0;
    if (iterationCount > 0) {
        m_performanceCluster->addTimePerTimestepSample(
            (duration.count() / 1E9) / iterationCount);
    }

    const SampleOffset safeWritePosition = m_audioSource->GetCurrentWritePosition();
    const SampleOffset writePosition = m_audioBuffer.m_writePointer;

    SampleOffset targetWritePosition =
        m_audioBuffer.getBufferIndex(safeWritePosition, (int)(44100 * 0.1));
    SampleOffset maxWrite = m_audioBuffer.offsetDelta(writePosition, targetWritePosition);

    SampleOffset currentLead = m_audioBuffer.offsetDelta(safeWritePosition, writePosition);
    SampleOffset newLead = m_audioBuffer.offsetDelta(safeWritePosition, targetWritePosition);

    if (currentLead > 44100 * 0.5) {
        m_audioBuffer.m_writePointer = m_audioBuffer.getBufferIndex(safeWritePosition, (int)(44100 * 0.05));
        currentLead = m_audioBuffer.offsetDelta(safeWritePosition, m_audioBuffer.m_writePointer);
        maxWrite = m_audioBuffer.offsetDelta(m_audioBuffer.m_writePointer, targetWritePosition);
    }

    if (currentLead > newLead) {
        maxWrite = 0;
    }

    if (m_audioScratch.size() < maxWrite) {
        m_audioScratch.resize(maxWrite);
    }

    int16_t *samples = m_audioScratch.data();
    const int readSamples = (maxWrite > 0)
        ? m_simulator->readAudioOutput(maxWrite, samples)
        : 0;

    for (SampleOffset i = 0; i < (SampleOffset)readSamples && i < maxWrite; ++i) {
        const int16_t sample = samples[i];
        if (m_oscillatorSampleOffset % 4 == 0) {
            m_oscCluster->getAudioWaveformOscilloscope()->addDataPoint(
                m_oscillatorSampleOffset,
                sample / (float)(INT16_MAX));
        }

        m_audioBuffer.writeSample(sample, m_audioBuffer.m_writePointer, (int)i);

        m_oscillatorSampleOffset = (m_oscillatorSampleOffset + 1) % (44100 / 10);
    }

    if (readSamples > 0) {
        SampleOffset size0, size1;
        void *data0, *data1;
        m_audioSource->LockBufferSegment(
            m_audioBuffer.m_writePointer, readSamples, &data0, &size0, &data1, &size1);

        m_audioBuffer.copyBuffer(
            reinterpret_cast<int16_t *>(data0), m_audioBuffer.m_writePointer, size0);
        m_audioBuffer.copyBuffer(
            reinterpret_cast<int16_t *>(data1),
            m_audioBuffer.getBufferIndex(m_audioBuffer.m_writePointer, size0),
            size1);

        m_audioSource->UnlockBufferSegments(data0, size0, data1, size1);
        m_audioBuffer.commitBlock(readSamples);
    }

    m_performanceCluster->addInputBufferUsageSample(
        (double)m_simulator->getSynthesizerInputLatency() / m_simulator->getSynthesizerInputLatencyTarget());
    m_performanceCluster->addAudioLatencySample(
        m_audioBuffer.offsetDelta(m_audioSource->GetCurrentWritePosition(), m_audioBuffer.m_writePointer) / (44100 * 0.1));
}

void EngineSimApplication::render() {
    if (m_showConsole) {
        m_uiManager.render();
        return;
    }

    for (SimulationObject *object : m_objects) {
        object->generateGeometry();
    }

    m_viewParameters.Sublayer = 0;
    for (SimulationObject *object : m_objects) {
        object->render(&getViewParameters());
    }

    m_viewParameters.Sublayer = 1;
    for (SimulationObject *object : m_objects) {
        object->render(&getViewParameters());
    }

    m_viewParameters.Sublayer = 2;
    for (SimulationObject *object : m_objects) {
        object->render(&getViewParameters());
    }

    m_uiManager.render();
}

float EngineSimApplication::pixelsToUnits(float pixels) const {
    const float f = m_displayHeight / m_engineView->m_bounds.height();
    return pixels * f;
}

float EngineSimApplication::unitsToPixels(float units) const {
    const float f = m_engineView->m_bounds.height() / m_displayHeight;
    return units * f;
}

void EngineSimApplication::run() {
    while (true) {
        m_engine.StartFrame();

        if (!m_engine.IsOpen()) break;
        if (m_engine.ProcessKeyDown(ysKey::Code::Escape)) {
            break;
        }

        if (m_engine.ProcessKeyDown(ysKey::Code::Return)) {
            reloadScripts();
        }

        if (m_engine.ProcessKeyDown(ysKey::Code::F5)) {
            m_showConsole = !m_showConsole;
        }

        if (m_engine.ProcessKeyDown(ysKey::Code::PageDown)) {
            cycleEngineScript(1);
        }
        else if (m_engine.ProcessKeyDown(ysKey::Code::PageUp)) {
            cycleEngineScript(-1);
        }

        if (m_engine.ProcessKeyDown(ysKey::Code::OEM_6)) {
            cycleThemeScript(1);
        }
        else if (m_engine.ProcessKeyDown(ysKey::Code::OEM_4)) {
            cycleThemeScript(-1);
        }

        // While the console is open the arrow keys scroll it instead of
        // changing gears
        if (m_showConsole && m_console != nullptr) {
            if (m_engine.ProcessKeyDown(ysKey::Code::Up)) m_console->scroll(-1);
            if (m_engine.ProcessKeyDown(ysKey::Code::Down)) m_console->scroll(1);
        }

        if (m_engine.ProcessKeyDown(ysKey::Code::Tab)) {
            m_screen++;
            if (m_screen > 2) m_screen = 0;
        }

        if (m_engine.ProcessKeyDown(ysKey::Code::F)) {
            if (m_engine.GetGameWindow()->GetWindowStyle() != ysWindow::WindowStyle::Fullscreen) {
                m_engine.GetGameWindow()->SetWindowStyle(ysWindow::WindowStyle::Fullscreen);
                m_infoCluster->setLogMessage("Entered fullscreen mode");
            }
            else {
                m_engine.GetGameWindow()->SetWindowStyle(ysWindow::WindowStyle::Windowed);
                m_infoCluster->setLogMessage("Exited fullscreen mode");
            }
        }

        m_gameWindowHeight = m_engine.GetGameWindow()->GetGameHeight();
        m_screenHeight = m_engine.GetGameWindow()->GetScreenHeight();
        m_screenWidth = m_engine.GetGameWindow()->GetScreenWidth();

        updateScreenSizeStability();

        processEngineInput();

        if (m_engine.ProcessKeyDown(ysKey::Code::Insert) &&
            m_engine.GetGameWindow()->IsActive()) {
            if (!isRecording() && readyToRecord()) {
                startRecording();
            }
            else if (isRecording()) {
                stopRecording();
            }
        }

        if (isRecording() && !readyToRecord()) {
            stopRecording();
        }

        if (!m_paused || m_engine.ProcessKeyDown(ysKey::Code::Right)) {
            process(m_engine.GetFrameLength());
        }

        m_uiManager.update(m_engine.GetFrameLength());

        renderScene();

        m_engine.EndFrame();

        if (isRecording()) {
            recordFrame();
        }
    }

    if (isRecording()) {
        stopRecording();
    }

    if (m_simulator != nullptr) {
        m_simulator->endAudioRenderingThread();
    }

    saveSession();
}

void EngineSimApplication::destroy() {
    m_shaderSet.Destroy();

    m_engine.GetDevice()->DestroyGPUBuffer(m_geometryVertexBuffer);
    m_engine.GetDevice()->DestroyGPUBuffer(m_geometryIndexBuffer);

    m_assetManager.Destroy();
    m_engine.Destroy();

    destroyObjects();

    if (m_simulator != nullptr) {
        m_simulator->releaseSimulation();
        delete m_simulator;
        m_simulator = nullptr;
    }

    if (m_iceEngine != nullptr) {
        m_iceEngine->destroy();
        delete m_iceEngine;
        m_iceEngine = nullptr;
    }

    delete m_vehicle;
    m_vehicle = nullptr;

    delete m_transmission;
    m_transmission = nullptr;

    m_audioBuffer.destroy();
}

void EngineSimApplication::loadEngine(
    Engine *engine,
    Vehicle *vehicle,
    Transmission *transmission)
{
    destroyObjects();

    if (m_simulator != nullptr) {
        m_simulator->releaseSimulation();
        delete m_simulator;
        m_simulator = nullptr;
    }

    if (m_vehicle != nullptr) {
        delete m_vehicle;
        m_vehicle = nullptr;
    }

    if (m_transmission != nullptr) {
        delete m_transmission;
        m_transmission = nullptr;
    }

    if (m_iceEngine != nullptr) {
        m_iceEngine->destroy();
        delete m_iceEngine;
        m_iceEngine = nullptr;
    }

    m_iceEngine = engine;
    m_vehicle = vehicle;
    m_transmission = transmission;

    if (engine == nullptr || vehicle == nullptr || transmission == nullptr) {
        // Keep an idle simulator around so that the UI has something to
        // display while no engine is loaded (e.g. after a script error)
        PistonEngineSimulator *idleSimulator = new PistonEngineSimulator;
        idleSimulator->initialize(Simulator::Parameters());
        idleSimulator->loadSimulation(nullptr, nullptr, nullptr);
        m_simulator = idleSimulator;

        m_viewParameters.Layer0 = 0;
        m_viewParameters.Layer1 = 0;
        return;
    }

    m_simulator = engine->createSimulator(vehicle, transmission);

    createObjects(engine);

    m_viewParameters.Layer0 = 0;
    m_viewParameters.Layer1 = engine->getMaxDepth();
    engine->calculateDisplacement();

    m_simulator->setSimulationFrequency(engine->getSimulationFrequency());

    Synthesizer::AudioParameters audioParams = m_simulator->synthesizer().getAudioParameters();
    audioParams.inputSampleNoise = static_cast<float>(engine->getInitialJitter());
    audioParams.airNoise = static_cast<float>(engine->getInitialNoise());
    audioParams.dF_F_mix = static_cast<float>(engine->getInitialHighFrequencyGain());
    audioParams.convolution = static_cast<float>(engine->getInitialConvolution());
    m_simulator->synthesizer().setAudioParameters(audioParams);

    for (int i = 0; i < engine->getExhaustSystemCount(); ++i) {
        ImpulseResponse *response = engine->getExhaustSystem(i)->getImpulseResponse();

        ysAudioWaveFile waveFile;
        const ysAudioFile::Error openError = waveFile.OpenFile(response->getFilename().c_str());
        if (openError != ysAudioFile::Error::None || waveFile.GetSampleCount() == 0) {
            // A missing or unreadable impulse response previously crashed the
            // application; fall back to a pass-through response instead
            m_scriptLog += "Warning: could not load impulse response '" + response->getFilename() + "'\n";

            const int16_t unitImpulse = INT16_MAX;
            m_simulator->synthesizer().initializeImpulseResponse(&unitImpulse, 1, response->getVolume(), i);
            continue;
        }

        waveFile.InitializeInternalBuffer(waveFile.GetSampleCount());
        waveFile.FillBuffer(0);
        waveFile.CloseFile();

        m_simulator->synthesizer().initializeImpulseResponse(
            reinterpret_cast<const int16_t *>(waveFile.GetBuffer()),
            waveFile.GetSampleCount(),
            response->getVolume(),
            i
        );

        waveFile.DestroyInternalBuffer();
    }

    m_simulator->startAudioRenderingThread();
}

void EngineSimApplication::drawGenerated(
    const GeometryGenerator::GeometryIndices &indices,
    int layer)
{
    drawGenerated(indices, layer, m_shaders.GetRegularFlags());
}

void EngineSimApplication::drawGeneratedUi(
    const GeometryGenerator::GeometryIndices &indices,
    int layer)
{
    drawGenerated(indices, layer, m_shaders.GetUiFlags());
}

void EngineSimApplication::drawGenerated(
    const GeometryGenerator::GeometryIndices &indices,
    int layer,
    dbasic::StageEnableFlags flags)
{
    m_engine.DrawGeneric(
        flags,
        m_geometryIndexBuffer,
        m_geometryVertexBuffer,
        sizeof(dbasic::Vertex),
        indices.BaseIndex,
        indices.BaseVertex,
        indices.FaceCount,
        false,
        layer);
}

void EngineSimApplication::configure(const ApplicationSettings &settings) {
    m_applicationSettings = settings;

    if (settings.startFullscreen) {
        m_engine.GetGameWindow()->SetWindowStyle(ysWindow::WindowStyle::Fullscreen);
    }

    m_background = ysColor::srgbiToLinear(m_applicationSettings.colorBackground);
    m_foreground = ysColor::srgbiToLinear(m_applicationSettings.colorForeground);
    m_shadow = ysColor::srgbiToLinear(m_applicationSettings.colorShadow);
    m_highlight1 = ysColor::srgbiToLinear(m_applicationSettings.colorHighlight1);
    m_highlight2 = ysColor::srgbiToLinear(m_applicationSettings.colorHighlight2);
    m_pink = ysColor::srgbiToLinear(m_applicationSettings.colorPink);
    m_red = ysColor::srgbiToLinear(m_applicationSettings.colorRed);
    m_orange = ysColor::srgbiToLinear(m_applicationSettings.colorOrange);
    m_yellow = ysColor::srgbiToLinear(m_applicationSettings.colorYellow);
    m_blue = ysColor::srgbiToLinear(m_applicationSettings.colorBlue);
    m_green = ysColor::srgbiToLinear(m_applicationSettings.colorGreen);
}

void EngineSimApplication::createObjects(Engine *engine) {
    WankelEngine *wankelEngine = dynamic_cast<WankelEngine *>(engine);
    if (wankelEngine != nullptr) {
        for (int i = 0; i < wankelEngine->getRotorCount(); ++i) {
            WankelRotorObject *rotorObject = new WankelRotorObject;
            rotorObject->initialize(this);
            rotorObject->m_engine = wankelEngine;
            rotorObject->m_rotor = wankelEngine->getRotor(i);
            m_objects.push_back(rotorObject);
        }

        for (int i = 0; i < engine->getCrankshaftCount(); ++i) {
            CrankshaftObject *crankshaftObject = new CrankshaftObject;
            crankshaftObject->initialize(this);
            crankshaftObject->m_crankshaft = engine->getCrankshaft(i);
            m_objects.push_back(crankshaftObject);
        }

        return;
    }

    for (int i = 0; i < engine->getCylinderCount(); ++i) {
        ConnectingRodObject *rodObject = new ConnectingRodObject;
        rodObject->initialize(this);
        rodObject->m_connectingRod = engine->getConnectingRod(i);
        m_objects.push_back(rodObject);

        PistonObject *pistonObject = new PistonObject;
        pistonObject->initialize(this);
        pistonObject->m_piston = engine->getPiston(i);
        m_objects.push_back(pistonObject);

        CombustionChamberObject *ccObject = new CombustionChamberObject;
        ccObject->initialize(this);
        ccObject->m_chamber = m_iceEngine->getChamber(i);
        m_objects.push_back(ccObject);
    }

    for (int i = 0; i < engine->getCrankshaftCount(); ++i) {
        CrankshaftObject *crankshaftObject = new CrankshaftObject;
        crankshaftObject->initialize(this);
        crankshaftObject->m_crankshaft = engine->getCrankshaft(i);
        m_objects.push_back(crankshaftObject);
    }

    for (int i = 0; i < engine->getCylinderBankCount(); ++i) {
        CylinderBankObject *cbObject = new CylinderBankObject;
        cbObject->initialize(this);
        cbObject->m_bank = engine->getCylinderBank(i);
        cbObject->m_head = engine->getHead(i);
        m_objects.push_back(cbObject);

        CylinderHeadObject *chObject = new CylinderHeadObject;
        chObject->initialize(this);
        chObject->m_head = engine->getHead(i);
        chObject->m_engine = engine;
        m_objects.push_back(chObject);
    }
}

void EngineSimApplication::destroyObjects() {
    for (SimulationObject *object : m_objects) {
        object->destroy();
        delete object;
    }

    m_objects.clear();
}

const SimulationObject::ViewParameters &
    EngineSimApplication::getViewParameters() const
{
    return m_viewParameters;
}

bool EngineSimApplication::compileScript(
    const std::string &path,
    Engine **engine,
    Vehicle **vehicle,
    Transmission **transmission,
    ApplicationSettings *settings,
    bool *settingsSet)
{
    *engine = nullptr;
    *vehicle = nullptr;
    *transmission = nullptr;
    *settingsSet = false;

#ifdef ATG_ENGINE_SIM_PIRANHA_ENABLED
    std::stringstream log;

    es_script::Compiler compiler;
    compiler.initialize(getScriptSearchPaths());
    const bool compiled = compiler.compile(path, log);
    if (compiled) {
        const es_script::Compiler::Output output = compiler.execute();

        *engine = output.engine;
        *vehicle = output.vehicle;
        *transmission = output.transmission;
        *settings = output.applicationSettings;
        *settingsSet = output.applicationSettingsSet;
    }

    compiler.destroy();

    const std::string errors = log.str();
    if (!errors.empty()) {
        m_scriptLog += fs::path(path).filename().string() + ":\n" + errors;
    }

    // Keep a copy of the log on disk for reference
    if (!m_userDataPath.empty()) {
        std::ofstream logFile(fs::path(m_userDataPath) / "error_log.log", std::ios::out | std::ios::trunc);
        logFile << m_scriptLog;
    }

    return compiled;
#else
    (void)path;
    (void)settings;
    m_scriptLog += "Scripting support is disabled in this build\n";
    return false;
#endif /* ATG_ENGINE_SIM_PIRANHA_ENABLED */
}

void EngineSimApplication::loadScript() {
    m_scriptLog.clear();
    m_scriptErrors = false;

    Engine *engine = nullptr;
    Vehicle *vehicle = nullptr;
    Transmission *transmission = nullptr;

    ApplicationSettings settings;
    bool settingsSet = false;

    if (!compileScript(m_engineScriptPath, &engine, &vehicle, &transmission, &settings, &settingsSet)) {
        m_scriptErrors = true;
    }
    else if (engine == nullptr) {
        m_scriptLog +=
            "The script did not specify an engine. Add a call such as\n"
            "    run(engine: my_engine())\n"
            "or call main() at the end of the file.\n";
        m_scriptErrors = true;
    }

    // A theme selected by the user takes priority over one set by the engine
    // script (e.g. the legacy main.mr)
    if (!m_themeScriptPath.empty()) {
        Engine *themeEngine = nullptr;
        Vehicle *themeVehicle = nullptr;
        Transmission *themeTransmission = nullptr;
        ApplicationSettings themeSettings;
        bool themeSettingsSet = false;

        if (compileScript(m_themeScriptPath, &themeEngine, &themeVehicle, &themeTransmission, &themeSettings, &themeSettingsSet)
            && themeSettingsSet)
        {
            settings = themeSettings;
            settingsSet = true;
        }

        // Theme scripts should only contain settings
        if (themeEngine != nullptr) {
            themeEngine->destroy();
            delete themeEngine;
        }

        delete themeVehicle;
        delete themeTransmission;
    }

    if (settingsSet) {
        configure(settings);
    }

    if (engine != nullptr && vehicle == nullptr) {
        Vehicle::Parameters vehParams;
        vehParams.mass = units::mass(1597, units::kg);
        vehParams.diffRatio = 3.42;
        vehParams.tireRadius = units::distance(10, units::inch);
        vehParams.dragCoefficient = 0.25;
        vehParams.crossSectionArea = units::distance(6.0, units::foot) * units::distance(6.0, units::foot);
        vehParams.rollingResistance = 2000.0;
        vehicle = new Vehicle;
        vehicle->initialize(vehParams);
    }

    if (engine != nullptr && transmission == nullptr) {
        const double gearRatios[] = { 2.97, 2.07, 1.43, 1.00, 0.84, 0.56 };
        Transmission::Parameters tParams;
        tParams.GearCount = 6;
        tParams.GearRatios = gearRatios;
        tParams.MaxClutchTorque = units::torque(1000.0, units::ft_lb);
        transmission = new Transmission;
        transmission->initialize(tParams);
    }

    if (engine == nullptr) {
        delete vehicle;
        delete transmission;
        vehicle = nullptr;
        transmission = nullptr;
    }

    // Keep the current engine running if the new script failed to produce one
    if (engine != nullptr || m_iceEngine == nullptr) {
        loadEngine(engine, vehicle, transmission);
    }

    refreshUserInterface();

    if (m_scriptErrors) {
        m_infoCluster->setLogMessage("<ERROR: CHECK CONSOLE [F5]> " + fs::path(m_engineScriptPath).filename().string());
    }
    else {
        m_infoCluster->setLogMessage("Loaded " + fs::path(m_engineScriptPath).filename().string());
    }
}

void EngineSimApplication::loadEngineScript(const std::string &path) {
    m_engineScriptPath = normalizePath(fs::absolute(path));
    reloadScripts();
    saveSession();
}

void EngineSimApplication::loadThemeScript(const std::string &path) {
    m_themeScriptPath = path.empty() ? std::string() : normalizePath(fs::absolute(path));
    reloadScripts();
    saveSession();
}

void EngineSimApplication::reloadScripts() {
    if (m_audioSource != nullptr) {
        m_audioSource->SetMode(ysAudioSource::Mode::Stop);
    }

    loadScript();

    if (m_audioSource != nullptr && m_simulator != nullptr && m_simulator->getEngine() != nullptr) {
        m_audioSource->SetMode(ysAudioSource::Mode::Loop);
    }
}

namespace {
    // Engine scripts call main() or run() at the top level; files without
    // such a call are part libraries (e.g. radial.mr)
    bool isRunnableScript(const fs::path &path) {
        std::ifstream file(path);
        std::string line;
        while (std::getline(file, line)) {
            const size_t start = line.find_first_not_of(" \t");
            if (start != 0) continue;
            if (line.compare(0, 6, "main()") == 0 || line.compare(0, 4, "run(") == 0) {
                return true;
            }
        }

        return false;
    }

    std::vector<std::string> findScripts(
        const std::vector<fs::path> &directories,
        bool recursive,
        bool runnableOnly)
    {
        std::vector<std::string> scripts;
        for (const fs::path &directory : directories) {
            std::error_code ec;
            if (!fs::is_directory(directory, ec)) continue;

            auto consider = [&](const fs::directory_entry &entry) {
                if (!entry.is_regular_file() || entry.path().extension() != ".mr") return;
                if (runnableOnly && !isRunnableScript(entry.path())) return;
                scripts.push_back(normalizePath(entry.path()));
            };

            if (recursive) {
                for (const auto &entry : fs::recursive_directory_iterator(directory, ec)) consider(entry);
            }
            else {
                for (const auto &entry : fs::directory_iterator(directory, ec)) consider(entry);
            }
        }

        std::sort(scripts.begin(), scripts.end());
        scripts.erase(std::unique(scripts.begin(), scripts.end()), scripts.end());
        return scripts;
    }

    int nextIndex(const std::vector<std::string> &list, const std::string &current, int direction) {
        const int n = static_cast<int>(list.size());
        const auto it = std::find(list.begin(), list.end(), current);
        if (it == list.end()) return direction > 0 ? 0 : n - 1;

        const int i = static_cast<int>(it - list.begin());
        return ((i + direction) % n + n) % n;
    }
} /* namespace */

std::vector<std::string> EngineSimApplication::findEngineScripts() const {
    std::vector<std::string> scripts = findScripts(
        { fs::path(m_assetPath) / "engines", fs::path(m_userDataPath) / "assets" / "engines" },
        true,
        true);

    const std::string mainScript = normalizePath(fs::path(m_assetPath) / "main.mr");
    if (fileExists(mainScript)) {
        scripts.insert(scripts.begin(), mainScript);
    }

    return scripts;
}

std::vector<std::string> EngineSimApplication::findThemeScripts() const {
    return findScripts(
        { fs::path(m_assetPath) / "themes", fs::path(m_userDataPath) / "assets" / "themes" },
        false,
        false);
}

void EngineSimApplication::cycleEngineScript(int direction) {
    const std::vector<std::string> scripts = findEngineScripts();
    if (scripts.empty()) {
        m_infoCluster->setLogMessage("No engine scripts found");
        return;
    }

    const int index = nextIndex(scripts, m_engineScriptPath, direction);
    loadEngineScript(scripts[index]);

    if (!m_scriptErrors) {
        m_infoCluster->setLogMessage(
            "[" + std::to_string(index + 1) + "/" + std::to_string(scripts.size()) + "] "
            + fs::path(scripts[index]).filename().string());
    }
}

void EngineSimApplication::cycleThemeScript(int direction) {
    std::vector<std::string> themes = findThemeScripts();

    // The first entry restores the theme set by the engine script (if any)
    themes.insert(themes.begin(), std::string());

    const int index = nextIndex(themes, m_themeScriptPath, direction);
    loadThemeScript(themes[index]);

    if (!m_scriptErrors) {
        m_infoCluster->setLogMessage(
            themes[index].empty()
                ? std::string("Theme: script default")
                : "Theme: " + fs::path(themes[index]).stem().string());
    }
}

void EngineSimApplication::processEngineInput() {
    if (m_iceEngine == nullptr) {
        return;
    }

    const float dt = m_engine.GetFrameLength();
    const bool fineControlMode = m_engine.IsKeyDown(ysKey::Code::Space);

    const int mouseWheel = m_engine.GetMouseWheel();
    const int mouseWheelDelta = mouseWheel - m_lastMouseWheel;
    m_lastMouseWheel = mouseWheel;

    bool fineControlInUse = false;
    if (m_engine.IsKeyDown(ysKey::Code::Z)) {
        const double rate = fineControlMode
            ? 0.001
            : 0.01;

        Synthesizer::AudioParameters audioParams = m_simulator->synthesizer().getAudioParameters();
        audioParams.volume = clamp(audioParams.volume + mouseWheelDelta * rate * dt);

        m_simulator->synthesizer().setAudioParameters(audioParams);
        fineControlInUse = true;

        m_infoCluster->setLogMessage("[Z] - Set volume to " + std::to_string(audioParams.volume));
    }
    else if (m_engine.IsKeyDown(ysKey::Code::X)) {
        const double rate = fineControlMode
            ? 0.001
            : 0.01;

        Synthesizer::AudioParameters audioParams = m_simulator->synthesizer().getAudioParameters();
        audioParams.convolution = clamp(audioParams.convolution + mouseWheelDelta * rate * dt);

        m_simulator->synthesizer().setAudioParameters(audioParams);
        fineControlInUse = true;

        m_infoCluster->setLogMessage("[X] - Set convolution level to " + std::to_string(audioParams.convolution));
    }
    else if (m_engine.IsKeyDown(ysKey::Code::C)) {
        const double rate = fineControlMode
            ? 0.00001
            : 0.001;

        Synthesizer::AudioParameters audioParams = m_simulator->synthesizer().getAudioParameters();
        audioParams.dF_F_mix = clamp(audioParams.dF_F_mix + mouseWheelDelta * rate * dt);

        m_simulator->synthesizer().setAudioParameters(audioParams);
        fineControlInUse = true;

        m_infoCluster->setLogMessage("[C] - Set high freq. gain to " + std::to_string(audioParams.dF_F_mix));
    }
    else if (m_engine.IsKeyDown(ysKey::Code::V)) {
        const double rate = fineControlMode
            ? 0.001
            : 0.01;

        Synthesizer::AudioParameters audioParams = m_simulator->synthesizer().getAudioParameters();
        audioParams.airNoise = clamp(audioParams.airNoise + mouseWheelDelta * rate * dt);

        m_simulator->synthesizer().setAudioParameters(audioParams);
        fineControlInUse = true;

        m_infoCluster->setLogMessage("[V] - Set low freq. noise to " + std::to_string(audioParams.airNoise));
    }
    else if (m_engine.IsKeyDown(ysKey::Code::B)) {
        const double rate = fineControlMode
            ? 0.001
            : 0.01;

        Synthesizer::AudioParameters audioParams = m_simulator->synthesizer().getAudioParameters();
        audioParams.inputSampleNoise = clamp(audioParams.inputSampleNoise + mouseWheelDelta * rate * dt);

        m_simulator->synthesizer().setAudioParameters(audioParams);
        fineControlInUse = true;

        m_infoCluster->setLogMessage("[B] - Set high freq. noise to " + std::to_string(audioParams.inputSampleNoise));
    }
    else if (m_engine.IsKeyDown(ysKey::Code::N)) {
        const double rate = fineControlMode
            ? 10.0
            : 100.0;

        const double newSimulationFrequency = clamp(
            m_simulator->getSimulationFrequency() + mouseWheelDelta * rate * dt,
            400.0, 400000.0);

        m_simulator->setSimulationFrequency(newSimulationFrequency);
        fineControlInUse = true;

        m_infoCluster->setLogMessage("[N] - Set simulation freq to " + std::to_string(m_simulator->getSimulationFrequency()));
    }
    else if (m_engine.IsKeyDown(ysKey::Code::G) && m_simulator->m_dyno.m_hold) {
        if (mouseWheelDelta > 0) {
            m_dynoSpeed += m_iceEngine->getDynoHoldStep();
        }
        else if (mouseWheelDelta < 0) {
            m_dynoSpeed -= m_iceEngine->getDynoHoldStep();
        }

        m_dynoSpeed = clamp(m_dynoSpeed, m_iceEngine->getDynoMinSpeed(), m_iceEngine->getDynoMaxSpeed());

        m_infoCluster->setLogMessage("[G] - Set dyno speed to " + std::to_string(units::toRpm(m_dynoSpeed)));
        fineControlInUse = true;
    }

    const double prevTargetThrottle = m_targetSpeedSetting;
    m_targetSpeedSetting = fineControlMode ? m_targetSpeedSetting : 0.0;
    if (m_engine.IsKeyDown(ysKey::Code::Q)) {
        m_targetSpeedSetting = 0.01;
    }
    else if (m_engine.IsKeyDown(ysKey::Code::W)) {
        m_targetSpeedSetting = 0.1;
    }
    else if (m_engine.IsKeyDown(ysKey::Code::E)) {
        m_targetSpeedSetting = 0.2;
    }
    else if (m_engine.IsKeyDown(ysKey::Code::R)) {
        m_targetSpeedSetting = 1.0;
    }
    else if (fineControlMode && !fineControlInUse) {
        m_targetSpeedSetting = clamp(m_targetSpeedSetting + mouseWheelDelta * 0.0001);
    }

    if (prevTargetThrottle != m_targetSpeedSetting) {
        m_infoCluster->setLogMessage("Speed control set to " + std::to_string(m_targetSpeedSetting));
    }

    m_speedSetting = m_targetSpeedSetting * 0.5 + 0.5 * m_speedSetting;

    m_iceEngine->setSpeedControl(m_speedSetting);
    if (m_engine.ProcessKeyDown(ysKey::Code::M)) {
        const int currentLayer = getViewParameters().Layer0;
        if (currentLayer + 1 < m_iceEngine->getMaxDepth()) {
            setViewLayer(currentLayer + 1);
        }

        m_infoCluster->setLogMessage("[M] - Set render layer to " + std::to_string(getViewParameters().Layer0));
    }

    if (m_engine.ProcessKeyDown(ysKey::Code::OEM_Comma)) {
        if (getViewParameters().Layer0 - 1 >= 0)
            setViewLayer(getViewParameters().Layer0 - 1);

        m_infoCluster->setLogMessage("[,] - Set render layer to " + std::to_string(getViewParameters().Layer0));
    }

    if (m_engine.ProcessKeyDown(ysKey::Code::D)) {
        m_simulator->m_dyno.m_enabled = !m_simulator->m_dyno.m_enabled;

        const std::string msg = m_simulator->m_dyno.m_enabled
            ? "DYNOMOMETER ENABLED"
            : "DYNOMOMETER DISABLED";
        m_infoCluster->setLogMessage(msg);
    }

    if (m_engine.ProcessKeyDown(ysKey::Code::H)) {
        m_simulator->m_dyno.m_hold = !m_simulator->m_dyno.m_hold;

        const std::string msg = m_simulator->m_dyno.m_hold
            ? m_simulator->m_dyno.m_enabled ? "HOLD ENABLED" : "HOLD ON STANDBY [ENABLE DYNO. FOR HOLD]"
            : "HOLD DISABLED";
        m_infoCluster->setLogMessage(msg);
    }

    if (m_simulator->m_dyno.m_enabled) {
        if (!m_simulator->m_dyno.m_hold) {
            if (m_simulator->getFilteredDynoTorque() > units::torque(1.0, units::ft_lb)) {
                m_dynoSpeed += units::rpm(500) * dt;
            }
            else {
                m_dynoSpeed *= (1 / (1 + dt));
            }

            if (m_dynoSpeed > m_iceEngine->getRedline()) {
                m_simulator->m_dyno.m_enabled = false;
                m_dynoSpeed = units::rpm(0);
            }
        }
    }
    else {
        if (!m_simulator->m_dyno.m_hold) {
            m_dynoSpeed = units::rpm(0);
        }
    }

    m_dynoSpeed = clamp(m_dynoSpeed, m_iceEngine->getDynoMinSpeed(), m_iceEngine->getDynoMaxSpeed());
    m_simulator->m_dyno.m_rotationSpeed = m_dynoSpeed;

    const bool prevStarterEnabled = m_simulator->m_starterMotor.m_enabled;
    if (m_engine.IsKeyDown(ysKey::Code::S)) {
        m_simulator->m_starterMotor.m_enabled = true;
    }
    else {
        m_simulator->m_starterMotor.m_enabled = false;
    }

    if (prevStarterEnabled != m_simulator->m_starterMotor.m_enabled) {
        const std::string msg = m_simulator->m_starterMotor.m_enabled
            ? "STARTER ENABLED"
            : "STARTER DISABLED";
        m_infoCluster->setLogMessage(msg);
    }

    if (m_engine.ProcessKeyDown(ysKey::Code::A)) {
        m_simulator->getEngine()->getIgnitionModule()->m_enabled =
            !m_simulator->getEngine()->getIgnitionModule()->m_enabled;

        const std::string msg = m_simulator->getEngine()->getIgnitionModule()->m_enabled
            ? "IGNITION ENABLED"
            : "IGNITION DISABLED";
        m_infoCluster->setLogMessage(msg);
    }

    if (m_engine.ProcessKeyDown(ysKey::Code::Up)) {
        m_simulator->getTransmission()->changeGear(m_simulator->getTransmission()->getGear() + 1);

        m_infoCluster->setLogMessage(
            "UPSHIFTED TO " + std::to_string(m_simulator->getTransmission()->getGear() + 1));
    }
    else if (m_engine.ProcessKeyDown(ysKey::Code::Down)) {
        m_simulator->getTransmission()->changeGear(m_simulator->getTransmission()->getGear() - 1);

        if (m_simulator->getTransmission()->getGear() != -1) {
            m_infoCluster->setLogMessage(
                "DOWNSHIFTED TO " + std::to_string(m_simulator->getTransmission()->getGear() + 1));
        }
        else {
            m_infoCluster->setLogMessage("SHIFTED TO NEUTRAL");
        }
    }

    const bool braking = m_engine.IsKeyDown(ysKey::Code::OEM_Period);
    if (m_vehicle != nullptr && braking != (m_vehicle->getBrakeLevel() > 0)) {
        m_vehicle->setBrakeLevel(braking ? 1.0 : 0.0);
        m_infoCluster->setLogMessage(braking ? "BRAKE APPLIED" : "BRAKE RELEASED");
    }

    if (m_engine.IsKeyDown(ysKey::Code::T)) {
        m_targetClutchPressure -= 0.2 * dt;
    }
    else if (m_engine.IsKeyDown(ysKey::Code::U)) {
        m_targetClutchPressure += 0.2 * dt;
    }
    else if (m_engine.IsKeyDown(ysKey::Code::Shift)) {
        m_targetClutchPressure = 0.0;
        m_infoCluster->setLogMessage("CLUTCH DEPRESSED");
    }
    else if (!m_engine.IsKeyDown(ysKey::Code::Y)) {
        m_targetClutchPressure = 1.0;
    }

    m_targetClutchPressure = clamp(m_targetClutchPressure);

    double clutchRC = 0.001;
    if (m_engine.IsKeyDown(ysKey::Code::Space)) {
        clutchRC = 1.0;
    }

    const double clutch_s = dt / (dt + clutchRC);
    m_clutchPressure = m_clutchPressure * (1 - clutch_s) + m_targetClutchPressure * clutch_s;
    m_simulator->getTransmission()->setClutchPressure(m_clutchPressure);
}

void EngineSimApplication::renderScene() {
    getShaders()->ResetBaseColor();
    getShaders()->SetObjectTransform(ysMath::LoadIdentity());

    m_textRenderer.SetColor(ysColor::linearToSrgb(m_foreground));
    m_shaders.SetClearColor(ysColor::linearToSrgb(m_shadow));

    const int screenWidth = m_engine.GetGameWindow()->GetGameWidth();
    const int screenHeight = m_engine.GetGameWindow()->GetGameHeight();
    const float aspectRatio = screenWidth / (float)screenHeight;

    const Point cameraPos = m_engineView->getCameraPosition();
    m_shaders.m_cameraPosition = ysMath::LoadVector(cameraPos.x, cameraPos.y);

    m_shaders.CalculateUiCamera(screenWidth, screenHeight);

    if (m_screen == 0) {
        Bounds windowBounds((float)screenWidth, (float)screenHeight, { 0, (float)screenHeight });
        Grid grid;
        grid.v_cells = 2;
        grid.h_cells = 3;
        Grid grid3x3;
        grid3x3.v_cells = 3;
        grid3x3.h_cells = 3;
        m_engineView->setDrawFrame(true);
        m_engineView->setBounds(grid.get(windowBounds, 1, 0, 1, 1));
        m_engineView->setLocalPosition({ 0, 0 });

        m_rightGaugeCluster->m_bounds = grid.get(windowBounds, 2, 0, 1, 2);
        m_oscCluster->m_bounds = grid.get(windowBounds, 1, 1);
        m_performanceCluster->m_bounds = grid3x3.get(windowBounds, 0, 1);
        m_loadSimulationCluster->m_bounds = grid3x3.get(windowBounds, 0, 2);

        Grid grid1x3;
        grid1x3.v_cells = 3;
        grid1x3.h_cells = 1;
        m_mixerCluster->m_bounds = grid1x3.get(grid3x3.get(windowBounds, 0, 0), 0, 2);
        m_infoCluster->m_bounds = grid1x3.get(grid3x3.get(windowBounds, 0, 0), 0, 0, 1, 2);

        m_engineView->setVisible(true);
        m_rightGaugeCluster->setVisible(true);
        m_oscCluster->setVisible(true);
        m_performanceCluster->setVisible(true);
        m_loadSimulationCluster->setVisible(true);
        m_mixerCluster->setVisible(true);
        m_infoCluster->setVisible(true);

        m_oscCluster->activate();
    }
    else if (m_screen == 1) {
        Bounds windowBounds((float)screenWidth, (float)screenHeight, { 0, (float)screenHeight });
        m_engineView->setDrawFrame(false);
        m_engineView->setBounds(windowBounds);
        m_engineView->setLocalPosition({ 0, 0 });
        m_engineView->activate();

        m_engineView->setVisible(true);
        m_rightGaugeCluster->setVisible(false);
        m_oscCluster->setVisible(false);
        m_performanceCluster->setVisible(false);
        m_loadSimulationCluster->setVisible(false);
        m_mixerCluster->setVisible(false);
        m_infoCluster->setVisible(false);
    }
    else if (m_screen == 2) {
        Bounds windowBounds((float)screenWidth, (float)screenHeight, { 0, (float)screenHeight });
        Grid grid;
        grid.v_cells = 1;
        grid.h_cells = 3;
        m_engineView->setDrawFrame(true);
        m_engineView->setBounds(grid.get(windowBounds, 0, 0, 2, 1));
        m_engineView->setLocalPosition({ 0, 0 });
        m_engineView->activate();

        m_rightGaugeCluster->m_bounds = grid.get(windowBounds, 2, 0, 1, 1);

        m_engineView->setVisible(true);
        m_rightGaugeCluster->setVisible(true);
        m_oscCluster->setVisible(false);
        m_performanceCluster->setVisible(false);
        m_loadSimulationCluster->setVisible(false);
        m_mixerCluster->setVisible(false);
        m_infoCluster->setVisible(false);
    }

    {
        const Bounds windowBounds((float)screenWidth, (float)screenHeight, { 0, (float)screenHeight });
        m_console->m_bounds = windowBounds.inset(40.0f);
        m_console->setVisible(m_showConsole);
        if (m_showConsole) {
            // Text is drawn in a separate pass so the console's background
            // can't cover the other panels; hide them instead
            m_engineView->setVisible(false);
            m_rightGaugeCluster->setVisible(false);
            m_oscCluster->setVisible(false);
            m_performanceCluster->setVisible(false);
            m_loadSimulationCluster->setVisible(false);
            m_mixerCluster->setVisible(false);
            m_infoCluster->setVisible(false);
            m_console->activate();
        }
    }

    const float cameraAspectRatio =
        m_engineView->m_bounds.width() / m_engineView->m_bounds.height();
    m_engine.GetDevice()->ResizeRenderTarget(
        m_mainRenderTarget,
        m_engineView->m_bounds.width(),
        m_engineView->m_bounds.height(),
        m_engineView->m_bounds.width(),
        m_engineView->m_bounds.height()
    );
    m_engine.GetDevice()->RepositionRenderTarget(
        m_mainRenderTarget,
        m_engineView->m_bounds.getPosition(Bounds::tl).x,
        screenHeight - m_engineView->m_bounds.getPosition(Bounds::tl).y
    );
    m_shaders.CalculateCamera(
        cameraAspectRatio * m_displayHeight / m_engineView->m_zoom,
        m_displayHeight / m_engineView->m_zoom,
        m_engineView->m_bounds,
        m_screenWidth,
        m_screenHeight,
        m_displayAngle);

    m_geometryGenerator.reset();

    render();

    m_engine.GetDevice()->EditBufferDataRange(
        m_geometryVertexBuffer,
        (char *)m_geometryGenerator.getVertexData(),
        sizeof(dbasic::Vertex) * m_geometryGenerator.getCurrentVertexCount(),
        0);

    m_engine.GetDevice()->EditBufferDataRange(
        m_geometryIndexBuffer,
        (char *)m_geometryGenerator.getIndexData(),
        sizeof(unsigned short) * m_geometryGenerator.getCurrentIndexCount(),
        0);
}

void EngineSimApplication::refreshUserInterface() {
    m_uiManager.destroy();
    m_uiManager.initialize(this);

    m_engineView = m_uiManager.getRoot()->addElement<EngineView>();
    m_rightGaugeCluster = m_uiManager.getRoot()->addElement<RightGaugeCluster>();
    m_oscCluster = m_uiManager.getRoot()->addElement<OscilloscopeCluster>();
    m_performanceCluster = m_uiManager.getRoot()->addElement<PerformanceCluster>();
    m_loadSimulationCluster = m_uiManager.getRoot()->addElement<LoadSimulationCluster>();
    m_mixerCluster = m_uiManager.getRoot()->addElement<MixerCluster>();
    m_infoCluster = m_uiManager.getRoot()->addElement<InfoCluster>();

    // Added last so that it's drawn on top of everything else
    m_console = m_uiManager.getRoot()->addElement<ConsoleOverlay>();

    m_infoCluster->setEngine(m_iceEngine);
    m_rightGaugeCluster->m_simulator = m_simulator;
    m_rightGaugeCluster->setEngine(m_iceEngine);
    m_oscCluster->setSimulator(m_simulator);
    if (m_iceEngine != nullptr) {
        m_oscCluster->setDynoMaxRange(units::toRpm(m_iceEngine->getRedline()));
    }
    m_performanceCluster->setSimulator(m_simulator);
    m_loadSimulationCluster->setSimulator(m_simulator);
    m_mixerCluster->setSimulator(m_simulator);
}

void EngineSimApplication::startRecording() {
    m_recording = true;

#ifdef ATG_ENGINE_SIM_VIDEO_CAPTURE
    atg_dtv::Encoder::VideoSettings settings{};

    // Output filename
    settings.fname = "../workspace/video_capture/engine_sim_video_capture.mp4";
    settings.inputWidth = m_engine.GetScreenWidth();
    settings.inputHeight = m_engine.GetScreenHeight();
    settings.width = settings.inputWidth;
    settings.height = settings.inputHeight;
    settings.hardwareEncoding = true;
    settings.inputAlpha = true;
    settings.bitRate = 40000000;

    m_encoder.run(settings, 2);
#endif /* ATG_ENGINE_SIM_VIDEO_CAPTURE */
}

void EngineSimApplication::updateScreenSizeStability() {
    m_screenResolution[m_screenResolutionIndex][0] = m_engine.GetScreenWidth();
    m_screenResolution[m_screenResolutionIndex][1] = m_engine.GetScreenHeight();

    m_screenResolutionIndex = (m_screenResolutionIndex + 1) % ScreenResolutionHistoryLength;
}

bool EngineSimApplication::readyToRecord() {
    const int w = m_screenResolution[0][0];
    const int h = m_screenResolution[0][1];

    if (w <= 0 && h <= 0) return false;
    if ((w % 2) != 0 || (h % 2) != 0) return false;

    for (int i = 1; i < ScreenResolutionHistoryLength; ++i) {
        if (m_screenResolution[i][0] != w) return false;
        if (m_screenResolution[i][1] != h) return false;
    }

    return true;
}

void EngineSimApplication::stopRecording() {
    m_recording = false;

#ifdef ATG_ENGINE_SIM_VIDEO_CAPTURE
    m_encoder.commit();
    m_encoder.stop();
#endif /* ATG_ENGINE_SIM_VIDEO_CAPTURE */
}

void EngineSimApplication::recordFrame() {
#ifdef ATG_ENGINE_SIM_VIDEO_CAPTURE
    atg_dtv::Frame *frame = m_encoder.newFrame(false);
    if (frame != nullptr && m_encoder.getError() == atg_dtv::Encoder::Error::None) {
        m_engine.GetDevice()->ReadRenderTarget(m_engine.GetScreenRenderTarget(), frame->m_rgb);
    }

    m_encoder.submitFrame();
#endif /* ATG_ENGINE_SIM_VIDEO_CAPTURE */
}
