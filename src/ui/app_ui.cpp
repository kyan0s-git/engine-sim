#include "../../include/ui/app_ui.h"

#include "../../include/ui/ui_widgets.h"
#include "../../include/engine_sim_application.h"
#include "../../include/engine.h"
#include "../../include/simulator.h"
#include "../../include/combustion_chamber.h"
#include "../../include/crankshaft.h"
#include "../../include/ignition_module.h"
#include "../../include/intake.h"
#include "../../include/fuel.h"
#include "../../include/transmission.h"
#include "../../include/vehicle.h"
#include "../../include/units.h"
#include "../../include/constants.h"

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl2.h>
#include <implot.h>

#include <delta-studio/include/yds_opengl_sdl_context.h>
#include <delta-studio/include/yds_sdl_window_system.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>

namespace fs = std::filesystem;

namespace {

    constexpr float NaN = std::numeric_limits<float>::quiet_NaN();
    constexpr double Ambient = units::pressure(1.0, units::atm);

#ifdef __APPLE__
    constexpr const char *SettingsShortcut = "Cmd+,";
    constexpr const char *LibraryShortcut = "Cmd+L";
    constexpr const char *DesignerShortcut = "Cmd+D";
#else
    constexpr const char *SettingsShortcut = "Ctrl+,";
    constexpr const char *LibraryShortcut = "Ctrl+L";
    constexpr const char *DesignerShortcut = "Ctrl+D";
#endif

    double now() {
        return SDL_GetTicks() / 1000.0;
    }

    ImVec4 rgb(int color, float alpha = 1.0f) {
        return ImVec4(
            ((color >> 16) & 0xFF) / 255.0f,
            ((color >> 8) & 0xFF) / 255.0f,
            (color & 0xFF) / 255.0f,
            alpha);
    }

    ImVec4 mix(const ImVec4 &a, const ImVec4 &b, float s) {
        return ImVec4(
            a.x + (b.x - a.x) * s,
            a.y + (b.y - a.y) * s,
            a.z + (b.z - a.z) * s,
            a.w + (b.w - a.w) * s);
    }

    // Accent colors applied on top of the base styles
    void applyAccent(ImGuiStyle &style, const ImVec4 &accent) {
        ImVec4 *c = style.Colors;
        c[ImGuiCol_CheckMark] = accent;
        c[ImGuiCol_SliderGrab] = mix(accent, ImVec4(0, 0, 0, 1), 0.1f);
        c[ImGuiCol_SliderGrabActive] = accent;
        c[ImGuiCol_ButtonActive] = mix(accent, ImVec4(0, 0, 0, 1), 0.2f);
        c[ImGuiCol_HeaderActive] = mix(accent, ImVec4(0, 0, 0, 1), 0.2f);
        c[ImGuiCol_TabSelectedOverline] = accent;
        c[ImGuiCol_PlotHistogram] = accent;
        c[ImGuiCol_PlotLines] = accent;
        c[ImGuiCol_TextSelectedBg] = ImVec4(accent.x, accent.y, accent.z, 0.35f);
        c[ImGuiCol_ResizeGripActive] = accent;
        c[ImGuiCol_SeparatorActive] = accent;
        c[ImGuiCol_NavCursor] = accent;
    }

    std::string formatDisplacement(double volume) {
        char buffer[64];
        if (volume < units::volume(1.0, units::L)) {
            std::snprintf(buffer, sizeof(buffer), "%.0f cc (%.0f CI)",
                units::convert(volume, units::cc), units::convert(volume, units::cubic_inches));
        }
        else {
            std::snprintf(buffer, sizeof(buffer), "%.1f L (%.0f CI)",
                units::convert(volume, units::L), units::convert(volume, units::cubic_inches));
        }

        return buffer;
    }

    // Script display name: folder/file without the extension
    std::string scriptLabel(const std::string &path) {
        const fs::path p(path);
        return p.parent_path().filename().string() + "/" + p.stem().string();
    }

    bool sliderDouble(const char *label, double *v, double min, double max, const char *format, double scale = 1.0) {
        float f = static_cast<float>(*v * scale);
        const bool changed = ImGui::SliderFloat(label, &f, (float)(min * scale), (float)(max * scale), format);
        if (changed) *v = f / scale;
        return changed;
    }

    bool inputDouble(const char *label, double *v, double scale, double step, const char *format) {
        double shown = *v * scale;
        const bool changed = ImGui::InputDouble(label, &shown, step, step * 10, format);
        if (changed) *v = shown / scale;
        return changed;
    }

    // Copies data into a converted buffer for plotting
    const std::vector<float> &scaled(const std::vector<float> &in, double scale, double offset, std::vector<float> &out) {
        out.resize(in.size());
        for (size_t i = 0; i < in.size(); ++i) {
            out[i] = std::isnan(in[i]) ? NaN : static_cast<float>(in[i] * scale + offset);
        }

        return out;
    }

    bool sdlEventHook(const SDL_Event &event, void *user) {
        return static_cast<AppUi *>(user)->processEvent(event);
    }

    void presentHook(void *user) {
        static_cast<AppUi *>(user)->render();
    }

} /* namespace */

AppUi::AppUi() {
    m_app = nullptr;
    m_window = nullptr;
    m_initialized = false;
    m_preferencesDirty = false;

    m_font = nullptr;
    m_monoFont = nullptr;
    m_appliedScale = 0.0f;
    m_appliedDpi = 0.0f;

    m_layout = Layout::Full;
    m_leftWidth = 290.0f;
    m_rightWidth = 380.0f;
    m_bottomHeight = 300.0f;

    m_showConsole = false;
    m_showSettings = false;
    m_showDesigner = false;
    m_showLibrary = false;
    m_savedDesignsValid = false;
    m_saveAsName[0] = '\0';
    m_showTuning = false;
    m_showHelp = false;
    m_showAbout = false;

    m_lastMessageTime = -100.0;
    m_scrollLogToBottom = false;
    m_scriptListsValid = false;

    m_engine = nullptr;
    m_simulator = nullptr;
    m_plotCylinder = 0;
    m_sampledCylinder = -1;
    m_lastAngleBin = -1;
    m_angleCycleLength = 4 * constants::pi;

    m_frameHistoryIndex = 0;
    m_clock = 0.0;

    m_audioIndex = 0;
    m_audioDecimation = 0;

    m_timePerStep = 0;
    m_firingFraction = 0;
    m_audioLatency = 0;
    m_inputBufferUsage = 0;

    m_designStatusError = false;

    m_angle.resize(AngleBins);
    m_cylinderPressure.resize(AngleBins);
    m_intakeLift.resize(AngleBins);
    m_exhaustLift.resize(AngleBins);
    m_cylinderVolume.resize(AngleBins);
    m_cylinderTemperature.resize(AngleBins);
    m_intakeFlow.resize(AngleBins);
    m_exhaustFlow.resize(AngleBins);
    m_totalExhaustFlow.resize(AngleBins);

    m_frameTime.assign(FrameHistory, NaN);
    m_rpmHistory.assign(FrameHistory, 0.0f);
    m_torqueHistory.assign(FrameHistory, 0.0f);
    m_mapHistory.assign(FrameHistory, 0.0f);

    m_audio.assign(AudioHistory, 0.0f);

    m_dynoTorque.assign(DynoBins, 0.0f);
    m_dynoPower.assign(DynoBins, 0.0f);
    m_dynoValid.assign(DynoBins, 0);

    resetAngleData();
}

AppUi::~AppUi() {
    destroy();
}

bool AppUi::initialize(EngineSimApplication *app, SDL_Window *window) {
    m_app = app;
    m_window = window;

    m_preferencesPath = (fs::path(app->getUserDataPath()) / "settings.ini").string();
    m_iniPath = (fs::path(app->getUserDataPath()) / "layout.ini").string();
    m_preferences.load(m_preferencesPath);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();

    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = m_iniPath.c_str();
    io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    if (!ImGui_ImplSDL2_InitForOpenGL(window, SDL_GL_GetCurrentContext())) {
        return false;
    }

    if (!ImGui_ImplOpenGL3_Init("#version 410")) {
        ImGui_ImplSDL2_Shutdown();
        return false;
    }

    ysSdlWindowSystem::SetEventHook(sdlEventHook, this);
    ysOpenGLSdlContext::SetPresentHook(presentHook, this);

    m_initialized = true;

    applyStyle();
    rebuildFonts();

    return true;
}

void AppUi::destroy() {
    if (!m_initialized) return;

    ysSdlWindowSystem::SetEventHook(nullptr, nullptr);
    ysOpenGLSdlContext::SetPresentHook(nullptr, nullptr);

    savePreferences();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    m_initialized = false;
}

void AppUi::savePreferences() {
    if (!m_preferencesPath.empty()) {
        m_preferences.save(m_preferencesPath);
    }

    m_preferencesDirty = false;
}

bool AppUi::processEvent(const SDL_Event &event) {
    if (!m_initialized) return false;

    ImGui_ImplSDL2_ProcessEvent(&event);

    // Ctrl+, / Ctrl+L / Ctrl+D (Cmd on macOS) toggle the settings, the
    // engine library and the designer wherever the focus is. The keys are
    // kept from the engine's own shortcuts (D toggles the dyno).
    if (event.type == SDL_KEYDOWN && (event.key.keysym.mod & (KMOD_CTRL | KMOD_GUI)) != 0) {
        bool *window = nullptr;
        switch (event.key.keysym.sym) {
        case SDLK_COMMA: window = &m_showSettings; break;
        case SDLK_l: window = &m_showLibrary; break;
        case SDLK_d: window = &m_showDesigner; break;
        default: break;
        }

        if (window != nullptr) {
            if (!event.key.repeat) *window = !*window;
            return true;
        }
    }

    const ImGuiIO &io = ImGui::GetIO();
    switch (event.type) {
    case SDL_MOUSEMOTION:
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
    case SDL_MOUSEWHEEL:
        return io.WantCaptureMouse;
    case SDL_KEYDOWN:
    case SDL_KEYUP:
    case SDL_TEXTINPUT:
        return io.WantCaptureKeyboard;
    default:
        return false;
    }
}

bool AppUi::wantsKeyboard() const {
    return m_initialized && ImGui::GetIO().WantCaptureKeyboard;
}

void AppUi::cycleLayout() {
    switch (m_layout) {
    case Layout::Full: m_layout = Layout::EngineOnly; break;
    case Layout::EngineOnly: m_layout = Layout::Instruments; break;
    case Layout::Instruments:
    default: m_layout = Layout::Full; break;
    }
}

void AppUi::log(const std::string &message) {
    m_lastMessage = message;
    m_lastMessageTime = now();

    m_log.push_back({ m_lastMessageTime, message });
    while (m_log.size() > 500) m_log.pop_front();
    m_scrollLogToBottom = true;
}

void AppUi::onEngineLoaded(Engine *engine, Simulator *simulator) {
    m_engine = engine;
    m_simulator = simulator;
    m_plotCylinder = 0;
    m_sampledCylinder = -1;
    m_metrics = EngineMetrics();
    m_shownMetrics = EngineMetrics();
    m_firingFraction = 0;
    resetAngleData();
    resetDynoData();

    std::fill(m_rpmHistory.begin(), m_rpmHistory.end(), 0.0f);
    std::fill(m_torqueHistory.begin(), m_torqueHistory.end(), 0.0f);
    std::fill(m_mapHistory.begin(), m_mapHistory.end(), 0.0f);

    // Remember the script's values so that tuning can be undone
    m_tuningDefaults = TuningDefaults();
    if (engine != nullptr) {
        IgnitionModule *ignition = engine->getIgnitionModule();
        m_tuningDefaults.timingOffset = ignition->getTimingOffset();
        m_tuningDefaults.revLimit = ignition->getRevLimit();
        m_tuningDefaults.lambda = (engine->getIntakeCount() > 0) ? engine->getIntake(0)->getLambda() : 1.0;
        m_tuningDefaults.frictionMultiplier = engine->getFrictionMultiplier();
        m_tuningDefaults.wallTemperature = engine->getWallTemperature();
        m_tuningDefaults.heatTransferMultiplier = engine->getHeatTransferMultiplier();
        m_tuningDefaults.combustionEfficiency = engine->getFuel()->getCombustionEfficiency();
        m_tuningDefaults.simulationFrequency = static_cast<int>(engine->getSimulationFrequency());

        const Synthesizer::AudioParameters audio = simulator->synthesizer().getAudioParameters();
        m_tuningDefaults.volume = audio.volume;
        m_tuningDefaults.convolution = audio.convolution;
        m_tuningDefaults.highFrequencyGain = audio.dF_F_mix;
        m_tuningDefaults.airNoise = audio.airNoise;
        m_tuningDefaults.inputNoise = audio.inputSampleNoise;

        if (const ForcedInduction *fi = engine->getForcedInduction()) {
            m_tuningDefaults.boostTarget = fi->getBoostTarget();
            m_tuningDefaults.driveRatio = fi->getDriveRatio();
            m_tuningDefaults.intercoolerEffectiveness = fi->getIntercoolerEffectiveness();
            m_tuningDefaults.blowoffValve = fi->hasBlowoffValve();
        }
    }

    // Generated engines can be opened in the designer again
    openDesignFromScript(m_app->getEngineScriptPath());

    if (m_app->hasScriptErrors()) {
        m_showConsole = true;
    }

    m_scriptListsValid = false;

    // The classic style follows the theme script's colors
    m_appliedTheme.clear();
}

void AppUi::resetAngleData() {
    const double cycleLength = m_angleCycleLength;
    for (int i = 0; i < AngleBins; ++i) {
        m_angle[i] = static_cast<float>(
            ((i + 0.5) / AngleBins - 0.5) * cycleLength * (180.0 / constants::pi));
    }

    std::fill(m_cylinderPressure.begin(), m_cylinderPressure.end(), NaN);
    std::fill(m_intakeLift.begin(), m_intakeLift.end(), NaN);
    std::fill(m_exhaustLift.begin(), m_exhaustLift.end(), NaN);
    std::fill(m_cylinderVolume.begin(), m_cylinderVolume.end(), NaN);
    std::fill(m_cylinderTemperature.begin(), m_cylinderTemperature.end(), NaN);
    std::fill(m_intakeFlow.begin(), m_intakeFlow.end(), NaN);
    std::fill(m_exhaustFlow.begin(), m_exhaustFlow.end(), NaN);
    std::fill(m_totalExhaustFlow.begin(), m_totalExhaustFlow.end(), NaN);
    m_lastAngleBin = -1;
}

void AppUi::resetDynoData() {
    std::fill(m_dynoTorque.begin(), m_dynoTorque.end(), 0.0f);
    std::fill(m_dynoPower.begin(), m_dynoPower.end(), 0.0f);
    std::fill(m_dynoValid.begin(), m_dynoValid.end(), 0);
}

void AppUi::sampleSimulationStep() {
    if (m_engine == nullptr || m_simulator == nullptr) return;

    const int cylinders = m_engine->getCylinderCount();
    if (cylinders <= 0) return;

    const int cylinder = std::clamp(m_plotCylinder, 0, cylinders - 1);
    Crankshaft *crankshaft = m_engine->getOutputCrankshaft();
    const double cycleLength = crankshaft->getCycleLength();
    if (cylinder != m_sampledCylinder || cycleLength != m_angleCycleLength) {
        m_sampledCylinder = cylinder;
        m_angleCycleLength = cycleLength;
        resetAngleData();
    }

    CombustionChamber *chamber = m_engine->getChamber(cylinder);
    const CombustionChamber::CycleMetrics &cycle = chamber->getLastCycleMetrics();

    // Crank angle after the cylinder's firing TDC
    double angle = std::fmod(crankshaft->getCycleAngle() - (cycle.valid ? cycle.firingTdcAngle : 0.0), cycleLength);
    if (angle >= cycleLength / 2) angle -= cycleLength;
    else if (angle < -cycleLength / 2) angle += cycleLength;
    if (!m_engine->isSpinningCw()) angle = -angle;
    if (!std::isfinite(angle)) return;

    const int bin = std::clamp(static_cast<int>((angle / cycleLength + 0.5) * AngleBins), 0, AngleBins - 1);

    const double dt = m_simulator->getTimestep();
    const float values[] = {
        static_cast<float>(chamber->m_system.pressure() + chamber->m_system.dynamicPressure(-1.0, 0.0)),
        static_cast<float>(chamber->getIntakeOpening()),
        static_cast<float>(chamber->getExhaustOpening()),
        static_cast<float>(chamber->getVolume()),
        static_cast<float>(chamber->m_system.temperature()),
        static_cast<float>(units::convert(chamber->getLastTimestepIntakeFlow() / dt, units::scfm)),
        static_cast<float>(units::convert(chamber->getLastTimestepExhaustFlow() / dt, units::scfm)),
        static_cast<float>(units::convert(m_simulator->getTotalExhaustFlow() / dt, units::scfm))
    };

    std::vector<float> *series[] = {
        &m_cylinderPressure, &m_intakeLift, &m_exhaustLift, &m_cylinderVolume,
        &m_cylinderTemperature, &m_intakeFlow, &m_exhaustFlow, &m_totalExhaustFlow
    };

    // Fill the bins skipped since the last step (fast engines move several
    // bins per step) by linear interpolation
    int gap = (m_lastAngleBin < 0) ? 0 : (bin - m_lastAngleBin + AngleBins) % AngleBins;
    if (gap > AngleBins / 8) gap = 0;

    for (int s = 0; s < 8; ++s) {
        std::vector<float> &data = *series[s];
        const float v1 = std::isfinite(values[s]) ? values[s] : NaN;
        const float v0 = (gap > 0) ? data[m_lastAngleBin] : v1;
        for (int k = 1; k < gap; ++k) {
            const float t = static_cast<float>(k) / gap;
            data[(m_lastAngleBin + k) % AngleBins] = (std::isnan(v0) ? v1 : v0 + (v1 - v0) * t);
        }

        data[bin] = v1;
    }

    m_lastAngleBin = bin;
}

void AppUi::addAudioSample(float sample) {
    if (++m_audioDecimation < 4) return;
    m_audioDecimation = 0;

    m_audio[m_audioIndex] = sample;
    m_audioIndex = (m_audioIndex + 1) % AudioHistory;
}

void AppUi::addFrameProfile(const FrameProfile &p) {
    const double a = 0.05;
    m_profile.ui += (p.ui - m_profile.ui) * a;
    m_profile.simulation += (p.simulation - m_profile.simulation) * a;
    m_profile.scene += (p.scene - m_profile.scene) * a;
    m_profile.present += (p.present - m_profile.present) * a;
    m_profile.total += (p.total - m_profile.total) * a;

    if (p.total > m_profilePeakRunning.total) m_profilePeakRunning = p;
    if (m_clock - m_profilePeakTime > 1.0) {
        m_profilePeak = m_profilePeakRunning;
        m_profilePeakRunning = FrameProfile();
        m_profilePeakTime = m_clock;
    }
}

void AppUi::addFrameTiming(double timePerStep, double audioLatency, double inputBufferUsage) {
    m_timePerStep = timePerStep;
    m_audioLatency = audioLatency;
    m_inputBufferUsage = inputBufferUsage;
}

void AppUi::collectFrameData() {
    const float dt = ImGui::GetIO().DeltaTime;
    m_clock += dt;

    if (m_engine == nullptr || m_simulator == nullptr) {
        m_metrics = m_shownMetrics = EngineMetrics();
        return;
    }

    const double brakeTorque = m_simulator->m_dyno.m_enabled
        ? m_simulator->getFilteredDynoTorque()
        : 0.0;
    m_metrics = calculateEngineMetrics(*m_engine, brakeTorque);
    averageMetrics(dt);

    const double rpm = m_engine->getRpm();
    m_frameTime[m_frameHistoryIndex] = static_cast<float>(m_clock);
    m_rpmHistory[m_frameHistoryIndex] = static_cast<float>(rpm);
    m_torqueHistory[m_frameHistoryIndex] = static_cast<float>(toTorque(brakeTorque));
    m_mapHistory[m_frameHistoryIndex] = static_cast<float>(units::convert(m_engine->getManifoldPressure(), units::kPa));
    m_frameHistoryIndex = (m_frameHistoryIndex + 1) % FrameHistory;

    // Dyno curve
    if (m_simulator->m_dyno.m_enabled && brakeTorque > 0 && rpm > 100) {
        const int bin = static_cast<int>(rpm / 100.0);
        if (bin >= 0 && bin < DynoBins) {
            const float torque = static_cast<float>(toTorque(brakeTorque));
            const float power = static_cast<float>(toPower(brakeTorque * m_engine->getSpeed()));
            if (m_dynoValid[bin]) {
                m_dynoTorque[bin] += (torque - m_dynoTorque[bin]) * 0.2f;
                m_dynoPower[bin] += (power - m_dynoPower[bin]) * 0.2f;
            }
            else {
                m_dynoTorque[bin] = torque;
                m_dynoPower[bin] = power;
                m_dynoValid[bin] = 1;
            }
        }
    }
}

void AppUi::averageMetrics(double dt) {
    // The metrics change with every cycle of every cylinder, and the rev
    // limiter alternates between full combustion and none, so the readouts
    // show a short running average instead
    const EngineMetrics &m = m_metrics;
    EngineMetrics &shown = m_shownMetrics;

    if (!m.valid) {
        shown = m;
        m_firingFraction = 0;
        return;
    }

    const double firing = (m.cylinders > 0) ? double(m.firingCylinders) / m.cylinders : 0.0;
    if (!shown.valid) {
        shown = m;
        m_firingFraction = firing;
        return;
    }

    const double s = 1.0 - std::exp(-std::max(dt, 0.0) / 0.5);
    auto blend = [s](double &a, double b) { a += (b - a) * s; };

    double EngineMetrics::*always[] = {
        &EngineMetrics::speed, &EngineMetrics::imepGross, &EngineMetrics::imepNet,
        &EngineMetrics::pmep, &EngineMetrics::bmep, &EngineMetrics::fmep,
        &EngineMetrics::fmepModel, &EngineMetrics::indicatedTorque, &EngineMetrics::brakeTorque,
        &EngineMetrics::indicatedPower, &EngineMetrics::brakePower,
        &EngineMetrics::mechanicalEfficiency, &EngineMetrics::indicatedEfficiency,
        &EngineMetrics::brakeEfficiency, &EngineMetrics::fuelFlow, &EngineMetrics::airFlow,
        &EngineMetrics::isfc, &EngineMetrics::bsfc, &EngineMetrics::volumetricEfficiency,
        &EngineMetrics::afr, &EngineMetrics::lambda, &EngineMetrics::peakPressure,
        &EngineMetrics::peakTemperature, &EngineMetrics::heatLossFraction
    };
    for (double EngineMetrics::*field : always) blend(shown.*field, m.*field);

    // Burn angles only exist for cycles that burned
    if (m.firingCylinders > 0) {
        double EngineMetrics::*combustion[] = {
            &EngineMetrics::sparkAdvance, &EngineMetrics::ca10, &EngineMetrics::ca50,
            &EngineMetrics::ca90, &EngineMetrics::peakPressureAngle
        };
        for (double EngineMetrics::*field : combustion) {
            if (shown.firingCylinders > 0) blend(shown.*field, m.*field);
            else shown.*field = m.*field;
        }
        shown.firingCylinders = m.firingCylinders;
    }

    shown.cylinders = m.cylinders;
    blend(m_firingFraction, firing);
}

// Units ---------------------------------------------------------------------

std::string AppUi::powerUnits() const {
    const std::string &u = m_preferences.powerUnits.empty()
        ? m_app->getAppSettings()->powerUnits
        : m_preferences.powerUnits;
    return (u == "kW") ? "kW" : "hp";
}

std::string AppUi::torqueUnits() const {
    const std::string &u = m_preferences.torqueUnits.empty()
        ? m_app->getAppSettings()->torqueUnits
        : m_preferences.torqueUnits;
    return (u == "Nm") ? "Nm" : "lb-ft";
}

std::string AppUi::speedUnits() const {
    const std::string &u = m_preferences.speedUnits.empty()
        ? m_app->getAppSettings()->speedUnits
        : m_preferences.speedUnits;
    return (u == "mph") ? "mph" : "km/h";
}

std::string AppUi::pressureUnits() const {
    const std::string &u = m_preferences.pressureUnits.empty()
        ? m_app->getAppSettings()->pressureUnits
        : m_preferences.pressureUnits;
    if (u == "kPa" || u == "mbar" || u == "bar" || u == "psi") return u;
    return "inHg";
}

double AppUi::toPower(double watts) const {
    return (powerUnits() == "kW") ? units::convert(watts, units::kW) : units::convert(watts, units::hp);
}

double AppUi::toTorque(double newtonMeters) const {
    return (torqueUnits() == "Nm") ? units::convert(newtonMeters, units::Nm) : units::convert(newtonMeters, units::ft_lb);
}

double AppUi::toSpeed(double metersPerSecond) const {
    return (speedUnits() == "mph")
        ? units::convert(metersPerSecond, units::mile / units::hour)
        : units::convert(metersPerSecond, units::km / units::hour);
}

double AppUi::toPressure(double pressure) const {
    const std::string u = pressureUnits();
    if (u == "kPa") return units::convert(pressure, units::kPa);
    if (u == "mbar") return units::convert(pressure, units::mbar);
    if (u == "bar") return units::convert(pressure, units::bar);
    if (u == "psi") return units::convert(pressure, units::psi);
    return units::convert(pressure, units::inHg);
}

double AppUi::toTemperature(double kelvin) const {
    if (m_preferences.temperatureUnits == "K") return kelvin;
    if (m_preferences.temperatureUnits == "F") return (kelvin - 273.15) * 9.0 / 5.0 + 32.0;
    return kelvin - 273.15;
}

const char *AppUi::temperatureUnits() const {
    if (m_preferences.temperatureUnits == "K") return "K";
    if (m_preferences.temperatureUnits == "F") return "F";
    return "C";
}

// Style ---------------------------------------------------------------------

void AppUi::applyStyle() {
    ImGuiStyle style;
    const std::string &theme = m_preferences.uiTheme;
    const ImVec4 accent(0.96f, 0.50f, 0.16f, 1.0f);

    if (theme == "light") {
        ImGui::StyleColorsLight(&style);
        ImVec4 *c = style.Colors;
        c[ImGuiCol_WindowBg] = ImVec4(0.96f, 0.96f, 0.97f, 1.0f);
        c[ImGuiCol_Button] = ImVec4(0.86f, 0.87f, 0.89f, 1.0f);
        c[ImGuiCol_ButtonHovered] = ImVec4(0.78f, 0.80f, 0.83f, 1.0f);
        c[ImGuiCol_Header] = ImVec4(0.88f, 0.89f, 0.91f, 1.0f);
        c[ImGuiCol_HeaderHovered] = ImVec4(0.80f, 0.82f, 0.85f, 1.0f);
        c[ImGuiCol_FrameBg] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        c[ImGuiCol_FrameBgHovered] = ImVec4(0.93f, 0.94f, 0.96f, 1.0f);
        c[ImGuiCol_FrameBgActive] = ImVec4(0.88f, 0.90f, 0.93f, 1.0f);
        c[ImGuiCol_Tab] = ImVec4(0.88f, 0.89f, 0.91f, 1.0f);
        c[ImGuiCol_TabHovered] = ImVec4(0.80f, 0.82f, 0.85f, 1.0f);
        c[ImGuiCol_TabSelected] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        c[ImGuiCol_Border] = ImVec4(0.78f, 0.79f, 0.82f, 1.0f);
        applyAccent(style, accent);
        ImPlot::StyleColorsLight();
    }
    else if (theme == "classic") {
        // Colors of the engine's theme script
        const ApplicationSettings &s = *m_app->getAppSettings();
        ImGui::StyleColorsDark(&style);
        ImVec4 *c = style.Colors;
        const ImVec4 bg = rgb(s.colorBackground);
        const ImVec4 fg = rgb(s.colorForeground);
        const ImVec4 hi = rgb(s.colorHighlight1);
        c[ImGuiCol_WindowBg] = ImVec4(bg.x, bg.y, bg.z, 1.0f);
        c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_PopupBg] = mix(bg, fg, 0.04f);
        c[ImGuiCol_MenuBarBg] = mix(bg, fg, 0.06f);
        c[ImGuiCol_Text] = fg;
        c[ImGuiCol_TextDisabled] = mix(fg, bg, 0.45f);
        c[ImGuiCol_Border] = mix(bg, fg, 0.35f);
        c[ImGuiCol_FrameBg] = mix(bg, fg, 0.08f);
        c[ImGuiCol_FrameBgHovered] = mix(bg, fg, 0.14f);
        c[ImGuiCol_FrameBgActive] = mix(bg, fg, 0.20f);
        c[ImGuiCol_TitleBg] = mix(bg, fg, 0.05f);
        c[ImGuiCol_TitleBgActive] = mix(bg, fg, 0.10f);
        c[ImGuiCol_Button] = mix(bg, fg, 0.12f);
        c[ImGuiCol_ButtonHovered] = mix(bg, fg, 0.20f);
        c[ImGuiCol_Header] = mix(bg, fg, 0.10f);
        c[ImGuiCol_HeaderHovered] = mix(bg, fg, 0.18f);
        c[ImGuiCol_Tab] = mix(bg, fg, 0.08f);
        c[ImGuiCol_TabHovered] = mix(bg, fg, 0.20f);
        c[ImGuiCol_TabSelected] = mix(bg, fg, 0.16f);
        c[ImGuiCol_TableHeaderBg] = mix(bg, fg, 0.08f);
        c[ImGuiCol_TableRowBgAlt] = ImVec4(fg.x, fg.y, fg.z, 0.03f);
        applyAccent(style, rgb(s.colorOrange));
        c[ImGuiCol_PlotLines] = hi;
        ImPlot::StyleColorsDark();
        ImPlot::GetStyle().Colors[ImPlotCol_FrameBg] = ImVec4(0, 0, 0, 0);
        ImPlot::GetStyle().Colors[ImPlotCol_PlotBg] = mix(bg, fg, 0.03f);
    }
    else {
        ImGui::StyleColorsDark(&style);
        ImVec4 *c = style.Colors;
        c[ImGuiCol_WindowBg] = ImVec4(0.075f, 0.082f, 0.094f, 1.0f);
        c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_PopupBg] = ImVec4(0.09f, 0.10f, 0.11f, 1.0f);
        c[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.11f, 0.12f, 1.0f);
        c[ImGuiCol_Border] = ImVec4(0.22f, 0.24f, 0.27f, 0.8f);
        c[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.16f, 0.18f, 1.0f);
        c[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.21f, 0.24f, 1.0f);
        c[ImGuiCol_FrameBgActive] = ImVec4(0.24f, 0.25f, 0.28f, 1.0f);
        c[ImGuiCol_TitleBg] = ImVec4(0.09f, 0.10f, 0.11f, 1.0f);
        c[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.13f, 0.15f, 1.0f);
        c[ImGuiCol_Button] = ImVec4(0.19f, 0.20f, 0.23f, 1.0f);
        c[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.27f, 0.31f, 1.0f);
        c[ImGuiCol_Header] = ImVec4(0.17f, 0.18f, 0.21f, 1.0f);
        c[ImGuiCol_HeaderHovered] = ImVec4(0.24f, 0.25f, 0.29f, 1.0f);
        c[ImGuiCol_Tab] = ImVec4(0.13f, 0.14f, 0.16f, 1.0f);
        c[ImGuiCol_TabHovered] = ImVec4(0.26f, 0.27f, 0.31f, 1.0f);
        c[ImGuiCol_TabSelected] = ImVec4(0.20f, 0.21f, 0.24f, 1.0f);
        c[ImGuiCol_TableHeaderBg] = ImVec4(0.13f, 0.14f, 0.16f, 1.0f);
        c[ImGuiCol_Separator] = ImVec4(0.22f, 0.24f, 0.27f, 1.0f);
        applyAccent(style, accent);
        ImPlot::StyleColorsDark();
        ImPlot::GetStyle().Colors[ImPlotCol_FrameBg] = ImVec4(0, 0, 0, 0);
        ImPlot::GetStyle().Colors[ImPlotCol_PlotBg] = ImVec4(0.05f, 0.055f, 0.065f, 1.0f);
    }

    style.WindowRounding = 0.0f;
    style.ChildRounding = 4.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 4.0f;
    style.ScrollbarRounding = 6.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = (theme == "light") ? 1.0f : 0.0f;
    style.WindowPadding = ImVec2(10, 10);
    style.FramePadding = ImVec2(8, 4);
    style.ItemSpacing = ImVec2(8, 6);
    style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
    style.SeparatorTextBorderSize = 1.0f;
    style.ScaleAllSizes(m_preferences.uiScale);

    ImGui::GetStyle() = style;

    ImPlotStyle &plot = ImPlot::GetStyle();
    plot.LineWeight = 1.5f;
    plot.PlotPadding = ImVec2(6, 6);
    plot.LegendPadding = ImVec2(6, 6);

    m_appliedTheme = theme;
}

void AppUi::rebuildFonts() {
    ImGuiIO &io = ImGui::GetIO();

    // Fonts are rasterized at the framebuffer's resolution (e.g. 2x on Retina
    // displays) and drawn at their logical size
    int w = 1, h = 1, pw = 1, ph = 1;
    SDL_GetWindowSize(m_window, &w, &h);
    SDL_GL_GetDrawableSize(m_window, &pw, &ph);
    const float dpi = std::clamp((w > 0) ? (float)pw / w : 1.0f, 1.0f, 4.0f);
    const float scale = std::clamp(m_preferences.uiScale, 0.5f, 3.0f);

    io.Fonts->Clear();

    const std::string fontPath = (fs::path(m_app->getDataRoot()) / "assets" / "fonts" / "Roboto-Medium.ttf").string();
    ImFontConfig config;
    config.OversampleH = 2;
    config.OversampleV = 1;
    std::error_code ec;
    m_font = fs::is_regular_file(fontPath, ec)
        ? io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 15.0f * scale * dpi, &config)
        : nullptr;

    ImFontConfig monoConfig;
    monoConfig.SizePixels = std::round(13.0f * scale * dpi);
    m_monoFont = io.Fonts->AddFontDefault(&monoConfig);
    if (m_font == nullptr) m_font = m_monoFont;

    io.FontDefault = m_font;
    io.FontGlobalScale = 1.0f / dpi;

    ImGui_ImplOpenGL3_DestroyFontsTexture();

    m_appliedScale = m_preferences.uiScale;
    m_appliedDpi = dpi;
}

// Frame ---------------------------------------------------------------------

void AppUi::build() {
    if (!m_initialized) return;

    // Style and font changes have to be made between frames
    {
        int w = 1, h = 1, pw = 1, ph = 1;
        SDL_GetWindowSize(m_window, &w, &h);
        SDL_GL_GetDrawableSize(m_window, &pw, &ph);
        const float dpi = std::clamp((w > 0) ? (float)pw / w : 1.0f, 1.0f, 4.0f);

        if (m_appliedScale != m_preferences.uiScale || m_appliedDpi != dpi) {
            applyStyle();
            rebuildFonts();
        }
        else if (m_appliedTheme != m_preferences.uiTheme) {
            applyStyle();
        }
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    collectFrameData();

    const float s = m_preferences.uiScale;
    buildMenuBar();

    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    const float statusHeight = ImGui::GetFrameHeight() + 6.0f * s;
    ImVec2 pos = viewport->WorkPos;
    ImVec2 size = ImVec2(viewport->WorkSize.x, viewport->WorkSize.y - statusHeight);

    buildStatusBar();

    const bool showControls = (m_layout == Layout::Full);
    const bool showPlots = (m_layout == Layout::Full);
    const bool showInstruments = (m_layout != Layout::EngineOnly);

    // Panels around the engine view; their sizes can be changed by dragging
    // their inner edges
    const ImGuiWindowFlags panelFlags =
        ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoBringToFrontOnFocus;

    float left = 0, right = 0, bottom = 0;
    if (showControls) {
        m_leftWidth = std::clamp(m_leftWidth, 200.0f, size.x * 0.35f / s);
        left = m_leftWidth * s;
    }

    if (showInstruments) {
        m_rightWidth = std::clamp(m_rightWidth, 260.0f, size.x * 0.40f / s);
        right = m_rightWidth * s;
    }

    if (showPlots) {
        m_bottomHeight = std::clamp(m_bottomHeight, 150.0f, size.y * 0.6f / s);
        bottom = m_bottomHeight * s;
    }

    if (showControls) {
        ImGui::SetNextWindowPos(pos);
        ImGui::SetNextWindowSize(ImVec2(left, size.y));
        ImGui::SetNextWindowSizeConstraints(ImVec2(200 * s, size.y), ImVec2(size.x * 0.35f, size.y));
        if (ImGui::Begin("Controls", nullptr, panelFlags)) {
            buildControlPanel();
        }
        m_leftWidth = ImGui::GetWindowWidth() / s;
        ImGui::End();
    }

    if (showInstruments) {
        ImGui::SetNextWindowPos(ImVec2(pos.x + size.x - right, pos.y));
        ImGui::SetNextWindowSize(ImVec2(right, size.y));
        ImGui::SetNextWindowSizeConstraints(ImVec2(260 * s, size.y), ImVec2(size.x * 0.40f, size.y));
        if (ImGui::Begin("Instruments", nullptr, panelFlags)) {
            buildInstrumentPanel();
        }
        const float width = ImGui::GetWindowWidth();
        if (std::abs(width - right) > 0.5f) {
            // Dragged on the left edge: keep the right edge in place
            m_rightWidth = width / s;
        }
        ImGui::End();
    }

    if (showPlots) {
        const float plotWidth = size.x - left - right;
        ImGui::SetNextWindowPos(ImVec2(pos.x + left, pos.y + size.y - bottom));
        ImGui::SetNextWindowSize(ImVec2(plotWidth, bottom));
        ImGui::SetNextWindowSizeConstraints(ImVec2(plotWidth, 150 * s), ImVec2(plotWidth, size.y * 0.6f));
        if (ImGui::Begin("Analysis", nullptr, panelFlags)) {
            buildPlotPanel();
        }
        m_bottomHeight = ImGui::GetWindowHeight() / s;
        ImGui::End();
    }

    // The engine view fills the rest
    const ImVec2 scale = ImGui::GetIO().DisplayFramebufferScale;
    m_engineViewRect.x = (pos.x + left) * scale.x;
    m_engineViewRect.y = pos.y * scale.y;
    m_engineViewRect.width = std::max(16.0f, (size.x - left - right) * scale.x);
    m_engineViewRect.height = std::max(16.0f, (size.y - bottom) * scale.y);

    buildEngineViewOverlay();

    if (m_showConsole) buildConsoleWindow();
    if (m_showSettings) buildSettingsWindow();
    if (m_showDesigner) buildDesignerWindow();
    if (m_showLibrary) buildLibraryWindow();
    if (m_showTuning) buildTuningWindow();
    if (m_showHelp) buildHelpWindow();
    if (m_showAbout) buildAboutWindow();

    ImGui::Render();

    if (m_preferencesDirty) {
        savePreferences();
    }
}

void AppUi::render() {
    if (!m_initialized) return;

    ImDrawData *drawData = ImGui::GetDrawData();
    if (drawData != nullptr) {
        ImGui_ImplOpenGL3_RenderDrawData(drawData);
    }
}

void AppUi::refreshScriptLists() {
    if (m_scriptListsValid) return;

    m_engineScripts = m_app->findEngineScripts();
    m_themeScripts = m_app->findThemeScripts();
    m_scriptListsValid = true;
}

void AppUi::buildEngineMenu() {
    refreshScriptLists();

    // Group the scripts by folder
    std::string currentGroup;
    bool groupOpen = false;
    for (const std::string &script : m_engineScripts) {
        const fs::path path(script);
        const std::string group = path.parent_path().filename().string();
        const std::string groupPath = path.parent_path().string();
        const bool selected = (script == m_app->getEngineScriptPath());

        if (group == "assets") {
            // main.mr
            if (ImGui::MenuItem("Default (main.mr)", nullptr, selected)) {
                m_controls.loadScript = script;
            }
            continue;
        }

        // Folders with the same name (bundled and user engines) are
        // separate menus; the IDs come from the full paths
        if (groupPath != currentGroup) {
            if (groupOpen) ImGui::EndMenu();
            currentGroup = groupPath;
            groupOpen = ImGui::BeginMenu((group + "###" + groupPath).c_str());
        }

        if (groupOpen) {
            ImGui::PushID(script.c_str());
            if (ImGui::MenuItem(path.stem().string().c_str(), nullptr, selected)) {
                m_controls.loadScript = script;
            }
            ImGui::PopID();
        }
    }

    if (groupOpen) ImGui::EndMenu();
}

void AppUi::buildMenuBar() {
    if (!ImGui::BeginMainMenuBar()) return;

    if (ImGui::BeginMenu("File")) {
        if (ImGui::BeginMenu("Load engine")) {
            buildEngineMenu();
            ImGui::EndMenu();
        }
        else {
            m_scriptListsValid = false;
        }

        if (ImGui::MenuItem("Previous engine", "PgUp")) m_controls.cycleEngine = -1;
        if (ImGui::MenuItem("Next engine", "PgDn")) m_controls.cycleEngine = 1;
        if (ImGui::MenuItem("Reload", "Enter")) m_controls.reloadRequested = true;
        ImGui::Separator();
        if (ImGui::MenuItem("Engine library...", LibraryShortcut)) m_showLibrary = true;
        if (ImGui::MenuItem("Engine designer...", DesignerShortcut)) m_showDesigner = true;
        ImGui::Separator();
        if (ImGui::MenuItem("Open user folder")) {
            const std::string url = "file://" + m_app->getUserDataPath();
            SDL_OpenURL(url.c_str());
        }
        if (ImGui::MenuItem("Settings...", SettingsShortcut)) m_showSettings = true;
        ImGui::Separator();
        if (ImGui::MenuItem("Quit", "Esc")) m_controls.quitRequested = true;
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
        if (ImGui::MenuItem("Full layout", nullptr, m_layout == Layout::Full)) m_layout = Layout::Full;
        if (ImGui::MenuItem("Engine and instruments", nullptr, m_layout == Layout::Instruments)) m_layout = Layout::Instruments;
        if (ImGui::MenuItem("Engine only", nullptr, m_layout == Layout::EngineOnly)) m_layout = Layout::EngineOnly;
        ImGui::TextDisabled("Tab cycles the layouts");
        ImGui::Separator();

        bool fullscreen = m_app->isFullscreen();
        if (ImGui::MenuItem("Fullscreen", "F", &fullscreen)) {
            m_app->setFullscreen(fullscreen);
            m_preferences.fullscreen = fullscreen;
            m_preferencesDirty = true;
        }

        if (ImGui::MenuItem("Engine view frame", nullptr, &m_preferences.showEngineViewFrame)) {
            m_preferencesDirty = true;
        }

        bool realistic = (m_preferences.engineViewStyle != "classic");
        if (ImGui::MenuItem("Realistic engine view", nullptr, &realistic)) {
            m_preferences.engineViewStyle = realistic ? "realistic" : "classic";
            m_preferencesDirty = true;
        }

        if (m_engine != nullptr) {
            int layer = m_app->getViewParameters().Layer0;
            ImGui::SetNextItemWidth(160 * m_preferences.uiScale);
            if (ImGui::SliderInt("Layer", &layer, 0, std::max(0, m_engine->getMaxDepth() - 1))) {
                m_app->setViewLayer(layer);
            }
        }

        if (ImGui::BeginMenu("Theme script")) {
            refreshScriptLists();
            if (ImGui::MenuItem("Engine default", nullptr, m_app->getThemeScriptPath().empty())) {
                m_controls.loadTheme = "-";
            }

            for (const std::string &theme : m_themeScripts) {
                ImGui::PushID(theme.c_str());
                if (ImGui::MenuItem(fs::path(theme).stem().string().c_str(), nullptr, theme == m_app->getThemeScriptPath())) {
                    m_controls.loadTheme = theme;
                }
                ImGui::PopID();
            }
            ImGui::EndMenu();
        }

        ImGui::Separator();
        ImGui::MenuItem("Console", "F5", &m_showConsole);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Simulation")) {
        bool paused = m_app->isPaused();
        if (ImGui::MenuItem("Pause", "P", &paused)) m_app->setPaused(paused);
        if (ImGui::MenuItem("Step one frame", "Right", false, paused)) m_controls.stepRequested = true;
        ImGui::Separator();
        if (m_simulator != nullptr && m_engine != nullptr) {
            ImGui::MenuItem("Ignition", "A", &m_engine->getIgnitionModule()->m_enabled);
            ImGui::MenuItem("Dynamometer", "D", &m_simulator->m_dyno.m_enabled);
            ImGui::MenuItem("Hold dyno speed", "H", &m_simulator->m_dyno.m_hold);
            ImGui::Separator();
        }
        if (ImGui::MenuItem("Tuning...")) m_showTuning = true;
        if (ImGui::MenuItem("Clear dyno curve")) resetDynoData();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("Controls")) m_showHelp = true;
        if (ImGui::MenuItem("Engineering documentation")) {
            const std::string doc = (fs::path(m_app->getDataRoot()) / "docs" / "engineering.md").string();
            std::error_code ec;
            const std::string url = fs::is_regular_file(doc, ec)
                ? "file://" + doc
                : "https://github.com/kyan0s-git/engine-sim/blob/master/docs/engineering.md";
            SDL_OpenURL(url.c_str());
        }
        if (ImGui::MenuItem("Project page")) SDL_OpenURL("https://github.com/kyan0s-git/engine-sim");
        ImGui::Separator();
        if (ImGui::MenuItem("Check for updates", "F8")) m_app->requestUpdateAction();
        if (ImGui::MenuItem("About")) m_showAbout = true;
        ImGui::EndMenu();
    }

    // Update notification on the right
    const Updater &updater = m_app->getUpdater();
    const Updater::State state = updater.getState();
    if (state == Updater::State::Available
        || state == Updater::State::Downloading
        || state == Updater::State::ReadyToRestart)
    {
        const std::string text = (state == Updater::State::Available)
            ? "Update " + updater.getRelease().version + " available"
            : updater.getStatusText();
        const float w = ImGui::CalcTextSize(text.c_str()).x + ImGui::GetStyle().FramePadding.x * 4;
        ImGui::SameLine(ImGui::GetWindowWidth() - w - 10);
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(ui::AccentColor()));
        if (ImGui::MenuItem(text.c_str())) m_app->requestUpdateAction();
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(state == Updater::State::ReadyToRestart
                ? "Click to restart and install"
                : "Click to download and install (F8)");
        }
    }

    ImGui::EndMainMenuBar();
}

void AppUi::buildStatusBar() {
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    const float height = ImGui::GetFrameHeight() + 6.0f * m_preferences.uiScale;

    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + viewport->WorkSize.y - height));
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, height));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 3));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::GetStyleColorVec4(ImGuiCol_MenuBarBg));
    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing;

    if (ImGui::Begin("##status", nullptr, flags)) {
        if (m_app->isPaused()) {
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(ui::WarningColor()), "PAUSED");
        }
        else if (m_engine == nullptr) {
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(ui::DangerColor()), "NO ENGINE");
        }
        else {
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(ui::OkColor()), "RUNNING");
        }

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        if (m_simulator != nullptr) {
            ImGui::Text("%.0f fps   %d Hz   %.1f us/step",
                ImGui::GetIO().Framerate,
                m_simulator->getSimulationFrequency(),
                m_timePerStep * 1E6);
        }

        if (m_engine != nullptr && m_engine->isEngineering()) {
            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(ui::AccentColor()), "ENGINEERING MODEL");
        }

        if (m_app->hasScriptErrors()) {
            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(ui::DangerColor()));
            if (ImGui::SmallButton("Script errors (F5)")) m_showConsole = true;
            ImGui::PopStyleColor();
        }

        // Recent message on the right
        const double age = now() - m_lastMessageTime;
        if (!m_lastMessage.empty() && age < 8.0) {
            const float alpha = static_cast<float>(std::clamp(8.0 - age, 0.0, 1.0));
            const float w = ImGui::CalcTextSize(m_lastMessage.c_str()).x;
            ImGui::SameLine(std::max(ImGui::GetCursorPosX() + 20, ImGui::GetWindowWidth() - w - 12));
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
            ImGui::TextUnformatted(m_lastMessage.c_str());
            ImGui::PopStyleVar();
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void AppUi::buildControlPanel() {
    const float s = m_preferences.uiScale;

    if (m_engine == nullptr || m_simulator == nullptr) {
        ImGui::TextWrapped("No engine is loaded. Pick one from the engine library or design one in the engine designer.");
        if (ImGui::Button("Engine library...", ImVec2(-FLT_MIN, 0))) m_showLibrary = true;
        if (ImGui::Button("Engine designer...", ImVec2(-FLT_MIN, 0))) m_showDesigner = true;
        return;
    }

    // Engine
    ImGui::PushFont(m_font);
    ImGui::TextWrapped("%s", m_engine->getName().c_str());
    ImGui::PopFont();
    ImGui::TextDisabled("%s, %d cyl.", formatDisplacement(m_engine->getDisplacement()).c_str(), m_engine->getCylinderCount());

    ImGui::SeparatorText("Engine");

    IgnitionModule *ignition = m_engine->getIgnitionModule();
    ImGui::Checkbox("Ignition [A]", &ignition->m_enabled);

    ImGui::Button("Hold to crank [S]", ImVec2(-FLT_MIN, 32 * s));
    m_controls.starterHeld = ImGui::IsItemActive();

    ImGui::Text("Throttle");
    ImGui::SameLine();
    ImGui::TextDisabled("(Q/W/E/R, Space+wheel)");
    float throttle = static_cast<float>(m_controls.throttle * 100.0);
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::SliderFloat("##throttle", &throttle, 0.0f, 100.0f, "%.1f %%")) {
        m_controls.throttle = throttle / 100.0;
    }

    const float third = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 3) / 4;
    const double presets[] = { 0.0, 0.1, 0.5, 1.0 };
    const char *presetNames[] = { "Idle", "10%", "50%", "WOT" };
    for (int i = 0; i < 4; ++i) {
        if (i > 0) ImGui::SameLine();
        if (ImGui::Button(presetNames[i], ImVec2(third, 0))) m_controls.throttle = presets[i];
    }

    ImGui::SeparatorText("Drivetrain");

    Transmission *transmission = m_simulator->getTransmission();
    if (transmission != nullptr) {
        const int gear = transmission->getGear();
        ImGui::Text("Gear");
        const int gears = transmission->getGearCount();
        const float buttonWidth = 30 * s;
        float x = ImGui::GetContentRegionAvail().x;
        for (int g = -1; g < gears; ++g) {
            if (g > -1) {
                x -= buttonWidth + ImGui::GetStyle().ItemSpacing.x;
                if (x > buttonWidth) ImGui::SameLine();
                else x = ImGui::GetContentRegionAvail().x;
            }

            const bool selected = (g == gear);
            if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::ColorConvertU32ToFloat4(ui::AccentColor(0.75f)));
            char label[16];
            if (g < 0) std::snprintf(label, sizeof(label), "N");
            else std::snprintf(label, sizeof(label), "%d", g + 1);
            if (ImGui::Button(label, ImVec2(buttonWidth, 0))) transmission->changeGear(g);
            if (selected) ImGui::PopStyleColor();
        }

        float clutch = static_cast<float>(m_controls.clutch * 100.0);
        ImGui::Text("Clutch engagement");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::SliderFloat("##clutch", &clutch, 0.0f, 100.0f, "%.0f %%")) {
            m_controls.clutch = clutch / 100.0;
        }
    }

    ImGui::Button("Hold to brake [.]", ImVec2(-FLT_MIN, 0));
    m_controls.brakeHeld = ImGui::IsItemActive();

    ImGui::SeparatorText("Dynamometer");

    ImGui::Checkbox("Enabled [D]", &m_simulator->m_dyno.m_enabled);
    ImGui::SameLine();
    ImGui::Checkbox("Hold speed [H]", &m_simulator->m_dyno.m_hold);

    {
        float rpm = static_cast<float>(units::toRpm(m_app->getDynoSpeed()));
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::BeginDisabled(!m_simulator->m_dyno.m_hold);
        if (ImGui::SliderFloat("##dynospeed", &rpm,
                (float)units::toRpm(m_engine->getDynoMinSpeed()),
                (float)units::toRpm(m_engine->getDynoMaxSpeed()), "%.0f rpm"))
        {
            m_app->setDynoSpeed(units::rpm(rpm));
        }
        ImGui::EndDisabled();
        if (!m_simulator->m_dyno.m_hold) {
            ImGui::TextDisabled("Without hold the dyno sweeps up to the redline.");
        }
    }

    ImGui::SeparatorText("Simulation");

    bool paused = m_app->isPaused();
    if (ImGui::Button(paused ? "Resume [P]" : "Pause [P]", ImVec2(third * 2 + ImGui::GetStyle().ItemSpacing.x, 0))) {
        m_app->setPaused(!paused);
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!paused);
    if (ImGui::Button("Step", ImVec2(-FLT_MIN, 0))) m_controls.stepRequested = true;
    ImGui::EndDisabled();

    const double speeds[] = { 1.0, 0.5, 0.1, 0.01, 0.005, 0.002, 0.001 };
    const char *speedNames[] = { "Real time", "1/2", "1/10", "1/100", "1/200", "1/500", "1/1000" };
    int speedIndex = 0;
    for (int i = 0; i < 7; ++i) {
        if (std::abs(m_controls.simulationSpeed - speeds[i]) < 1E-9) speedIndex = i;
    }

    ImGui::Text("Speed");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::Combo("##speed", &speedIndex, speedNames, 7)) {
        m_controls.simulationSpeed = speeds[speedIndex];
    }

    ImGui::Spacing();
    const float buttonWidth = (ImGui::GetContentRegionAvail().x - 2 * ImGui::GetStyle().ItemSpacing.x) / 3;
    if (ImGui::Button("Tuning...", ImVec2(buttonWidth, 0))) m_showTuning = true;
    ImGui::SameLine();
    if (ImGui::Button("Library...", ImVec2(buttonWidth, 0))) m_showLibrary = true;
    ImGui::SameLine();
    if (ImGui::Button("Designer...", ImVec2(-FLT_MIN, 0))) m_showDesigner = true;

    // Script warnings
    if (!m_engine->getWarnings().empty()) {
        ImGui::SeparatorText("Warnings");
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(ui::WarningColor()));
        for (const std::string &warning : m_engine->getWarnings()) {
            ImGui::TextWrapped("%s", warning.c_str());
        }
        ImGui::PopStyleColor();
    }
}


void AppUi::buildInstrumentPanel() {
    if (m_engine == nullptr || m_simulator == nullptr) {
        ImGui::TextDisabled("No engine");
        return;
    }

    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float gaugeSize = std::floor((ImGui::GetContentRegionAvail().x - spacing) / 2);
    const double rpm = std::max(0.0, m_engine->getRpm());
    const EngineMetrics &m = m_shownMetrics;
    const bool limiting = m_engine->getIgnitionModule()->m_enabled
        && m_engine->getIgnitionModule()->isLimiting();

    // Tachometer
    {
        const float redline = static_cast<float>(units::toRpm(m_engine->getRedline()));
        const float max = std::ceil(redline * 1.2f / 1000.0f) * 1000.0f;
        const ui::GaugeBand bands[] = {
            { redline * 0.9f, redline, ui::WarningColor(0.8f) },
            { redline, max, ui::DangerColor(0.9f) }
        };

        ui::GaugeStyle style;
        style.min = 0;
        style.max = max;
        style.majorStep = (max > 12000) ? 2000.0f : 1000.0f;
        style.minorTicks = (max > 12000) ? 3 : 1;
        style.labelScale = 0.001f;
        style.format = "%.0f";
        style.valueFormat = "%.0f";
        style.unit = "rpm";
        style.bands = bands;
        style.bandCount = 2;
        ui::RoundGauge("RPM x1000", static_cast<float>(rpm), style, gaugeSize);
    }

    ImGui::SameLine();

    // Speedometer
    {
        const bool mph = (speedUnits() == "mph");
        const Vehicle *vehicle = m_simulator->getVehicle();
        const double speed = (vehicle != nullptr) ? std::abs(vehicle->getSpeed()) : 0.0;

        ui::GaugeStyle style;
        style.min = 0;
        style.max = mph ? 160.0f : 260.0f;
        style.majorStep = 20.0f;
        style.minorTicks = 1;
        style.valueFormat = "%.0f";
        const std::string unit = speedUnits();
        style.unit = unit.c_str();
        ui::RoundGauge("SPEED", static_cast<float>(toSpeed(speed)), style, gaugeSize);
    }

    // Manifold pressure
    {
        const std::string unit = pressureUnits();
        const double map = m_engine->getManifoldPressure();

        ui::GaugeStyle style;
        style.unit = unit.c_str();
        float value = 0;
        const char *label = "MANIFOLD";
        const ForcedInduction *fi = m_engine->getForcedInduction();
        const double maxBoost = (fi != nullptr)
            ? std::max(1.6 * fi->getBoostTarget(), units::pressure(1.2, units::bar))
            : 0.0;
        if (fi != nullptr && (unit == "inHg" || unit == "psi")) {
            // Boost gauge: vacuum and boost in psi
            label = "BOOST";
            style.unit = "psi";
            value = static_cast<float>(units::convert(map - Ambient, units::psi));
            style.min = -15.0f;
            style.max = std::ceil(static_cast<float>(units::convert(maxBoost, units::psi)) / 5.0f) * 5.0f;
            style.majorStep = 5.0f;
            style.minorTicks = 4;
            style.valueFormat = "%.1f";
        }
        else if (fi != nullptr) {
            label = "MANIFOLD";
            value = static_cast<float>(toPressure(map));
            style.min = 0;
            style.max = static_cast<float>(toPressure(Ambient + maxBoost));
            style.max = (unit == "bar") ? std::ceil(style.max * 2.0f) / 2.0f : std::ceil(style.max / 50.0f) * 50.0f;
            style.majorStep = (unit == "bar") ? 0.5f : 50.0f;
            style.minorTicks = 4;
            style.format = (unit == "bar") ? "%.1f" : "%.0f";
            style.valueFormat = (unit == "bar") ? "%.2f" : "%.0f";
        }
        else if (unit == "inHg" || unit == "psi") {
            // Vacuum gauge: pressure relative to the atmosphere
            label = "VACUUM";
            value = static_cast<float>(toPressure(map - Ambient));
            style.min = (unit == "inHg") ? -30.0f : -15.0f;
            style.max = (unit == "inHg") ? 10.0f : 5.0f;
            style.majorStep = 5.0f;
            style.minorTicks = 4;
            style.valueFormat = "%.1f";
        }
        else {
            value = static_cast<float>(toPressure(map));
            style.min = 0;
            style.max = static_cast<float>(toPressure(units::pressure(120, units::kPa)));
            style.majorStep = style.max / 6;
            style.minorTicks = 3;
            style.format = (unit == "bar") ? "%.1f" : "%.0f";
            style.valueFormat = (unit == "bar") ? "%.2f" : "%.0f";
        }

        ui::RoundGauge(label, value, style, gaugeSize);
    }

    ImGui::SameLine();

    // Intake air flow
    {
        const double theoretical = 0.5 * (Ambient * m_engine->getDisplacement())
            / (constants::R * SimulationAmbientTemperature) * units::toRpm(m_engine->getRedline()) / 60.0;
        const float max = std::max(50.0f,
            std::ceil(static_cast<float>(units::convert(theoretical, units::scfm)) * 1.2f / 50.0f) * 50.0f);

        ui::GaugeStyle style;
        style.min = 0;
        style.max = max;
        style.majorStep = max / 5;
        style.minorTicks = 1;
        style.valueFormat = "%.0f";
        style.unit = "cfm";
        ui::RoundGauge("AIR FLOW", static_cast<float>(units::convert(std::max(0.0, m_engine->getIntakeFlowRate()), units::scfm)), style, gaugeSize);
    }

    // Bars
    {
        ui::BarMeter("Throttle", static_cast<float>(m_engine->getSpeedControl() * 100.0), 0, 100, "%.0f %%");

        double ve = m.volumetricEfficiency;
        if (!m.valid) {
            const double theoretical = 0.5 * (Ambient * m_engine->getDisplacement())
                / (constants::R * SimulationAmbientTemperature) * rpm / 60.0;
            ve = (theoretical > 1E-3) ? m_engine->getIntakeFlowRate() / theoretical : 0.0;
        }
        ui::BarMeter("Vol. efficiency", static_cast<float>(ve * 100.0), 0, 120, "%.0f %%");

        const double afr = (m.valid && m.afr > 0) ? m.afr : m_engine->getIntakeAfr();
        ImU32 afrColor = ui::OkColor();
        if (m.valid && m.lambda > 0) {
            if (m.lambda < 0.85 || m.lambda > 1.15) afrColor = ui::WarningColor();
        }
        char afrFormat[48];
        if (m.valid && m.lambda > 0) std::snprintf(afrFormat, sizeof(afrFormat), "%%.1f : 1  (lambda %.2f)", m.lambda);
        else std::snprintf(afrFormat, sizeof(afrFormat), "%%.1f : 1");
        ui::BarMeter("Air-fuel ratio", static_cast<float>(afr), 8, 20, afrFormat, afrColor);

        const double advance = m_engine->getIgnitionModule()->getTimingAdvance() * 180.0 / constants::pi;
        ui::BarMeter("Spark advance", static_cast<float>(advance), 0, 60, "%.1f deg");

        if (m_simulator->getTransmission() != nullptr) {
            ui::BarMeter("Clutch", static_cast<float>(m_simulator->getTransmission()->getClutchPressure() * 100.0), 0, 100, "%.0f %%");
        }
    }

    if (const ForcedInduction *fi = m_engine->getForcedInduction()) {
        if (ImGui::CollapsingHeader(fi->getDescription().c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            const bool imperial = (pressureUnits() == "inHg" || pressureUnits() == "psi");
            const double boost = fi->getBoost();
            char format[48];
            std::snprintf(format, sizeof(format), "%%.%df %s", imperial ? 1 : 2, imperial ? "psi" : "bar");
            auto toBoost = [&](double pa) {
                return static_cast<float>(imperial ? units::convert(pa, units::psi) : units::convert(pa, units::bar));
            };
            const float maxBoost = toBoost(std::max(1.6 * fi->getBoostTarget(), units::pressure(1.2, units::bar)));
            ui::BarMeter("Boost (charge air)", toBoost(boost), 0.0f, maxBoost, format,
                boost > 1.1 * fi->getBoostTarget() && fi->isTurbocharger() ? ui::WarningColor() : ui::AccentColor());

            const double krpm = fi->getShaftSpeed() * 60 / (2 * constants::pi) / 1000;
            const double maxKrpm = fi->getMaxSpeed() * 60 / (2 * constants::pi) / 1000;
            ui::BarMeter(fi->isTurbocharger() ? "Turbo speed" : "Compressor speed",
                static_cast<float>(krpm), 0.0f, static_cast<float>(std::max(maxKrpm, 1.0)), "%.0f krpm");

            if (fi->getArrangement() == ForcedInduction::Arrangement::Sequential) {
                const ForcedInduction::Unit &second = fi->getUnit(1);
                ui::BarMeter(second.active ? "Second turbo (on)" : "Second turbo (off)",
                    static_cast<float>(second.omega * 60 / (2 * constants::pi) / 1000),
                    0.0f, static_cast<float>(std::max(maxKrpm, 1.0)), "%.0f krpm");
            }

            if (fi->isTurbocharger()) {
                ui::BarMeter("Wastegate", static_cast<float>(fi->getWastegateOpening() * 100), 0, 100, "%.0f %% open");
            }

            if (ImGui::BeginTable("##forcedinduction", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
                ui::ValueRow("Charge air", "%.0f %s (compressor out %.0f)",
                    toTemperature(fi->getChargeTemperature()), temperatureUnits(),
                    toTemperature(fi->getCompressorOutletTemperature()));
                if (fi->isTurbocharger()) {
                    ui::ValueRow("Turbine power", "%.1f %s", toPower(fi->getTurbinePower()), powerUnits().c_str());
                }
                else {
                    ui::ValueRow("Drive power", "%.1f %s", toPower(fi->getDrivePower()), powerUnits().c_str());
                }
                if (fi->getBlowoffOpening() > 0.05) {
                    ui::ValueRow("Blow-off valve", "venting");
                }
                ImGui::EndTable();
            }
        }
    }

    const ImGuiTableFlags tableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp;
    const std::string tq = torqueUnits();
    const std::string pw = powerUnits();
    const bool imperialPressure = (pressureUnits() == "inHg" || pressureUnits() == "psi");
    auto mep = [&](double pa) { return imperialPressure ? units::convert(pa, units::psi) : units::convert(pa, units::bar); };
    const char *mepUnits = imperialPressure ? "psi" : "bar";

    if (ImGui::CollapsingHeader("Output", ImGuiTreeNodeFlags_DefaultOpen)
        && ImGui::BeginTable("##output", 2, tableFlags))
    {
        if (m_simulator->m_dyno.m_enabled) {
            ui::ValueRow("Brake torque", "%.1f %s", toTorque(m_simulator->getFilteredDynoTorque()), tq.c_str());
            ui::ValueRow("Brake power", "%.1f %s", toPower(m_simulator->getDynoPower()), pw.c_str());
        }
        else {
            ui::ValueRow("Brake torque", "enable the dyno");
        }

        if (m.valid) {
            ui::ValueRow("Indicated torque", "%.1f %s", toTorque(m.indicatedTorque), tq.c_str());
            ui::ValueRow("Indicated power", "%.1f %s", toPower(m.indicatedPower), pw.c_str());
            ui::ValueRow("IMEP net / gross", "%.2f / %.2f %s", mep(m.imepNet), mep(m.imepGross), mepUnits);
            ui::ValueRow("PMEP", "%.2f %s", mep(m.pmep), mepUnits);
            if (m_simulator->m_dyno.m_enabled) {
                ui::ValueRow("BMEP", "%.2f %s", mep(m.bmep), mepUnits);
                ui::ValueRow("FMEP", "%.2f %s", mep(m.fmep), mepUnits);
                ui::ValueRow("Mech. efficiency", "%.1f %%", m.mechanicalEfficiency * 100);
                ui::ValueRow("Brake efficiency", "%.1f %%", m.brakeEfficiency * 100);
                ui::ValueRow("BSFC", "%.0f g/kWh", m.bsfc * 3.6E9);
            }
            ui::ValueRow("Ind. efficiency", "%.1f %%", m.indicatedEfficiency * 100);
            ui::ValueRow("Fuel flow", "%.2f kg/h", m.fuelFlow * 3600);
            ui::ValueRow("Air flow", "%.1f kg/h", m.airFlow * 3600);
        }
        else {
            ui::ValueRow("Cycle data", "waiting for a full cycle");
        }

        if (m_engine->usesFrictionModel()) {
            ui::ValueRow("Friction model", "%.2f %s", mep(m_engine->getFrictionMeanEffectivePressure()), mepUnits);
        }

        ImGui::EndTable();
    }

    if (ImGui::CollapsingHeader("Combustion", ImGuiTreeNodeFlags_DefaultOpen)
        && ImGui::BeginTable("##combustion", 2, tableFlags))
    {
        if (limiting) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(ui::DangerColor()), "Rev limiter");
            ImGui::TableSetColumnIndex(1);
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(ui::DangerColor()), "cutting spark");
        }

        if (m.valid && m.firingCylinders > 0) {
            ui::ValueRow("Cycles firing", "%.0f %%", m_firingFraction * 100);
            ui::ValueRow("Spark", "%.1f deg BTDC", m.sparkAdvance);
            ui::ValueRow("CA10 / 50 / 90", "%.1f / %.1f / %.1f", m.ca10, m.ca50, m.ca90);
            ui::ValueRow("Peak pressure", "%.1f %s @ %.1f deg", mep(m.peakPressure), mepUnits, m.peakPressureAngle);
            ui::ValueRow("Peak temperature", "%.0f %s", toTemperature(m.peakTemperature), temperatureUnits());
            ui::ValueRow("Wall heat loss", "%.1f %% of fuel", m.heatLossFraction * 100);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextDisabled("Averaged over the last half second");
        }
        else {
            ui::ValueRow("Combustion", m.valid ? "no cylinder fired" : "no complete cycle yet");
        }

        ImGui::EndTable();
    }

    if (ImGui::CollapsingHeader("Vehicle")
        && ImGui::BeginTable("##vehicle", 2, tableFlags))
    {
        Transmission *transmission = m_simulator->getTransmission();
        Vehicle *vehicle = m_simulator->getVehicle();
        if (transmission != nullptr) {
            const int gear = transmission->getGear();
            if (gear < 0) ui::ValueRow("Gear", "neutral");
            else ui::ValueRow("Gear", "%d (%.2f)", gear + 1, transmission->getGearRatio(gear));
        }

        if (vehicle != nullptr) {
            ui::ValueRow("Speed", "%.1f %s", toSpeed(std::abs(vehicle->getSpeed())), speedUnits().c_str());
            ui::ValueRow("Distance", "%.2f km", vehicle->getTravelledDistance() / 1000.0);
            ui::ValueRow("Mass", "%.0f kg", vehicle->getMass());
        }

        ui::ValueRow("Fuel used", "%.3f L", units::convert(m_engine->getTotalVolumeFuelConsumed(), units::L));
        ImGui::EndTable();
    }

    if (ImGui::CollapsingHeader("Engine")
        && ImGui::BeginTable("##engine", 2, tableFlags))
    {
        ui::ValueRow("Displacement", "%s", formatDisplacement(m_engine->getDisplacement()).c_str());
        ui::ValueRow("Cylinders", "%d", m_engine->getCylinderCount());
        if (m_engine->getCompressionRatio() > 0) ui::ValueRow("Compression", "%.1f : 1", m_engine->getCompressionRatio());
        ui::ValueRow("Redline", "%.0f rpm", units::toRpm(m_engine->getRedline()));
        ui::ValueRow("Physics", "%s", m_engine->isEngineering() ? "engineering" : "legacy");
        if (m_engine->getFuel()->isKnownType()) {
            ui::ValueRow("Fuel", "%s", m_engine->getFuel()->getDefinition().name.c_str());
        }
        ImGui::EndTable();
    }
}

// Plots ---------------------------------------------------------------------

void AppUi::buildPlotPanel() {
    if (m_engine == nullptr) {
        ImGui::TextDisabled("No engine");
        return;
    }

    if (ImGui::BeginTabBar("##plots")) {
        if (ImGui::BeginTabItem("Cylinder")) { buildCylinderPlot(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("P-V")) { buildPvPlot(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Flow")) { buildFlowPlot(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Dyno")) { buildDynoPlot(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("History")) { buildTrendPlot(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Audio")) { buildAudioPlot(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Performance")) { buildPerformanceTable(); ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }
}

void AppUi::buildCylinderSelector() {
    // Cylinder shown in the crank angle plots
    const int cylinders = (m_engine != nullptr) ? m_engine->getCylinderCount() : 0;
    if (cylinders <= 0) return;

    ImGui::SetNextItemWidth(130 * m_preferences.uiScale);
    const std::string preview = "Cylinder " + std::to_string(m_plotCylinder + 1);
    if (ImGui::BeginCombo("##cylinderselect", preview.c_str())) {
        for (int i = 0; i < cylinders; ++i) {
            const std::string label = "Cylinder " + std::to_string(i + 1);
            if (ImGui::Selectable(label.c_str(), i == m_plotCylinder)) m_plotCylinder = i;
        }
        ImGui::EndCombo();
    }
}

void AppUi::buildCylinderPlot() {
    const bool imperial = (pressureUnits() == "inHg" || pressureUnits() == "psi");
    const double pressureScale = imperial ? 1.0 / units::psi : 1.0 / units::bar;
    const double halfCycle = m_angleCycleLength * 90.0 / constants::pi;

    static std::vector<float> pressure, intake, exhaust, temperature;
    scaled(m_cylinderPressure, pressureScale, 0.0, pressure);
    scaled(m_intakeLift, 1000.0, 0.0, intake);
    scaled(m_exhaustLift, 1000.0, 0.0, exhaust);
    temperature.resize(AngleBins);
    for (int i = 0; i < AngleBins; ++i) {
        const float t = m_cylinderTemperature[i];
        temperature[i] = std::isnan(t) ? NaN : static_cast<float>(toTemperature(t));
    }

    static bool showTemperature = false;
    buildCylinderSelector();
    ImGui::SameLine();
    ImGui::Checkbox("Gas temperature", &showTemperature);

    if (ImPlot::BeginPlot("##cylinderplot", ImVec2(-1, -1), ImPlotFlags_NoTitle)) {
        ImPlot::SetupAxis(ImAxis_X1, "Crank angle after firing TDC [deg]");
        ImPlot::SetupAxis(ImAxis_Y1, imperial ? "Pressure [psi]" : "Pressure [bar]", ImPlotAxisFlags_AutoFit);
        ImPlot::SetupAxis(ImAxis_Y2, "Valve lift [mm]", ImPlotAxisFlags_AuxDefault | ImPlotAxisFlags_AutoFit);
        // Keep flat data (engine stopped) from being zoomed into noise
        ImPlot::SetupAxisZoomConstraints(ImAxis_Y1, imperial ? 5.0 : 0.5, HUGE_VAL);
        ImPlot::SetupAxisZoomConstraints(ImAxis_Y2, 2.0, HUGE_VAL);
        if (showTemperature) {
            const std::string label = std::string("Temperature [") + temperatureUnits() + "]";
            ImPlot::SetupAxis(ImAxis_Y3, label.c_str(), ImPlotAxisFlags_AuxDefault | ImPlotAxisFlags_AutoFit);
        }
        ImPlot::SetupAxisLimits(ImAxis_X1, -halfCycle, halfCycle, ImPlotCond_Once);
        ImPlot::SetupLegend(ImPlotLocation_NorthEast);

        ImPlot::SetAxes(ImAxis_X1, ImAxis_Y2);
        ImPlot::SetNextLineStyle(ImVec4(0.47f, 0.81f, 0.88f, 1.0f));
        ImPlot::PlotLine("Intake lift", m_angle.data(), intake.data(), AngleBins, ImPlotLineFlags_SkipNaN);
        ImPlot::SetNextLineStyle(ImVec4(0.93f, 0.27f, 0.27f, 1.0f));
        ImPlot::PlotLine("Exhaust lift", m_angle.data(), exhaust.data(), AngleBins, ImPlotLineFlags_SkipNaN);

        if (showTemperature) {
            ImPlot::SetAxes(ImAxis_X1, ImAxis_Y3);
            ImPlot::SetNextLineStyle(ImVec4(0.99f, 0.74f, 0.18f, 1.0f));
            ImPlot::PlotLine("Gas temperature", m_angle.data(), temperature.data(), AngleBins, ImPlotLineFlags_SkipNaN);
        }

        ImPlot::SetAxes(ImAxis_X1, ImAxis_Y1);
        ImPlot::SetNextLineStyle(ImGui::ColorConvertU32ToFloat4(ui::AccentColor()), 2.0f);
        ImPlot::PlotLine("Pressure", m_angle.data(), pressure.data(), AngleBins, ImPlotLineFlags_SkipNaN);

        // Firing TDC and the spark of the last cycle
        const double tdc = 0.0;
        ImPlot::SetNextLineStyle(ImVec4(0.6f, 0.6f, 0.6f, 0.6f));
        ImPlot::PlotInfLines("##tdc", &tdc, 1);

        if (m_sampledCylinder >= 0 && m_sampledCylinder < m_engine->getCylinderCount()) {
            const CombustionChamber::CycleMetrics &cycle = m_engine->getChamber(m_sampledCylinder)->getLastCycleMetrics();
            if (cycle.valid && cycle.burned) {
                const double spark = -cycle.sparkAdvance;
                ImPlot::SetNextLineStyle(ImVec4(0.99f, 0.74f, 0.18f, 0.8f));
                ImPlot::PlotInfLines("Spark", &spark, 1);
            }
        }

        ImPlot::EndPlot();
    }
}

void AppUi::buildPvPlot() {
    const bool imperial = (pressureUnits() == "inHg" || pressureUnits() == "psi");
    static std::vector<float> volume, pressure;
    scaled(m_cylinderVolume, 1.0 / units::cc, 0.0, volume);
    scaled(m_cylinderPressure, imperial ? 1.0 / units::psi : 1.0 / units::bar, 0.0, pressure);

    static bool logScale = true;
    buildCylinderSelector();
    ImGui::SameLine();
    ImGui::Checkbox("Log-log", &logScale);

    if (ImPlot::BeginPlot("##pv", ImVec2(-1, -1), ImPlotFlags_NoTitle | ImPlotFlags_NoLegend)) {
        ImPlot::SetupAxis(ImAxis_X1, "Volume [cc]", ImPlotAxisFlags_AutoFit);
        ImPlot::SetupAxis(ImAxis_Y1, imperial ? "Pressure [psi]" : "Pressure [bar]", ImPlotAxisFlags_AutoFit);
        if (logScale) {
            ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Log10);
            ImPlot::SetupAxisScale(ImAxis_Y1, ImPlotScale_Log10);
        }

        ImPlot::SetNextLineStyle(ImGui::ColorConvertU32ToFloat4(ui::AccentColor()), 2.0f);
        ImPlot::PlotLine("p-V", volume.data(), pressure.data(), AngleBins, ImPlotLineFlags_SkipNaN | ImPlotLineFlags_Loop);
        ImPlot::EndPlot();
    }
}

void AppUi::buildFlowPlot() {
    const double halfCycle = m_angleCycleLength * 90.0 / constants::pi;
    buildCylinderSelector();
    if (ImPlot::BeginPlot("##flow", ImVec2(-1, -1), ImPlotFlags_NoTitle)) {
        ImPlot::SetupAxis(ImAxis_X1, "Crank angle after firing TDC [deg]");
        ImPlot::SetupAxis(ImAxis_Y1, "Flow [scfm]", ImPlotAxisFlags_AutoFit);
        ImPlot::SetupAxisZoomConstraints(ImAxis_Y1, 2.0, HUGE_VAL);
        ImPlot::SetupAxisLimits(ImAxis_X1, -halfCycle, halfCycle, ImPlotCond_Once);
        ImPlot::SetupLegend(ImPlotLocation_NorthEast);

        ImPlot::SetNextLineStyle(ImVec4(0.47f, 0.81f, 0.88f, 1.0f));
        ImPlot::PlotLine("Cylinder intake", m_angle.data(), m_intakeFlow.data(), AngleBins, ImPlotLineFlags_SkipNaN);
        ImPlot::SetNextLineStyle(ImVec4(0.93f, 0.27f, 0.27f, 1.0f));
        ImPlot::PlotLine("Cylinder exhaust", m_angle.data(), m_exhaustFlow.data(), AngleBins, ImPlotLineFlags_SkipNaN);
        ImPlot::SetNextLineStyle(ImGui::ColorConvertU32ToFloat4(ui::AccentColor(0.8f)));
        ImPlot::PlotLine("Total exhaust", m_angle.data(), m_totalExhaustFlow.data(), AngleBins, ImPlotLineFlags_SkipNaN);
        ImPlot::EndPlot();
    }
}

void AppUi::buildDynoPlot() {
    static std::vector<float> rpm, torque, power;
    rpm.clear();
    torque.clear();
    power.clear();
    for (int i = 0; i < DynoBins; ++i) {
        if (!m_dynoValid[i]) continue;
        rpm.push_back((i + 0.5f) * 100.0f);
        torque.push_back(m_dynoTorque[i]);
        power.push_back(m_dynoPower[i]);
    }

    if (ImGui::Button("Clear")) resetDynoData();
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("Enable the dynamometer [D] at full throttle to record a power curve.");

    const std::string torqueLabel = "Torque [" + torqueUnits() + "]";
    const std::string powerLabel = "Power [" + powerUnits() + "]";
    if (ImPlot::BeginPlot("##dyno", ImVec2(-1, -1), ImPlotFlags_NoTitle)) {
        ImPlot::SetupAxis(ImAxis_X1, "Engine speed [rpm]");
        ImPlot::SetupAxis(ImAxis_Y1, torqueLabel.c_str(), ImPlotAxisFlags_AutoFit);
        ImPlot::SetupAxis(ImAxis_Y2, powerLabel.c_str(), ImPlotAxisFlags_AuxDefault | ImPlotAxisFlags_AutoFit);
        ImPlot::SetupAxisLimits(ImAxis_X1, 0, units::toRpm(m_engine->getRedline()) * 1.1, ImPlotCond_Once);
        ImPlot::SetupLegend(ImPlotLocation_NorthWest);

        if (!rpm.empty()) {
            ImPlot::SetAxes(ImAxis_X1, ImAxis_Y1);
            ImPlot::SetNextLineStyle(ImGui::ColorConvertU32ToFloat4(ui::AccentColor()), 2.0f);
            ImPlot::PlotLine(torqueLabel.c_str(), rpm.data(), torque.data(), static_cast<int>(rpm.size()));
            ImPlot::SetAxes(ImAxis_X1, ImAxis_Y2);
            ImPlot::SetNextLineStyle(ImVec4(0.95f, 0.58f, 0.75f, 1.0f), 2.0f);
            ImPlot::PlotLine(powerLabel.c_str(), rpm.data(), power.data(), static_cast<int>(rpm.size()));
        }

        const double current = std::max(0.0, m_engine->getRpm());
        ImPlot::SetAxes(ImAxis_X1, ImAxis_Y1);
        ImPlot::SetNextLineStyle(ImVec4(0.6f, 0.6f, 0.6f, 0.6f));
        ImPlot::PlotInfLines("##rpm", &current, 1);
        ImPlot::EndPlot();
    }
}

void AppUi::buildTrendPlot() {
    const std::string torqueLabel = "Brake torque [" + torqueUnits() + "]";
    if (ImPlot::BeginPlot("##history", ImVec2(-1, -1), ImPlotFlags_NoTitle)) {
        ImPlot::SetupAxis(ImAxis_X1, "Time [s]");
        ImPlot::SetupAxis(ImAxis_Y1, "Engine speed [rpm]", ImPlotAxisFlags_AutoFit);
        ImPlot::SetupAxisZoomConstraints(ImAxis_Y1, 500.0, HUGE_VAL);
        ImPlot::SetupAxis(ImAxis_Y2, "Manifold [kPa]", ImPlotAxisFlags_AuxDefault | ImPlotAxisFlags_AutoFit);
        ImPlot::SetupAxisZoomConstraints(ImAxis_Y2, 20.0, HUGE_VAL);
        ImPlot::SetupAxis(ImAxis_Y3, torqueLabel.c_str(), ImPlotAxisFlags_AuxDefault | ImPlotAxisFlags_AutoFit);
        ImPlot::SetupAxisZoomConstraints(ImAxis_Y3, 10.0, HUGE_VAL);
        ImPlot::SetupAxisLimits(ImAxis_X1, m_clock - 10.0, m_clock, ImPlotCond_Always);
        ImPlot::SetupLegend(ImPlotLocation_NorthWest);

        ImPlot::SetAxes(ImAxis_X1, ImAxis_Y1);
        ImPlot::SetNextLineStyle(ImGui::ColorConvertU32ToFloat4(ui::AccentColor()), 2.0f);
        ImPlot::PlotLine("Speed", m_frameTime.data(), m_rpmHistory.data(), FrameHistory, ImPlotLineFlags_SkipNaN, m_frameHistoryIndex);
        ImPlot::SetAxes(ImAxis_X1, ImAxis_Y2);
        ImPlot::SetNextLineStyle(ImVec4(0.47f, 0.81f, 0.88f, 1.0f));
        ImPlot::PlotLine("Manifold", m_frameTime.data(), m_mapHistory.data(), FrameHistory, ImPlotLineFlags_SkipNaN, m_frameHistoryIndex);
        ImPlot::SetAxes(ImAxis_X1, ImAxis_Y3);
        ImPlot::SetNextLineStyle(ImVec4(0.95f, 0.58f, 0.75f, 1.0f));
        ImPlot::PlotLine("Torque", m_frameTime.data(), m_torqueHistory.data(), FrameHistory, ImPlotLineFlags_SkipNaN, m_frameHistoryIndex);
        ImPlot::EndPlot();
    }
}

void AppUi::buildAudioPlot() {
    if (ImPlot::BeginPlot("##audio", ImVec2(-1, -1), ImPlotFlags_NoTitle | ImPlotFlags_NoLegend)) {
        const double dt = 4.0 / 44100.0 * 1000.0;
        ImPlot::SetupAxis(ImAxis_X1, "Time [ms]");
        ImPlot::SetupAxis(ImAxis_Y1, "Output");
        ImPlot::SetupAxisLimits(ImAxis_X1, 0, AudioHistory * dt, ImPlotCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, -1.0, 1.0, ImPlotCond_Once);
        ImPlot::SetNextLineStyle(ImVec4(0.47f, 0.81f, 0.88f, 1.0f));
        ImPlot::PlotLine("Audio", m_audio.data(), AudioHistory, dt, 0.0, 0, m_audioIndex);
        ImPlot::EndPlot();
    }
}

void AppUi::buildPerformanceTable() {
    if (m_simulator == nullptr) return;

    const double frequency = m_simulator->getSimulationFrequency();
    const double load = m_timePerStep * frequency * m_simulator->getSimulationSpeed();

    if (ImGui::BeginTable("##performance", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp, ImVec2(std::min(ImGui::GetContentRegionAvail().x, 520.0f * m_preferences.uiScale), 0))) {
        ui::ValueRow("Frame rate", "%.0f fps", ImGui::GetIO().Framerate);
        ui::ValueRow("Simulation frequency", "%.0f Hz", frequency);
        ui::ValueRow("Steps per slice", "%d", m_simulator->getFrameIterationCount());
        ui::ValueRow("Time per step", "%.2f us", m_timePerStep * 1E6);
        ui::ValueRow("Physics load", "%.0f %% of real time", load * 100);
        ui::ValueRow("Audio latency", "%.0f %% of target", m_audioLatency * 100);
        ui::ValueRow("Synthesizer input buffer", "%.0f %%", m_inputBufferUsage * 100);
        ui::ValueRow("Frame (average / worst)", "%.1f / %.1f ms", m_profile.total * 1E3, m_profilePeak.total * 1E3);
        ui::ValueRow("  User interface", "%.1f / %.1f ms", m_profile.ui * 1E3, m_profilePeak.ui * 1E3);
        ui::ValueRow("  Waiting for the simulation", "%.1f / %.1f ms", m_profile.simulation * 1E3, m_profilePeak.simulation * 1E3);
        ui::ValueRow("  Engine view", "%.1f / %.1f ms", m_profile.scene * 1E3, m_profilePeak.scene * 1E3);
        ui::ValueRow("  Present (incl. vsync)", "%.1f / %.1f ms", m_profile.present * 1E3, m_profilePeak.present * 1E3);
        ImGui::EndTable();
    }

    if (load > 0.9) {
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(ui::WarningColor()),
            "The simulation is close to running slower than real time. Lower the simulation "
            "frequency (Tuning or Settings) if the audio stutters.");
    }
}

void AppUi::buildEngineViewOverlay() {
    if (m_engine == nullptr || m_simulator == nullptr) return;

    // Small read-out in the corner of the engine view
    const ImVec2 scale = ImGui::GetIO().DisplayFramebufferScale;
    const ImVec2 origin(m_engineViewRect.x / scale.x + 12, m_engineViewRect.y / scale.y + 10);
    ImDrawList *draw = ImGui::GetBackgroundDrawList();

    char text[224];
    const Transmission *transmission = m_simulator->getTransmission();
    const int gear = (transmission != nullptr) ? transmission->getGear() : -1;
    const bool limiting = m_engine->getIgnitionModule()->m_enabled && m_engine->getIgnitionModule()->isLimiting();
    char boost[48] = "";
    if (const ForcedInduction *fi = m_engine->getForcedInduction()) {
        const bool imperial = (pressureUnits() == "inHg" || pressureUnits() == "psi");
        std::snprintf(boost, sizeof(boost), "   boost %.*f %s",
            imperial ? 1 : 2,
            imperial ? units::convert(fi->getBoost(), units::psi) : units::convert(fi->getBoost(), units::bar),
            imperial ? "psi" : "bar");
    }
    std::snprintf(text, sizeof(text), "%.0f rpm   %s   throttle %.0f%%%s%s%s%s",
        std::max(0.0, m_engine->getRpm()),
        gear < 0 ? "N" : ("gear " + std::to_string(gear + 1)).c_str(),
        m_engine->getSpeedControl() * 100,
        boost,
        m_simulator->m_dyno.m_enabled ? "   dyno" : "",
        m_engine->getIgnitionModule()->m_enabled ? "" : "   ignition off",
        limiting ? "   REV LIMITER" : "");

    const ImU32 shadow = IM_COL32(0, 0, 0, 160);
    draw->AddText(ImVec2(origin.x + 1, origin.y + 1), shadow, text);
    draw->AddText(origin, IM_COL32(230, 230, 230, 220), text);

    if (m_layout == Layout::EngineOnly) {
        const char *hint = "Tab: show panels";
        draw->AddText(ImVec2(origin.x, origin.y + ImGui::GetTextLineHeightWithSpacing()), IM_COL32(200, 200, 200, 140), hint);
    }
}

// Windows -------------------------------------------------------------------

void AppUi::buildConsoleWindow() {
    const float s = m_preferences.uiScale;
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(720 * s, 420 * s), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Console", &m_showConsole)) {
        ImGui::End();
        return;
    }

    if (ImGui::Button("Reload [Enter]")) m_controls.reloadRequested = true;
    ImGui::SameLine();
    if (ImGui::Button("Copy script log")) ImGui::SetClipboardText(m_app->getScriptLog().c_str());
    ImGui::SameLine();
    if (ImGui::Button("Clear messages")) m_log.clear();
    ImGui::SameLine();
    ImGui::TextDisabled("%s", m_app->getEngineScriptPath().c_str());

    if (ImGui::BeginTabBar("##console")) {
        if (ImGui::BeginTabItem("Script")) {
            ImGui::BeginChild("##scriptlog", ImVec2(0, 0), ImGuiChildFlags_Borders);
            ImGui::PushFont(m_monoFont);
            const std::string &log = m_app->getScriptLog();
            if (log.empty()) {
                ImGui::TextDisabled("The engine script compiled without messages.");
            }

            std::istringstream lines(log);
            std::string line;
            while (std::getline(lines, line)) {
                const bool error = line.find("rror") != std::string::npos;
                const bool warning = line.find("arning") != std::string::npos;
                if (error) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(ui::DangerColor()));
                else if (warning) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(ui::WarningColor()));
                ImGui::TextUnformatted(line.c_str());
                if (error || warning) ImGui::PopStyleColor();
            }

            if (m_engine != nullptr) {
                for (const std::string &warning : m_engine->getWarnings()) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(ui::WarningColor()));
                    ImGui::TextWrapped("Warning: %s", warning.c_str());
                    ImGui::PopStyleColor();
                }
            }
            ImGui::PopFont();
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Messages")) {
            ImGui::BeginChild("##messages", ImVec2(0, 0), ImGuiChildFlags_Borders);
            ImGui::PushFont(m_monoFont);
            for (const LogEntry &entry : m_log) {
                ImGui::TextDisabled("%8.1f", entry.time);
                ImGui::SameLine();
                ImGui::TextUnformatted(entry.text.c_str());
            }
            if (m_scrollLogToBottom) {
                ImGui::SetScrollHereY(1.0f);
                m_scrollLogToBottom = false;
            }
            ImGui::PopFont();
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

void AppUi::buildSettingsWindow() {
    const float s = m_preferences.uiScale;
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(560 * s, 460 * s), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Settings", &m_showSettings)) {
        ImGui::End();
        return;
    }

    UserPreferences &p = m_preferences;
    bool changed = false;
    bool applyToApp = false;
    const float labelWidth = 220 * s;

    auto label = [&](const char *text) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(text);
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-FLT_MIN);
    };

    auto unitCombo = [&](const char *id, std::string *value, const char *const *options, int count) {
        int index = 0;
        for (int i = 1; i < count; ++i) {
            if (*value == options[i]) index = i;
        }

        std::vector<const char *> names(options, options + count);
        names[0] = "Theme default";
        if (ImGui::Combo(id, &index, names.data(), count)) {
            *value = (index == 0) ? std::string() : std::string(options[index]);
            changed = true;
        }
    };

    if (ImGui::BeginTabBar("##settings")) {
        if (ImGui::BeginTabItem("Display")) {
            label("Fullscreen");
            if (ImGui::Checkbox("##fullscreen", &p.fullscreen)) { m_app->setFullscreen(p.fullscreen); changed = true; }
            label("Vertical sync");
            if (ImGui::Checkbox("##vsync", &p.vsync)) { changed = applyToApp = true; }
            label("Interface scale");
            // Applied when the slider is released so the window doesn't
            // change size while dragging
            static float pendingScale = 1.0f;
            ImGui::SliderFloat("##scale", &pendingScale, 0.75f, 2.0f, "%.2fx");
            if (ImGui::IsItemDeactivatedAfterEdit()) { p.uiScale = pendingScale; changed = true; }
            if (!ImGui::IsItemActive()) pendingScale = p.uiScale;
            label("Interface theme");
            {
                const char *themes[] = { "dark", "light", "classic" };
                const char *names[] = { "Dark", "Light", "Classic (engine theme colors)" };
                int index = 0;
                for (int i = 0; i < 3; ++i) if (p.uiTheme == themes[i]) index = i;
                if (ImGui::Combo("##theme", &index, names, 3)) { p.uiTheme = themes[index]; changed = true; }
            }
            label("Engine view");
            {
                int realistic = (p.engineViewStyle == "classic") ? 1 : 0;
                const char *styles[] = { "Realistic (shaded metal)", "Classic (flat theme colors)" };
                if (ImGui::Combo("##viewstyle", &realistic, styles, 2)) {
                    p.engineViewStyle = realistic ? "classic" : "realistic";
                    changed = true;
                }
            }
            label("Frame around the engine view");
            if (ImGui::Checkbox("##frame", &p.showEngineViewFrame)) changed = true;
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Units")) {
            const char *power[] = { "", "hp", "kW" };
            const char *torque[] = { "", "lb-ft", "Nm" };
            const char *speed[] = { "", "mph", "kph" };
            const char *pressure[] = { "", "inHg", "psi", "kPa", "mbar", "bar" };
            const char *temperature[] = { "C", "F", "K" };

            label("Power"); unitCombo("##power", &p.powerUnits, power, 3);
            label("Torque"); unitCombo("##torque", &p.torqueUnits, torque, 3);
            label("Speed"); unitCombo("##speed", &p.speedUnits, speed, 3);
            label("Manifold pressure"); unitCombo("##pressure", &p.pressureUnits, pressure, 6);
            label("Temperature");
            int index = 0;
            for (int i = 0; i < 3; ++i) if (p.temperatureUnits == temperature[i]) index = i;
            if (ImGui::Combo("##temperature", &index, temperature, 3)) { p.temperatureUnits = temperature[index]; changed = true; }
            ImGui::Spacing();
            ImGui::TextDisabled("\"Theme default\" uses the units of the engine's theme script.");
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Audio")) {
            label("Master volume");
            float volume = p.masterVolume * 100;
            if (ImGui::SliderFloat("##volume", &volume, 0, 100, "%.0f %%")) { p.masterVolume = volume / 100; changed = applyToApp = true; }
            label("Mute when in the background");
            if (ImGui::Checkbox("##mute", &p.muteWhenInactive)) changed = true;
            label("Audio buffer target");
            float latency = p.targetAudioLatency * 1000;
            if (ImGui::SliderFloat("##latency", &latency, 20, 250, "%.0f ms")) { p.targetAudioLatency = latency / 1000; changed = applyToApp = true; }
            ImGui::TextDisabled("Larger buffers stutter less on slow computers but respond later.");
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Simulation")) {
            label("Simulation frequency");
            bool custom = p.simulationFrequency > 0;
            if (ImGui::Checkbox("Override the engine's setting", &custom)) {
                p.simulationFrequency = custom ? ((m_simulator != nullptr) ? m_simulator->getSimulationFrequency() : 10000) : 0;
                changed = applyToApp = true;
            }
            if (custom) {
                ImGui::Dummy(ImVec2(labelWidth - ImGui::GetStyle().ItemSpacing.x, 0));
                ImGui::SameLine();
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (ImGui::SliderInt("##frequency", &p.simulationFrequency, 2000, 50000, "%d Hz", ImGuiSliderFlags_Logarithmic)) {
                    changed = applyToApp = true;
                }
            }
            ImGui::TextDisabled("Higher frequencies are more accurate and need a faster computer.");
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Updates")) {
            label("Check for updates at startup");
            if (ImGui::Checkbox("##updates", &p.checkForUpdates)) { changed = applyToApp = true; }

            const Updater &updater = m_app->getUpdater();
            ImGui::Spacing();
            ImGui::Text("Installed version: %s", EngineSimApplication::getBuildVersion().c_str());
            ImGui::Text("Status: %s", updater.getStatusText().c_str());
            if (!updater.getError().empty() && updater.getState() == Updater::State::Failed) {
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(ui::DangerColor()), "%s", updater.getError().c_str());
            }
            if (ImGui::Button("Check now / install")) m_app->requestUpdateAction();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Files")) {
            ImGui::TextWrapped("User data: %s", m_app->getUserDataPath().c_str());
            ImGui::TextWrapped("Application data: %s", m_app->getDataRoot().c_str());
            ImGui::Spacing();
            ImGui::TextWrapped(
                "Engine scripts placed in assets/engines inside the user data folder appear in "
                "File > Load engine. Designs from the engine designer are saved in "
                "assets/engines/custom.");
            if (ImGui::Button("Open user data folder")) {
                const std::string url = "file://" + m_app->getUserDataPath();
                SDL_OpenURL(url.c_str());
            }
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::Separator();
    if (ImGui::Button("Restore defaults")) {
        p = UserPreferences();
        m_app->setFullscreen(p.fullscreen);
        changed = applyToApp = true;
    }

    if (changed) {
        m_preferencesDirty = true;
        if (applyToApp) m_app->applyPreferences();
    }

    ImGui::End();
}

void AppUi::generateDesign(bool load) {
    m_design.validate();

    const fs::path directory = fs::path(m_app->getUserDataPath()) / "assets" / "engines" / "custom";
    std::error_code ec;
    fs::create_directories(directory, ec);

    const fs::path path = directory / (designFileName(m_design.name) + ".mr");
    std::ofstream file(path, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        m_designStatus = "Could not write " + path.string();
        m_designStatusError = true;
        return;
    }

    file << generateEngineScript(m_design);
    file.close();

    m_designPath = path.string();
    m_designStatus = "Saved " + path.filename().string() + " to My engines";
    m_designStatusError = false;
    m_scriptListsValid = false;
    m_savedDesignsValid = false;

    if (load) {
        m_controls.loadScript = m_designPath;
    }
}

bool AppUi::openDesignFromScript(const std::string &path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::stringstream ss;
    ss << file.rdbuf();

    EngineDesign design;
    if (!readEngineDesign(ss.str(), &design)) return false;

    m_design = design;
    m_designPath = path;
    return true;
}

void AppUi::loadDesign(const EngineDesign &design) {
    // Presets are generated into a cache folder; saving them from the
    // designer puts a copy into My engines
    const fs::path directory = fs::path(m_app->getUserDataPath()) / "assets" / "engines" / "presets";
    std::error_code ec;
    fs::create_directories(directory, ec);

    const fs::path path = directory / (designFileName(design.name) + ".mr");
    std::ofstream file(path, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        log("Could not write " + path.string());
        return;
    }

    file << generateEngineScript(design);
    file.close();

    m_scriptListsValid = false;
    m_controls.loadScript = path.string();
}

void AppUi::refreshSavedDesigns() {
    if (m_savedDesignsValid) return;
    m_savedDesignsValid = true;
    m_savedDesigns.clear();

    const fs::path directory = fs::path(m_app->getUserDataPath()) / "assets" / "engines" / "custom";
    std::error_code ec;
    if (!fs::is_directory(directory, ec)) return;

    for (const fs::directory_entry &entry : fs::directory_iterator(directory, ec)) {
        if (entry.path().extension() != ".mr") continue;

        std::ifstream file(entry.path());
        std::stringstream ss;
        ss << file.rdbuf();

        SavedDesign saved;
        saved.path = entry.path().string();
        saved.designed = readEngineDesign(ss.str(), &saved.design);
        if (!saved.designed) saved.design.name = entry.path().stem().string();
        m_savedDesigns.push_back(saved);
    }

    std::sort(m_savedDesigns.begin(), m_savedDesigns.end(), [](const SavedDesign &a, const SavedDesign &b) {
        return a.design.name < b.design.name;
    });
}

namespace {

    std::string describeDesign(const EngineDesign &d) {
        char text[160];
        if (d.isRotary()) {
            std::snprintf(text, sizeof(text), "%d-rotor rotary, %s", d.rotors, formatDisplacement(d.displacement()).c_str());
        }
        else {
            const char *layout =
                (d.layout == EngineDesign::Layout::V) ? "V"
                : (d.layout == EngineDesign::Layout::Flat) ? "flat-"
                : (d.layout == EngineDesign::Layout::W) ? "W"
                : "I";
            std::snprintf(text, sizeof(text), "%s%d, %s", layout, d.cylinders, formatDisplacement(d.displacement()).c_str());
        }

        std::string result = text;
        if (d.isBoosted()) result += std::string(", ") + EngineDesign::inductionName(d.induction);
        return result;
    }

} /* namespace */

void AppUi::buildLibraryWindow() {
    const float s = m_preferences.uiScale;
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(760 * s, 560 * s), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Engine library", &m_showLibrary)) {
        ImGui::End();
        return;
    }

    refreshSavedDesigns();

    const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH
        | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY;

    if (ImGui::BeginTabBar("##library")) {
        if (ImGui::BeginTabItem("Presets")) {
            ImGui::TextDisabled("Well-known engines built with the designer. Load one, or customize it and save your own version.");
            if (ImGui::BeginTable("##presets", 3, flags)) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("Engine", ImGuiTableColumnFlags_WidthStretch, 0.32f);
                ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch, 0.68f);
                ImGui::TableSetupColumn("##actions", ImGuiTableColumnFlags_WidthFixed, 150 * s);
                ImGui::TableHeadersRow();

                const std::vector<EngineDesign> &presets = builtinEnginePresets();
                for (size_t i = 0; i < presets.size(); ++i) {
                    const EngineDesign &d = presets[i];
                    ImGui::PushID(static_cast<int>(i));
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(d.name.c_str());
                    ImGui::TextDisabled("%s", describeDesign(d).c_str());
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextWrapped("%s", d.description.c_str());
                    ImGui::TableSetColumnIndex(2);
                    if (ImGui::Button("Load")) loadDesign(d);
                    ImGui::SameLine();
                    if (ImGui::Button("Customize")) {
                        m_design = d;
                        m_designPath.clear();
                        m_designStatus = "Editing a copy of the " + d.name + " preset";
                        m_designStatusError = false;
                        m_showDesigner = true;
                    }
                    ImGui::PopID();
                }

                ImGui::EndTable();
            }

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("My engines")) {
            if (ImGui::Button("New engine...")) {
                m_design = EngineDesign();
                m_designPath.clear();
                m_designStatus.clear();
                m_showDesigner = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Refresh")) m_savedDesignsValid = false;
            ImGui::SameLine();
            if (ImGui::Button("Open folder")) {
                const fs::path directory = fs::path(m_app->getUserDataPath()) / "assets" / "engines" / "custom";
                std::error_code ec;
                fs::create_directories(directory, ec);
                SDL_OpenURL(("file://" + directory.string()).c_str());
            }

            if (m_savedDesigns.empty()) {
                ImGui::Spacing();
                ImGui::TextWrapped("No saved engines yet. Design one (or customize a preset) and press Save; "
                    "engine scripts copied into the folder appear here too.");
            }
            else if (ImGui::BeginTable("##saved", 3, flags)) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("Engine", ImGuiTableColumnFlags_WidthStretch, 0.45f);
                ImGui::TableSetupColumn("Configuration", ImGuiTableColumnFlags_WidthStretch, 0.55f);
                ImGui::TableSetupColumn("##actions", ImGuiTableColumnFlags_WidthFixed, 250 * s);
                ImGui::TableHeadersRow();

                for (size_t i = 0; i < m_savedDesigns.size(); ++i) {
                    const SavedDesign &saved = m_savedDesigns[i];
                    ImGui::PushID(saved.path.c_str());
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    const bool current = (saved.path == m_app->getEngineScriptPath());
                    if (current) ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(ui::AccentColor()), "%s", saved.design.name.c_str());
                    else ImGui::TextUnformatted(saved.design.name.c_str());
                    ImGui::TableSetColumnIndex(1);
                    if (saved.designed) ImGui::TextDisabled("%s", describeDesign(saved.design).c_str());
                    else ImGui::TextDisabled("hand-written script");
                    ImGui::TableSetColumnIndex(2);
                    if (ImGui::Button("Load")) m_controls.loadScript = saved.path;
                    if (saved.designed) {
                        ImGui::SameLine();
                        if (ImGui::Button("Edit")) {
                            m_design = saved.design;
                            m_designPath = saved.path;
                            m_designStatus.clear();
                            m_showDesigner = true;
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Duplicate")) {
                            m_design = saved.design;
                            m_design.name += " (copy)";
                            generateDesign(false);
                        }
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Delete")) {
                        m_pendingDelete = saved.path;
                        ImGui::OpenPopup("Delete engine?");
                    }

                    if (ImGui::BeginPopupModal("Delete engine?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                        ImGui::Text("Delete %s?", fs::path(m_pendingDelete).filename().string().c_str());
                        ImGui::TextDisabled("The script file is removed from the My engines folder.");
                        if (ImGui::Button("Delete", ImVec2(120 * s, 0))) {
                            std::error_code ec;
                            fs::remove(m_pendingDelete, ec);
                            if (ec) log("Could not delete " + m_pendingDelete + ": " + ec.message());
                            m_savedDesignsValid = false;
                            m_scriptListsValid = false;
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Cancel", ImVec2(120 * s, 0))) ImGui::CloseCurrentPopup();
                        ImGui::EndPopup();
                    }

                    ImGui::PopID();
                }

                ImGui::EndTable();
            }

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

void AppUi::buildDesignerWindow() {
    const float s = m_preferences.uiScale;
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(660 * s, 680 * s), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Engine designer", &m_showDesigner)) {
        ImGui::End();
        return;
    }

    EngineDesign &d = m_design;
    constexpr double Deg = constants::pi / 180.0;
    const float labelWidth = 230 * s;
    auto label = [&](const char *text) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(text);
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-FLT_MIN);
    };
    const bool imperial = (pressureUnits() == "inHg" || pressureUnits() == "psi");

    // Summary
    {
        char name[128];
        std::snprintf(name, sizeof(name), "%s", d.name.c_str());
        label("Name");
        if (ImGui::InputText("##name", name, sizeof(name))) d.name = name;

        label("Start from");
        if (ImGui::BeginCombo("##preset", "a preset...")) {
            for (const EngineDesign &preset : builtinEnginePresets()) {
                if (ImGui::Selectable(preset.name.c_str())) {
                    d = preset;
                    m_designPath.clear();
                    m_designStatus = "Editing a copy of the " + preset.name + " preset";
                    m_designStatusError = false;
                }
            }
            ImGui::EndCombo();
        }

        if (d.isRotary()) {
            ImGui::TextDisabled("%s: %d x %.0f cc, compression %.1f : 1",
                formatDisplacement(d.displacement()).c_str(), d.rotors,
                units::convert(d.cylinderDisplacement(), units::cc), d.compressionRatio);
        }
        else {
            const double pistonSpeed = 2 * d.stroke * d.redline / 60.0;
            ImGui::TextDisabled("%s, %.0f cc per cylinder, bore/stroke %.2f, mean piston speed at redline %.1f m/s",
                formatDisplacement(d.displacement()).c_str(),
                units::convert(d.cylinderDisplacement(), units::cc),
                d.bore / d.stroke,
                pistonSpeed);
            if (pistonSpeed > 25.0) {
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(ui::WarningColor()),
                    "Mean piston speeds above about 25 m/s are beyond production engines.");
            }

            std::string order;
            for (int c : d.firingOrder()) order += (order.empty() ? "" : "-") + std::to_string(c);
            ImGui::TextDisabled("Firing order %s", order.c_str());
        }
    }

    if (ImGui::BeginTabBar("##design")) {
        if (ImGui::BeginTabItem("Layout")) {
            label("Configuration");
            int layout = static_cast<int>(d.layout);
            const char *layouts[] = { "Inline", "V", "Flat (boxer)", "W", "Rotary (Wankel)" };
            if (ImGui::Combo("##layout", &layout, layouts, 5)) {
                const EngineDesign::Layout previous = d.layout;
                d.layout = static_cast<EngineDesign::Layout>(layout);
                if (d.layout == EngineDesign::Layout::Flat) d.bankAngle = constants::pi;
                else if (d.layout == EngineDesign::Layout::V && d.bankAngle >= constants::pi) d.bankAngle = constants::pi / 2;
                else if (d.layout == EngineDesign::Layout::W) {
                    d.bankAngle = (d.wBanks == 3) ? 120 * Deg : 72 * Deg;
                    if (d.cylinders < 8) d.cylinders = 12;
                }
                if (d.layout == EngineDesign::Layout::Rotary || previous == EngineDesign::Layout::Rotary) {
                    d.compressionRatio = (d.layout == EngineDesign::Layout::Rotary) ? 9.0 : 10.5;
                    if (d.redline < 7000 && d.layout == EngineDesign::Layout::Rotary) d.redline = 8000;
                }
                d.firingOrderText.clear();
                d.validate();
            }

            if (d.isRotary()) {
                label("Rotors");
                ImGui::SliderInt("##rotors", &d.rotors, 1, 4);
                label("Eccentricity");
                sliderDouble("##ecc", &d.eccentricity, 0.008, 0.025, "%.1f mm", 1000.0);
                label("Generating radius");
                sliderDouble("##genr", &d.generatingRadius, 5.0 * d.eccentricity, 10.0 * d.eccentricity, "%.1f mm", 1000.0);
                ImGui::SameLine(); ui::HelpMarker("Rotor center to apex. R / e between 6.5 and 7.5 is typical.");
                label("Rotor width");
                sliderDouble("##rwidth", &d.rotorWidth, 0.03, 0.12, "%.1f mm", 1000.0);
            }
            else {
                label("Cylinders");
                if (d.layout == EngineDesign::Layout::Inline) {
                    ImGui::SliderInt("##cylinders", &d.cylinders, 1, 12);
                }
                else {
                    const int banks = d.bankCount();
                    int perBank = d.cylinders / banks;
                    char format[16];
                    std::snprintf(format, sizeof(format), "%d", perBank * banks);
                    if (ImGui::SliderInt("##cylinders", &perBank, 1, (banks == 2) ? 8 : 6, format)) {
                        d.cylinders = perBank * banks;
                        d.firingOrderText.clear();
                    }
                }

                if (d.layout == EngineDesign::Layout::V) {
                    label("Bank angle");
                    sliderDouble("##bankangle", &d.bankAngle, 10 * Deg, 150 * Deg, "%.0f deg", 1.0 / Deg);
                }
                else if (d.layout == EngineDesign::Layout::W) {
                    label("Banks");
                    if (ImGui::RadioButton("Two narrow V's (4 banks)", d.wBanks == 4)) {
                        d.wBanks = 4; d.bankAngle = 72 * Deg; d.validate();
                    }
                    ImGui::SameLine();
                    if (ImGui::RadioButton("Broad arrow (3 banks)", d.wBanks == 3)) {
                        d.wBanks = 3; d.bankAngle = 120 * Deg; d.validate();
                    }
                    label(d.wBanks == 4 ? "Angle between the halves" : "Angle between the outer banks");
                    sliderDouble("##bankangle", &d.bankAngle, 30 * Deg, 150 * Deg, "%.0f deg", 1.0 / Deg);
                    if (d.wBanks == 4) {
                        label("Angle within each half");
                        sliderDouble("##vrangle", &d.vrAngle, 5 * Deg, 30 * Deg, "%.1f deg", 1.0 / Deg);
                    }
                }

                char order[96];
                std::snprintf(order, sizeof(order), "%s", d.firingOrderText.c_str());
                label("Firing order");
                if (ImGui::InputTextWithHint("##firing", "automatic (even firing)", order, sizeof(order))) {
                    d.firingOrderText = order;
                }
                std::vector<int> parsed;
                if (!d.firingOrderText.empty() && !EngineDesign::parseFiringOrder(d.firingOrderText, d.cylinders, &parsed)) {
                    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(ui::WarningColor()),
                        "List each cylinder 1-%d once (e.g. 1-3-4-2); the automatic order is used until then.", d.cylinders);
                }
                else if (d.bankCount() > 1) {
                    ImGui::TextDisabled("Cylinders are numbered bank by bank: 1-%d on the first bank, and so on.", d.cylindersPerBank());
                }

                label("Bore");
                sliderDouble("##bore", &d.bore, 0.03, 0.15, "%.1f mm", 1000.0);
                label("Stroke");
                sliderDouble("##stroke", &d.stroke, 0.03, 0.15, "%.1f mm", 1000.0);
                label("Connecting rod length");
                sliderDouble("##rod", &d.rodLength, 1.2 * d.stroke, 3.0 * d.stroke, "%.1f mm", 1000.0);
            }

            label("Compression ratio");
            sliderDouble("##cr", &d.compressionRatio, 6.0, 18.0, "%.1f : 1");
            label("Flywheel mass");
            sliderDouble("##flywheel", &d.flywheelMass, 0.5, 40.0, "%.1f kg");
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(d.isRotary() ? "Ports" : "Valvetrain")) {
            if (d.isRotary()) {
                ImGui::TextWrapped("Peripheral ports, timed in eccentric shaft degrees. A port is open for at least "
                    "360 degrees; wider ports stay open longer.");
                ImGui::SeparatorText("Intake");
                label("Opens (before TDC)");
                sliderDouble("##io", &d.intakeOpen, -40 * Deg, 60 * Deg, "%.0f deg", 1.0 / Deg);
                label("Closes (after BDC)");
                sliderDouble("##ic", &d.intakeClose, std::min(std::max(0.0, 95 * Deg - d.intakeOpen), 115 * Deg), 120 * Deg, "%.0f deg", 1.0 / Deg);
                ImGui::SeparatorText("Exhaust");
                label("Opens (before BDC)");
                sliderDouble("##eo", &d.exhaustOpen, 30 * Deg, 110 * Deg, "%.0f deg", 1.0 / Deg);
                label("Closes (after TDC)");
                sliderDouble("##ec", &d.exhaustClose, std::max(-20 * Deg, 95 * Deg - d.exhaustOpen), 70 * Deg, "%.0f deg", 1.0 / Deg);
                ImGui::TextDisabled("Overlap %.0f deg", (d.intakeOpen + d.exhaustClose) / Deg);
            }
            else {
                label("Valvetrain");
                const char *valvetrains[] = { "dohc_bucket", "sohc_finger", "sohc_rocker", "pushrod", "pushrod_roller" };
                const char *valvetrainNames[] = { "DOHC, bucket tappets", "SOHC, finger followers", "SOHC, rocker arms", "Pushrod (cam in block)", "Pushrod, roller lifters" };
                int valvetrain = 0;
                for (int i = 0; i < 5; ++i) if (d.valvetrain == valvetrains[i]) valvetrain = i;
                if (ImGui::Combo("##valvetrain", &valvetrain, valvetrainNames, 5)) d.valvetrain = valvetrains[valvetrain];
                label("Valves per cylinder");
                ImGui::SliderInt("##valves", &d.valvesPerCylinder, 2, 5);
                ImGui::SeparatorText("Intake cam");
                label("Duration at 0.050\"");
                sliderDouble("##iduration", &d.intakeDuration, 160 * Deg, 320 * Deg, "%.0f deg", 1.0 / Deg);
                label("Lift");
                sliderDouble("##ilift", &d.intakeLift, 0.004, 0.020, "%.1f mm", 1000.0);
                label("Lobe center (ATDC)");
                sliderDouble("##icenter", &d.intakeLobeCenter, 90 * Deg, 140 * Deg, "%.0f deg", 1.0 / Deg);
                ImGui::SeparatorText("Exhaust cam");
                label("Duration at 0.050\"");
                sliderDouble("##eduration", &d.exhaustDuration, 160 * Deg, 320 * Deg, "%.0f deg", 1.0 / Deg);
                label("Lift");
                sliderDouble("##elift", &d.exhaustLift, 0.004, 0.020, "%.1f mm", 1000.0);
                label("Lobe center (BTDC)");
                sliderDouble("##ecenter", &d.exhaustLobeCenter, 90 * Deg, 140 * Deg, "%.0f deg", 1.0 / Deg);

                const double lsa = (d.intakeLobeCenter + d.exhaustLobeCenter) / 2 / Deg;
                const double overlap = (d.intakeDuration + d.exhaustDuration) / 2 - (d.intakeLobeCenter + d.exhaustLobeCenter);
                ImGui::TextDisabled("Lobe separation %.1f deg, overlap at 0.050\" %.0f deg", lsa, overlap / Deg);
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Intake & exhaust")) {
            label("Throttle body flow");
            bool autoFlow = d.throttleFlow <= 0;
            if (ImGui::Checkbox("Size from the displacement", &autoFlow)) d.throttleFlow = autoFlow ? 0.0 : 600.0;
            if (!autoFlow) {
                label("  Flow at 1.5 inHg");
                sliderDouble("##flow", &d.throttleFlow, 50.0, 3000.0, "%.0f cfm");
            }
            label("Idle air bypass");
            double idle = (1.0 - d.idleThrottle) * 100.0;
            if (sliderDouble("##idle", &idle, 0.0, 6.0, "%.2f %%")) d.idleThrottle = 1.0 - idle / 100.0;
            label("Intake runner length");
            sliderDouble("##runner", &d.runnerLength, 0.05, 0.8, "%.0f mm", 1000.0);
            label("Exhaust primary length");
            sliderDouble("##primary", &d.primaryLength, 0.1, 1.5, "%.0f mm", 1000.0);
            label("Exhaust system length");
            sliderDouble("##exlength", &d.exhaustLength, 0.5, 6.0, "%.2f m");
            label("Exhaust pipe diameter");
            sliderDouble("##exdiameter", &d.exhaustDiameter, 0.025, 0.12, "%.0f mm", 1000.0);

            label("Exhaust sound");
            const auto &sounds = EngineDesign::exhaustSounds();
            const char *current = sounds.front().second.c_str();
            for (const auto &sound : sounds) if (sound.first == d.exhaustSound) current = sound.second.c_str();
            if (ImGui::BeginCombo("##sound", current)) {
                for (const auto &sound : sounds) {
                    if (ImGui::Selectable(sound.second.c_str(), sound.first == d.exhaustSound)) d.exhaustSound = sound.first;
                }
                ImGui::EndCombo();
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Forced induction")) {
            label("Induction");
            int induction = static_cast<int>(d.induction);
            const char *inductions[] = {
                "Naturally aspirated", "Turbocharger", "Twin turbochargers", "Sequential turbochargers",
                "Roots supercharger", "Twin-screw supercharger", "Centrifugal supercharger" };
            if (ImGui::Combo("##induction", &induction, inductions, 7)) {
                d.induction = static_cast<EngineDesign::Induction>(induction);
                if (d.isBoosted() && d.compressionRatio > 10.5 && !d.isRotary()) d.compressionRatio = 9.5;
                if (d.isBoosted() && d.lambda > 0.9) d.lambda = 0.85;
            }

            if (d.isBoosted()) {
                const double unit = imperial ? 6894.757 : 1e5;
                label(d.isTurbocharged() ? "Boost (wastegate)" : "Target boost");
                sliderDouble("##boost", &d.boost, 0.2e5, 3.5e5, imperial ? "%.1f psi" : "%.2f bar", 1.0 / unit);
                if (!d.isTurbocharged()) {
                    ImGui::SameLine(); ui::HelpMarker("Superchargers have no wastegate: the drive ratio and the "
                        "compressor size are chosen for about this boost near the redline.");
                }

                label("Compressor size");
                sliderDouble("##scale", &d.compressorScale, 0.5, 2.0, "%.2fx");
                ImGui::SameLine(); ui::HelpMarker("Relative to a compressor sized for the boost at the redline. "
                    "Larger compressors flow more but spool later.");

                if (d.isTurbocharged()) {
                    label("Turbine housing A/R");
                    sliderDouble("##ar", &d.turbineAr, 0.35, 1.4, "%.2f");
                    ImGui::SameLine(); ui::HelpMarker("Smaller housings spool sooner but restrict the exhaust at high speed.");
                    label("Ball bearings");
                    ImGui::Checkbox("##ball", &d.ballBearings);
                }
                else {
                    label("Drive ratio");
                    bool autoRatio = d.driveRatio <= 0;
                    if (ImGui::Checkbox("Automatic##ratio", &autoRatio)) {
                        d.driveRatio = autoRatio ? 0.0 : (d.induction == EngineDesign::Induction::Centrifugal ? 10.0 : 2.2);
                    }
                    if (!autoRatio) {
                        label("  Compressor / crank speed");
                        sliderDouble("##ratio", &d.driveRatio, 0.8,
                            (d.induction == EngineDesign::Induction::Centrifugal) ? 20.0 : 4.0, "%.2f : 1");
                    }
                }

                if (d.induction != EngineDesign::Induction::Roots && d.induction != EngineDesign::Induction::TwinScrew) {
                    label("Blow-off valve");
                    ImGui::Checkbox("##bov", &d.blowoffValve);
                }

                label("Intercooler effectiveness");
                sliderDouble("##intercooler", &d.intercoolerEffectiveness, 0.0, 0.95, "%.0f %%", 100.0);

                if (d.compressionRatio > 10.5 && !d.isRotary()) {
                    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(ui::WarningColor()),
                        "Boosted engines usually run 8.5-10.5 : 1 compression (knock isn't simulated).");
                }
            }
            else {
                ImGui::TextWrapped("Turbochargers are driven by the exhaust and need time to spool up; "
                    "superchargers are driven by the crankshaft and boost immediately. "
                    "Pick one above to set it up.");
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Ignition & fuel")) {
            label("Idle advance");
            sliderDouble("##idleadv", &d.idleAdvance, 0.0, 30 * Deg, "%.0f deg BTDC", 1.0 / Deg);
            label("Maximum advance");
            sliderDouble("##maxadv", &d.maxAdvance, d.idleAdvance, 50 * Deg, "%.0f deg BTDC", 1.0 / Deg);
            label("Full advance at");
            sliderDouble("##advrpm", &d.maxAdvanceRpm, 1500, 12000, "%.0f rpm");
            label("Redline");
            sliderDouble("##redline", &d.redline, 2000, 18000, "%.0f rpm");

            ImGui::SeparatorText("Physics");
            label("Engineering model");
            ImGui::Checkbox("##engineering", &d.engineering);
            ImGui::SameLine();
            ui::HelpMarker("Real-gas thermodynamics, flame propagation, heat transfer and friction models. "
                "Slower than the legacy model.");
            label("Fuel");
            const char *fuels[] = { "gasoline", "e85", "ethanol", "methanol", "iso-octane", "propane", "methane", "hydrogen" };
            int fuel = 0;
            for (int i = 0; i < 8; ++i) if (d.fuel == fuels[i]) fuel = i;
            if (ImGui::Combo("##fuel", &fuel, fuels, 8)) d.fuel = fuels[fuel];
            label("Mixture (lambda)");
            sliderDouble("##lambda", &d.lambda, 0.7, 1.4, "%.2f");

            ImGui::SeparatorText("Vehicle");
            label("Vehicle mass");
            sliderDouble("##vehicle", &d.vehicleMass, 300, 5000, "%.0f kg");
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::Separator();
    if (ImGui::Button("Build and run")) generateDesign(true);
    ImGui::SameLine();
    if (ImGui::Button("Save")) generateDesign(false);
    ImGui::SameLine();
    if (ImGui::Button("Save as...")) {
        std::snprintf(m_saveAsName, sizeof(m_saveAsName), "%s", d.name.c_str());
        ImGui::OpenPopup("Save engine as");
    }
    ImGui::SameLine();
    if (ImGui::Button("Copy script")) ImGui::SetClipboardText(generateEngineScript(m_design).c_str());
    ImGui::SameLine();
    if (ImGui::Button("Library...")) m_showLibrary = true;
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        m_design = EngineDesign();
        m_designPath.clear();
        m_designStatus.clear();
    }

    if (ImGui::BeginPopupModal("Save engine as", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Name");
        ImGui::SetNextItemWidth(320 * s);
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        const bool enter = ImGui::InputText("##saveas", m_saveAsName, sizeof(m_saveAsName), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::TextDisabled("Saved as %s.mr in My engines", designFileName(m_saveAsName).c_str());
        if (ImGui::Button("Save", ImVec2(120 * s, 0)) || enter) {
            d.name = m_saveAsName;
            generateDesign(false);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120 * s, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (!m_designStatus.empty()) {
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(m_designStatusError ? ui::DangerColor() : ui::OkColor()),
            "%s", m_designStatus.c_str());
    }
    else {
        ImGui::TextDisabled("Designs are saved as engine scripts (My engines) that can be edited by hand.");
    }

    ImGui::End();
}

void AppUi::buildTuningWindow() {
    const float s = m_preferences.uiScale;
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(520 * s, 520 * s), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Tuning", &m_showTuning)) {
        ImGui::End();
        return;
    }

    if (m_engine == nullptr || m_simulator == nullptr) {
        ImGui::TextDisabled("No engine");
        ImGui::End();
        return;
    }

    constexpr double Deg = constants::pi / 180.0;
    const TuningDefaults &defaults = m_tuningDefaults;
    const float labelWidth = 200 * s;
    auto label = [&](const char *text) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(text);
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-FLT_MIN);
    };

    ImGui::TextWrapped("Changes apply immediately and last until the engine is reloaded. "
        "Edit the script or use the designer to make them permanent.");

    IgnitionModule *ignition = m_engine->getIgnitionModule();

    ImGui::SeparatorText("Ignition");
    {
        double offset = ignition->getTimingOffset();
        label("Timing offset");
        if (sliderDouble("##timing", &offset, -20 * Deg, 20 * Deg, "%+.1f deg", 1.0 / Deg)) ignition->setTimingOffset(offset);

        double limit = ignition->getRevLimit();
        label("Rev limiter");
        if (sliderDouble("##revlimit", &limit, units::rpm(1000), units::rpm(20000), "%.0f rpm", 1.0 / units::rpm(1))) {
            ignition->setRevLimit(limit);
        }
    }

    if (m_engine->isEngineering()) {
        ImGui::SeparatorText("Mixture and combustion");
        double lambda = (m_engine->getIntakeCount() > 0) ? m_engine->getIntake(0)->getLambda() : 1.0;
        label("Lambda");
        if (sliderDouble("##lambda", &lambda, 0.6, 1.6, "%.2f")) {
            for (int i = 0; i < m_engine->getIntakeCount(); ++i) m_engine->getIntake(i)->setLambda(lambda);
        }

        double efficiency = m_engine->getFuel()->getCombustionEfficiency();
        label("Combustion efficiency");
        if (sliderDouble("##efficiency", &efficiency, 0.80, 1.0, "%.3f")) m_engine->getFuel()->setCombustionEfficiency(efficiency);

        ImGui::SeparatorText("Heat transfer");
        double wall = m_engine->getWallTemperature();
        label("Wall temperature");
        if (sliderDouble("##wall", &wall, 300, 600, "%.0f K")) m_engine->setWallTemperature(wall);

        double heat = m_engine->getHeatTransferMultiplier();
        label("Heat transfer multiplier");
        if (sliderDouble("##heat", &heat, 0.0, 3.0, "%.2fx")) m_engine->setHeatTransferMultiplier(heat);
    }

    if (m_engine->usesFrictionModel()) {
        ImGui::SeparatorText("Friction");
        double friction = m_engine->getFrictionMultiplier();
        label("Friction multiplier");
        if (sliderDouble("##friction", &friction, 0.0, 3.0, "%.2fx")) m_engine->setFrictionMultiplier(friction);
    }

    if (ForcedInduction *fi = m_engine->getForcedInduction()) {
        ImGui::SeparatorText(fi->getDescription().c_str());
        const bool imperial = (pressureUnits() == "inHg" || pressureUnits() == "psi");
        if (fi->isTurbocharger()) {
            double boost = fi->getBoostTarget();
            label("Boost target (wastegate)");
            const double unit = imperial ? units::pressure(1.0, units::psi) : units::pressure(1.0, units::bar);
            if (sliderDouble("##boost", &boost, 0.0, units::pressure(3.0, units::bar), imperial ? "%.1f psi" : "%.2f bar", 1.0 / unit)) {
                fi->setBoostTarget(boost);
            }
        }
        else {
            double ratio = fi->getDriveRatio();
            label("Drive ratio");
            const double maxRatio = (fi->getType() == ForcedInduction::Type::Centrifugal) ? 20.0 : 4.0;
            if (sliderDouble("##driveratio", &ratio, 0.5, maxRatio, "%.2f : 1")) fi->setDriveRatio(ratio);
        }

        double intercooler = fi->getIntercoolerEffectiveness();
        label("Intercooler effectiveness");
        if (sliderDouble("##intercooler", &intercooler, 0.0, 1.0, "%.0f %%", 100.0)) fi->setIntercoolerEffectiveness(intercooler);

        if (!fi->isPositiveDisplacement()) {
            bool blowoff = fi->hasBlowoffValve();
            label("Blow-off valve");
            if (ImGui::Checkbox("##blowoff", &blowoff)) fi->setBlowoffValve(blowoff);
        }
    }

    ImGui::SeparatorText("Sound");
    Synthesizer::AudioParameters audio = m_simulator->synthesizer().getAudioParameters();
    bool audioChanged = false;
    label("Volume [Z]"); audioChanged |= ImGui::SliderFloat("##volume", &audio.volume, 0.0f, 1.0f, "%.2f");
    label("Convolution [X]"); audioChanged |= ImGui::SliderFloat("##convolution", &audio.convolution, 0.0f, 1.0f, "%.2f");
    label("High frequency gain [C]"); audioChanged |= ImGui::SliderFloat("##dff", &audio.dF_F_mix, 0.0f, 0.1f, "%.4f");
    label("Low frequency noise [V]"); audioChanged |= ImGui::SliderFloat("##airnoise", &audio.airNoise, 0.0f, 1.0f, "%.2f");
    label("High frequency noise [B]"); audioChanged |= ImGui::SliderFloat("##inputnoise", &audio.inputSampleNoise, 0.0f, 1.0f, "%.2f");
    if (m_engine->getForcedInduction() != nullptr) {
        label("Turbo / supercharger sound"); audioChanged |= ImGui::SliderFloat("##fisound", &audio.forcedInductionVolume, 0.0f, 3.0f, "%.2f");
    }
    if (audioChanged) m_simulator->synthesizer().setAudioParameters(audio);

    ImGui::SeparatorText("Simulation");
    {
        int frequency = m_simulator->getSimulationFrequency();
        label("Frequency [N]");
        if (ImGui::SliderInt("##frequency", &frequency, 2000, 50000, "%d Hz", ImGuiSliderFlags_Logarithmic)) {
            m_simulator->setSimulationFrequency(frequency);
        }
    }

    ImGui::Separator();
    if (ImGui::Button("Restore script values")) {
        ignition->setTimingOffset(defaults.timingOffset);
        ignition->setRevLimit(defaults.revLimit);
        for (int i = 0; i < m_engine->getIntakeCount(); ++i) m_engine->getIntake(i)->setLambda(defaults.lambda);
        m_engine->getFuel()->setCombustionEfficiency(defaults.combustionEfficiency);
        m_engine->setWallTemperature(defaults.wallTemperature);
        m_engine->setHeatTransferMultiplier(defaults.heatTransferMultiplier);
        m_engine->setFrictionMultiplier(defaults.frictionMultiplier);
        m_simulator->setSimulationFrequency(m_preferences.simulationFrequency > 0
            ? m_preferences.simulationFrequency
            : defaults.simulationFrequency);

        Synthesizer::AudioParameters restored = m_simulator->synthesizer().getAudioParameters();
        restored.volume = defaults.volume;
        restored.convolution = defaults.convolution;
        restored.dF_F_mix = defaults.highFrequencyGain;
        restored.airNoise = defaults.airNoise;
        restored.inputSampleNoise = defaults.inputNoise;
        restored.forcedInductionVolume = 1.0f;
        m_simulator->synthesizer().setAudioParameters(restored);

        if (ForcedInduction *fi = m_engine->getForcedInduction()) {
            fi->setBoostTarget(defaults.boostTarget);
            fi->setDriveRatio(defaults.driveRatio);
            fi->setIntercoolerEffectiveness(defaults.intercoolerEffectiveness);
            fi->setBlowoffValve(defaults.blowoffValve);
        }
    }

    ImGui::End();
}

void AppUi::buildHelpWindow() {
    const float s = m_preferences.uiScale;
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(560 * s, 560 * s), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Controls", &m_showHelp)) {
        ImGui::End();
        return;
    }

    struct Key { const char *key; const char *action; };
    const Key keys[] = {
        { "A", "Toggle ignition" },
        { "S (hold)", "Starter motor" },
        { "Q / W / E / R (hold)", "Throttle 1 / 10 / 20 / 100 %" },
        { "Space + wheel", "Fine throttle" },
        { "Up / Down", "Shift up / down" },
        { "Shift (hold)", "Clutch in" },
        { "T / U", "Release / engage the clutch slowly" },
        { ". (hold)", "Brake" },
        { "D", "Toggle the dynamometer" },
        { "H", "Hold the dyno speed" },
        { "G + wheel", "Change the held dyno speed" },
        { "1 - 5 (hold)", "Slow motion" },
        { "P / Right", "Pause / step one frame" },
        { "Z, X, C, V, B + wheel", "Volume, convolution, high frequency gain, low / high frequency noise" },
        { "N + wheel", "Simulation frequency" },
        { "M / ,", "Next / previous drawing layer" },
        { "Mouse drag / wheel", "Pan and zoom the engine view" },
        { "F1 / F2 / F3", "Rotate the engine view / reset" },
        { "Tab", "Cycle the panel layout" },
        { "F", "Fullscreen" },
        { "PgUp / PgDn", "Previous / next engine" },
        { "[ / ]", "Previous / next theme script" },
        { "Enter", "Reload the engine script" },
        { "F5", "Console" },
        { "F8", "Check for / install updates" },
        { SettingsShortcut, "Settings" },
        { LibraryShortcut, "Engine library (presets and saved engines)" },
        { DesignerShortcut, "Engine designer" },
        { "Esc", "Quit" },
    };

    if (ImGui::BeginTable("##keys", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerH)) {
        ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, 190 * s);
        ImGui::TableSetupColumn("Action");
        ImGui::TableHeadersRow();
        for (const Key &key : keys) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(ui::AccentColor()), "%s", key.key);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextWrapped("%s", key.action);
        }
        ImGui::EndTable();
    }

    ImGui::End();
}

void AppUi::buildAboutWindow() {
    const float s = m_preferences.uiScale;
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(480 * s, 0), ImGuiCond_Always);
    if (!ImGui::Begin("About", &m_showAbout, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    ImGui::Text("Engine Simulator");
    ImGui::TextDisabled("Version %s, built %s", EngineSimApplication::getBuildVersion().c_str(), __DATE__);
    ImGui::Spacing();
    ImGui::TextWrapped("Real-time internal combustion engine simulation with engine sound synthesis "
        "and engineering outputs.");
    ImGui::Spacing();
    if (ImGui::Button("github.com/kyan0s-git/engine-sim")) SDL_OpenURL("https://github.com/kyan0s-git/engine-sim");
    ImGui::Spacing();
    ImGui::TextDisabled("Based on Engine Simulator by Ange Yaghi. MIT license.");
    ImGui::TextDisabled("Uses SDL2, Dear ImGui, ImPlot and Piranha.");
    ImGui::End();
}
