// Toolbar, beginner controls, ECU tuning, launch test, welcome screen and the
// pictures of the design settings.

#include "../../include/ui/app_ui.h"

#include "../../include/ui/ui_widgets.h"
#include "../../include/engine_sim_application.h"
#include "../../include/engine.h"
#include "../../include/ecu.h"
#include "../../include/forced_induction.h"
#include "../../include/simulator.h"
#include "../../include/transmission.h"
#include "../../include/vehicle.h"
#include "../../include/units.h"
#include "../../include/constants.h"

#include <imgui.h>
#include <implot.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace {

    constexpr double Pi = constants::pi;
    constexpr double Deg = Pi / 180.0;

    bool sliderDouble(const char *label, double *v, double min, double max, const char *format, double scale = 1.0) {
        float f = static_cast<float>(*v * scale);
        const bool changed = ImGui::SliderFloat(label, &f, (float)(min * scale), (float)(max * scale), format);
        if (changed) *v = f / scale;
        return changed;
    }

    ImU32 colorU32(float r, float g, float b, float a = 1.0f) {
        return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a));
    }

    // A row of mutually exclusive buttons; returns true when the choice changes
    bool segmented(const char *id, int *value, const char *const *labels, int count, float width) {
        bool changed = false;
        ImGui::PushID(id);
        const float spacing = ImGui::GetStyle().ItemSpacing.x * 0.25f;
        const float w = (width - spacing * (count - 1)) / count;
        for (int i = 0; i < count; ++i) {
            if (i > 0) ImGui::SameLine(0, spacing);
            const bool selected = (*value == i);
            if (selected) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::ColorConvertU32ToFloat4(ui::AccentColor(0.85f)));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::ColorConvertU32ToFloat4(ui::AccentColor(1.0f)));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05f, 0.05f, 0.05f, 1));
            }
            if (ImGui::Button(labels[i], ImVec2(w, 0)) && !selected) {
                *value = i;
                changed = true;
            }
            if (selected) ImGui::PopStyleColor(3);
        }
        ImGui::PopID();
        return changed;
    }

    std::string seconds(double t) {
        if (t < 0) return "-";
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%.2f s", t);
        return buffer;
    }

} /* namespace */

// Toolbar ---------------------------------------------------------------------

void AppUi::buildToolbar(float y, float height) {
    const float s = m_preferences.uiScale;
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, y));
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, height));
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus
        | ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8 * s, 5 * s));
    if (!ImGui::Begin("##toolbar", nullptr, flags)) {
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    if (m_engine != nullptr && m_simulator != nullptr) {
        IgnitionModule *ignition = m_engine->getIgnitionModule();
        const bool running = ignition->m_enabled && m_engine->getRpm() > 300;

        // Start / stop
        if (m_autoStarting) {
            ImGui::BeginDisabled();
            ImGui::Button("Starting...", ImVec2(130 * s, 0));
            ImGui::EndDisabled();
        }
        else if (running) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::ColorConvertU32ToFloat4(ui::DangerColor(0.75f)));
            if (ImGui::Button("Stop engine", ImVec2(130 * s, 0))) {
                ignition->m_enabled = false;
                m_controls.throttle = 0;
                log("Ignition off");
            }
            ImGui::PopStyleColor();
        }
        else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::ColorConvertU32ToFloat4(ui::OkColor(0.75f)));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05f, 0.05f, 0.05f, 1));
            if (ImGui::Button("Start engine", ImVec2(130 * s, 0))) {
                m_autoStarting = true;
                m_autoStartTimer = 0;
            }
            ImGui::PopStyleColor(2);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Ignition on and the starter held until the engine runs ([A] and [S])");

        ImGui::SameLine();
        Ecu &ecu = m_simulator->getEcu();
        bool automatic = ecu.settings().enabled && ecu.settings().automatic;
        if (ImGui::Checkbox("Automatic gearbox", &automatic)) {
            ecu.settings().enabled = true;
            ecu.settings().automatic = automatic;
            log(automatic ? "Automatic gearbox on" : "Automatic gearbox off (shift with the arrow keys)");
        }

        ImGui::SameLine();
        ImGui::Checkbox("Dyno", &m_simulator->m_dyno.m_enabled);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Dynamometer [D]: loads the engine to measure torque and power");
    }

    ImGui::SameLine(0, 18 * s);
    if (ImGui::Button("Engines")) m_showLibrary = !m_showLibrary;
    ImGui::SameLine();
    if (ImGui::Button("Designer")) m_showDesigner = !m_showDesigner;
    ImGui::SameLine();
    if (ImGui::Button("ECU tuning")) m_showEcu = !m_showEcu;
    ImGui::SameLine();
    if (ImGui::Button("Launch test")) m_showLaunchTest = !m_showLaunchTest;
    if (!isBeginner()) {
        ImGui::SameLine();
        if (ImGui::Button("Tuning")) m_showTuning = !m_showTuning;
    }

    // Right side: update status and the interface mode
    const Updater &updater = m_app->getUpdater();
    const Updater::State state = updater.getState();
    const float modeWidth = 200 * s;
    float right = ImGui::GetWindowWidth() - modeWidth - 10 * s;

    std::string update;
    const char *action = nullptr;
    if (state == Updater::State::Available) {
        update = "Version " + updater.getRelease().version + " is available";
        action = updater.canInstall() ? "Install" : "Download";
    }
    else if (state == Updater::State::Downloading) {
        update = "Downloading " + updater.getRelease().version + "...";
    }
    else if (state == Updater::State::ReadyToRestart) {
        update = updater.usesInstaller() ? "Installer downloaded" : "Update ready";
        action = updater.usesInstaller() ? "Run installer" : "Restart now";
    }
    else if (state == Updater::State::Failed) {
        update = "Update failed";
        action = "Retry";
    }

    if (!update.empty()) {
        const float textWidth = ImGui::CalcTextSize(update.c_str()).x;
        const float buttonWidth = (action != nullptr) ? ImGui::CalcTextSize(action).x + 20 * s : 0;
        right -= textWidth + buttonWidth + 20 * s;
        ImGui::SameLine(std::max(right, ImGui::GetCursorPosX() + 10 * s));
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(state == Updater::State::Failed ? ui::DangerColor() : ui::AccentColor()), "%s", update.c_str());
        if (state == Updater::State::Failed && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", updater.getError().c_str());
        if (action != nullptr) {
            ImGui::SameLine();
            if (ImGui::Button(action)) m_app->requestUpdateAction();
        }
    }

    ImGui::SameLine(ImGui::GetWindowWidth() - modeWidth - 8 * s);
    int mode = isBeginner() ? 0 : 1;
    const char *modes[] = { "Beginner", "Advanced" };
    if (segmented("##mode", &mode, modes, 2, modeWidth)) {
        m_preferences.uiMode = mode ? "advanced" : "beginner";
        m_layout = mode ? Layout::Full : Layout::Instruments;
        m_preferencesDirty = true;
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

void AppUi::autoStart(double dt) {
    if (!m_autoStarting || m_engine == nullptr || m_simulator == nullptr) {
        m_autoStarting = false;
        return;
    }

    m_engine->getIgnitionModule()->m_enabled = true;
    m_autoStartTimer += dt;

    const double startedRpm = std::max(450.0, 1.6 * units::toRpm(m_engine->getStarterSpeed()));
    const bool started = m_engine->getRpm() > startedRpm && m_autoStartTimer > 0.4;
    if (started || m_autoStartTimer > 6.0) {
        m_autoStarting = false;
        m_controls.starterHeld = false;
        log(started ? "Engine started" : "The engine didn't start; check the ignition timing and the fuel");
    }
    else {
        m_controls.starterHeld = true;
    }
}

// Beginner controls -----------------------------------------------------------

void AppUi::buildBeginnerControls() {
    const float s = m_preferences.uiScale;
    ImGui::PushFont(m_font);
    ImGui::TextWrapped("%s", m_engine->getName().c_str());
    ImGui::PopFont();

    const ForcedInduction *fi = m_engine->getForcedInduction();
    ImGui::TextDisabled("%.1f L, %d %s%s", units::convert(m_engine->getDisplacement(), units::L),
        m_engine->getCylinderCount(), "cylinders",
        fi != nullptr ? (", " + fi->getDescription()).c_str() : "");

    ImGui::Spacing();
    ImGui::SeparatorText("Throttle");
    float throttle = static_cast<float>(m_controls.throttle * 100.0);
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::SliderFloat("##throttle", &throttle, 0.0f, 100.0f, "%.0f %%")) {
        m_controls.throttle = throttle / 100.0;
    }
    const double presets[] = { 0.0, 0.25, 0.5, 1.0 };
    const char *names[] = { "Idle", "25%", "50%", "Full" };
    const float w = (ImGui::GetContentRegionAvail().x - 3 * ImGui::GetStyle().ItemSpacing.x) / 4;
    for (int i = 0; i < 4; ++i) {
        if (i > 0) ImGui::SameLine();
        if (ImGui::Button(names[i], ImVec2(w, 0))) m_controls.throttle = presets[i];
    }
    ImGui::TextDisabled("Keys: Q/W/E/R, or hold Space and scroll");

    ImGui::SeparatorText("Driving");
    Transmission *transmission = m_simulator->getTransmission();
    Ecu &ecu = m_simulator->getEcu();
    const bool automatic = ecu.settings().enabled && ecu.settings().automatic;
    if (transmission != nullptr) {
        const int gear = transmission->getGear();
        ImGui::Text("Gear: %s", gear < 0 ? "neutral" : std::to_string(gear + 1).c_str());
        if (automatic) {
            ImGui::TextDisabled("The automatic gearbox picks the gear.");
        }
        else {
            if (ImGui::Button("Shift down", ImVec2((ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2, 0))) ecu.requestShift(-1);
            ImGui::SameLine();
            if (ImGui::Button("Shift up", ImVec2(-FLT_MIN, 0))) ecu.requestShift(1);
            ImGui::TextDisabled("Arrow keys shift; hold Shift for the clutch.");
        }
    }

    ImGui::Button("Hold to brake", ImVec2(-FLT_MIN, 30 * s));
    m_controls.brakeHeld = ImGui::IsItemActive();

    ImGui::SeparatorText("Measure");
    ImGui::Checkbox("Dynamometer", &m_simulator->m_dyno.m_enabled);
    ui::HelpMarker("Holds the engine to measure its torque and power. Put the gearbox in neutral, "
        "open the throttle and the dyno sweeps up through the speed range; see the Dyno tab in Advanced mode.");
    if (ImGui::Button("Launch test...", ImVec2(-FLT_MIN, 0))) m_showLaunchTest = true;

    ImGui::SeparatorText("Make it yours");
    if (ImGui::Button("Pick another engine...", ImVec2(-FLT_MIN, 0))) m_showLibrary = true;
    if (ImGui::Button("Design an engine...", ImVec2(-FLT_MIN, 0))) m_showDesigner = true;
    if (ImGui::Button("Tune the ECU...", ImVec2(-FLT_MIN, 0))) m_showEcu = true;

    ImGui::Spacing();
    ImGui::TextDisabled("Switch to Advanced (top right) for the analysis\nplots, the clutch, dyno hold and all settings.");
}

// ECU -----------------------------------------------------------------------

std::string AppUi::tunePath() const {
    const fs::path script(m_app->getEngineScriptPath());
    const std::string stem = script.stem().string().empty() ? "engine" : script.stem().string();
    return (fs::path(m_app->getUserDataPath()) / "tunes" / (stem + ".ecu")).string();
}

void AppUi::saveTune() {
    if (m_simulator == nullptr) return;

    std::error_code ec;
    const fs::path path = tunePath();
    fs::create_directories(path.parent_path(), ec);
    std::ofstream file(path, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        m_ecuStatus = "Could not write " + path.string();
        return;
    }

    file << m_simulator->getEcu().serialize();
    m_ecuStatus = "Saved " + path.filename().string() + "; it loads with this engine";
    log(m_ecuStatus);
}

bool AppUi::loadTune(bool quiet) {
    if (m_simulator == nullptr) return false;

    std::ifstream file(tunePath());
    if (!file.is_open()) {
        if (!quiet) m_ecuStatus = "No saved tune for this engine";
        return false;
    }

    std::stringstream ss;
    ss << file.rdbuf();
    std::string error;
    if (!m_simulator->getEcu().deserialize(ss.str(), &error)) {
        m_ecuStatus = "The saved tune could not be read: " + error;
        log(m_ecuStatus);
        return false;
    }

    m_ecuStatus = "Loaded the saved tune";
    if (!quiet) log(m_ecuStatus);
    return true;
}

void AppUi::buildMapEditor(
    EcuTable &table,
    EcuTable *stock,
    float height,
    double cursorX,
    double cursorY,
    double minValue,
    double maxValue,
    double step,
    const char *format)
{
    const float s = m_preferences.uiScale;
    const bool twoD = (table.rows() > 1);
    ImGui::PushID(table.name.c_str());

    // Picture: heat map (2D) or curve (1D) with the operating point
    if (twoD) {
        std::vector<double> flipped(table.v.size());
        for (int r = 0; r < table.rows(); ++r) {
            for (int c = 0; c < table.cols(); ++c) {
                flipped[static_cast<size_t>(table.rows() - 1 - r) * table.cols() + c] = table.at(r, c);
            }
        }

        double lo = 1e300, hi = -1e300;
        for (double v : table.v) { lo = std::min(lo, v); hi = std::max(hi, v); }
        if (hi - lo < 1e-6) hi = lo + 1;

        ImPlot::PushColormap(ImPlotColormap_Plasma);
        if (ImPlot::BeginPlot("##heat", ImVec2(-80 * s, height), ImPlotFlags_NoLegend | ImPlotFlags_NoMouseText)) {
            ImPlot::SetupAxes(table.xLabel.c_str(), table.yLabel.c_str(), ImPlotAxisFlags_Lock, ImPlotAxisFlags_Lock);
            ImPlot::SetupAxesLimits(table.x.front(), table.x.back(), table.y.front(), table.y.back(), ImPlotCond_Always);
            const double dx = (table.x.back() - table.x.front()) / std::max(table.cols() - 1, 1);
            const double dy = (table.y.back() - table.y.front()) / std::max(table.rows() - 1, 1);
            ImPlot::PlotHeatmap("##map", flipped.data(), table.rows(), table.cols(), lo, hi, nullptr,
                ImPlotPoint(table.x.front() - dx / 2, table.y.front() - dy / 2),
                ImPlotPoint(table.x.back() + dx / 2, table.y.back() + dy / 2));
            const double px = std::clamp(cursorX, table.x.front(), table.x.back());
            const double py = std::clamp(cursorY, table.y.front(), table.y.back());
            ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, 7 * s, ImVec4(1, 1, 1, 1), 2 * s, ImVec4(0, 0, 0, 1));
            ImPlot::PlotScatter("Now", &px, &py, 1);
            ImPlot::EndPlot();
        }
        ImGui::SameLine();
        ImPlot::ColormapScale("##scale", lo, hi, ImVec2(70 * s, height), format);
        ImPlot::PopColormap();
    }
    else {
        if (ImPlot::BeginPlot("##curve", ImVec2(-1, height), ImPlotFlags_NoLegend)) {
            ImPlot::SetupAxes(table.xLabel.c_str(), table.unit.c_str(), ImPlotAxisFlags_Lock, ImPlotAxisFlags_AutoFit);
            ImPlot::SetupAxisLimits(ImAxis_X1, table.x.front(), table.x.back(), ImPlotCond_Always);
            ImPlot::PlotLine("##line", table.x.data(), table.v.data(), table.cols());
            if (stock != nullptr && stock->v.size() == table.v.size()) {
                ImPlot::SetNextLineStyle(ImVec4(0.6f, 0.6f, 0.6f, 0.6f));
                ImPlot::PlotLine("##stock", stock->x.data(), stock->v.data(), stock->cols());
            }
            for (int c = 0; c < table.cols(); ++c) {
                double x = table.x[c], y = table.at(0, c);
                if (ImPlot::DragPoint(c, &x, &y, ImGui::ColorConvertU32ToFloat4(ui::AccentColor()), 5 * s)) {
                    table.at(0, c) = std::clamp(std::round(y / step) * step, minValue, maxValue);
                }
            }
            const double py = table.sample(cursorX);
            ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, 6 * s, ImVec4(1, 1, 1, 1), 2 * s);
            ImPlot::PlotScatter("Now", &cursorX, &py, 1);
            ImPlot::EndPlot();
        }
        ImGui::TextDisabled("Drag the points, or edit the values below. The grey line is the stock tune.");
    }

    // Whole-map adjustments
    if (ImGui::Button("-")) for (double &v : table.v) v = std::clamp(v - step, minValue, maxValue);
    ImGui::SameLine();
    if (ImGui::Button("+")) for (double &v : table.v) v = std::clamp(v + step, minValue, maxValue);
    ImGui::SameLine();
    ImGui::TextDisabled("whole map by %g", step);
    if (stock != nullptr) {
        ImGui::SameLine();
        if (ImGui::Button("Reset to stock")) table = *stock;
    }

    // Value grid
    const int cols = table.cols();
    const ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY
        | ImGuiTableFlags_SizingFixedFit;
    const float gridHeight = twoD ? std::min(ImGui::GetContentRegionAvail().y, (table.rows() + 2) * ImGui::GetFrameHeightWithSpacing()) : ImGui::GetFrameHeightWithSpacing() * 2.6f;
    if (ImGui::BeginTable("##grid", cols + 1, flags, ImVec2(-1, std::max(gridHeight, 60 * s)))) {
        ImGui::TableSetupScrollFreeze(1, 1);
        ImGui::TableSetupColumn(twoD ? "kPa \\ rpm" : "", ImGuiTableColumnFlags_WidthFixed, 70 * s);
        for (int c = 0; c < cols; ++c) {
            char header[32];
            std::snprintf(header, sizeof(header), "%.0f", table.x[c]);
            ImGui::TableSetupColumn(header, ImGuiTableColumnFlags_WidthFixed, 52 * s);
        }
        ImGui::TableHeadersRow();

        const double ex = EcuTable::position(table.x, cursorX);
        const double ey = twoD ? EcuTable::position(table.y, cursorY) : 0.0;
        for (int r = table.rows() - 1; r >= 0; --r) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (twoD) ImGui::Text("%.0f", table.y[r]);
            else ImGui::TextUnformatted(table.unit.c_str());

            for (int c = 0; c < cols; ++c) {
                ImGui::TableSetColumnIndex(c + 1);
                const bool near = std::abs(c - ex) < 0.75 && std::abs(r - ey) < 0.75;
                if (near) ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, ui::AccentColor(0.35f));
                ImGui::PushID(r * 1000 + c);
                ImGui::SetNextItemWidth(-FLT_MIN);
                float v = static_cast<float>(table.at(r, c));
                if (ImGui::DragFloat("##v", &v, static_cast<float>(step), static_cast<float>(minValue), static_cast<float>(maxValue), format)) {
                    table.at(r, c) = v;
                }
                ImGui::PopID();
            }
        }
        ImGui::EndTable();
    }

    ImGui::PopID();
}

void AppUi::buildEcuWindow() {
    const float s = m_preferences.uiScale;
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(820 * s, 640 * s), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("ECU tuning", &m_showEcu)) {
        ImGui::End();
        return;
    }

    if (m_engine == nullptr || m_simulator == nullptr || !m_simulator->getEcu().isInitialized()) {
        ImGui::TextDisabled("No engine");
        ImGui::End();
        return;
    }

    Ecu &ecu = m_simulator->getEcu();
    Ecu::Settings &st = ecu.settings();
    const Ecu::Settings &stock = ecu.stock();
    const Ecu::State &state = ecu.state();
    const ForcedInduction *fi = m_engine->getForcedInduction();
    const float labelWidth = 220 * s;
    auto label = [&](const char *text) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(text);
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-FLT_MIN);
    };

    // Live readout
    {
        ImGui::Checkbox("ECU in control", &st.enabled);
        ui::HelpMarker("Off: the engine runs on its script's fixed ignition curve and mixture, "
            "and the gearbox is manual.");
        ImGui::SameLine(0, 20 * s);
        ImGui::TextDisabled("%5.0f rpm   %4.0f kPa   %5.1f deg   lambda %.2f%s%s%s%s",
            state.rpm, state.load, state.advance, state.lambda,
            state.limiting ? "   LIMITER" : "",
            state.launchActive ? "   LAUNCH" : "",
            state.antiLagActive ? "   ANTI-LAG" : "",
            state.overboost ? "   OVERBOOST CUT" : "");
    }

    ImGui::SeparatorText("Ready-made tunes");
    {
        const char *names[] = { "Stock", "Economy", "Performance", "Race" };
        const char *keys[] = { "stock", "economy", "performance", "race" };
        const char *tips[] = {
            "The engine as its script describes it",
            "Lean cruise, more advance at light load, earlier shifts, less boost",
            "More advance, richer at full load, 20% more boost, launch control, quicker shifts",
            "Performance plus a higher rev limit, anti-lag (turbo), flat-out shifts" };
        const float w = (ImGui::GetContentRegionAvail().x - 3 * ImGui::GetStyle().ItemSpacing.x) / 4;
        for (int i = 0; i < 4; ++i) {
            if (i > 0) ImGui::SameLine();
            if (ImGui::Button(names[i], ImVec2(w, 0))) {
                const bool automatic = st.automatic;
                ecu.applyPreset(keys[i]);
                st.automatic = automatic;
                m_ecuStatus = std::string(names[i]) + " tune applied";
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tips[i]);
        }
    }

    if (isBeginner()) {
        // A few safe adjustments instead of the maps
        ImGui::SeparatorText("Quick adjustments");
        double limit = st.revLimit;
        label("Rev limiter");
        if (sliderDouble("##revlimit", &limit, 0.6 * stock.revLimit, 1.25 * stock.revLimit, "%.0f rpm")) st.revLimit = limit;

        if (fi != nullptr && fi->isTurbocharger()) {
            double boost = st.boost.v.empty() ? 0.0 : st.boost.v.back();
            label("Boost");
            const bool imperial = (pressureUnits() == "inHg" || pressureUnits() == "psi");
            const double scale = imperial ? 0.145038 : 0.01;
            if (sliderDouble("##boost", &boost, 0.0, 300.0, imperial ? "%.1f psi" : "%.2f bar", scale)) {
                for (double &v : st.boost.v) v = boost;
            }
        }

        double timing = 0;
        label("Ignition timing");
        ImGui::TextDisabled("%+.1f deg from stock", st.ignition.v.empty() || stock.ignition.v.empty() ? 0.0 : st.ignition.v[0] - stock.ignition.v[0]);
        ImGui::SameLine();
        if (ImGui::SmallButton("-1 deg")) for (double &v : st.ignition.v) v -= 1.0;
        ImGui::SameLine();
        if (ImGui::SmallButton("+1 deg")) for (double &v : st.ignition.v) v += 1.0;
        (void)timing;

        label("Automatic gearbox");
        ImGui::Checkbox("##auto", &st.automatic);
        label("Launch control");
        ImGui::Checkbox("##launch", &st.launchControl);
        ui::HelpMarker("Holds the engine at the launch speed while the car is standing still at full throttle.");
        if (fi != nullptr && fi->isTurbocharger()) {
            label("Anti-lag");
            ImGui::Checkbox("##antilag", &st.antiLag);
            ui::HelpMarker("Keeps the turbo spinning off the throttle by burning late in the exhaust (pops and bangs).");
        }

        // Pictures of what the sliders change: the full-throttle timing
        // curve against stock, and where the gearbox shifts
        ImGui::SeparatorText("What the ECU does");
        const int n = st.ignition.cols();
        if (n > 1 && stock.ignition.cols() == n
            && ImPlot::BeginPlot("##beginnerecu", ImVec2(-1, 200 * s), ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect))
        {
            std::vector<double> now(n), base(n);
            const double fullLoad = st.ignition.y.empty() ? 0.0 : st.ignition.y.back();
            for (int i = 0; i < n; ++i) {
                now[i] = st.ignition.sample(st.ignition.x[i], fullLoad);
                base[i] = stock.ignition.sample(st.ignition.x[i], fullLoad);
            }
            double lo = 1e9, hi = -1e9;
            for (int i = 0; i < n; ++i) {
                lo = std::min({ lo, now[i], base[i] });
                hi = std::max({ hi, now[i], base[i] });
            }
            ImPlot::SetupAxes("Engine speed [rpm]", "Timing at full throttle [deg]");
            ImPlot::SetupAxisLimits(ImAxis_X1, 0, std::max(st.ignition.x.back(), st.revLimit * 1.05), ImPlotCond_Always);
            ImPlot::SetupAxisLimits(ImAxis_Y1, lo - 3, hi + 3, ImPlotCond_Always);
            ImPlot::SetupLegend(ImPlotLocation_SouthEast);
            ImPlot::SetNextLineStyle(ImVec4(0.6f, 0.6f, 0.6f, 1), 1.0f);
            ImPlot::PlotLine("Stock", st.ignition.x.data(), base.data(), n);
            ImPlot::SetNextLineStyle(ImGui::ColorConvertU32ToFloat4(ui::AccentColor()), 2.0f);
            ImPlot::PlotLine("Now", st.ignition.x.data(), now.data(), n);

            const double limit[] = { st.revLimit };
            ImPlot::SetNextLineStyle(ImGui::ColorConvertU32ToFloat4(ui::DangerColor()), 1.5f);
            ImPlot::PlotInfLines("Rev limit", limit, 1);
            if (st.automatic) {
                const double shift = std::min(st.upshift.sample(100.0), st.revLimit - 50);
                ImPlot::SetNextLineStyle(ImGui::ColorConvertU32ToFloat4(ui::OkColor()), 1.5f);
                ImPlot::PlotInfLines("Full-throttle upshift", &shift, 1);
            }
            if (state.rpm > 0) {
                const double x = state.rpm, y = state.advance;
                ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, 5, ImGui::ColorConvertU32ToFloat4(ui::AccentColor()));
                ImPlot::PlotScatter("Engine", &x, &y, 1);
            }
            ImPlot::EndPlot();
        }

        ImGui::TextDisabled("Switch to Advanced mode to edit the ignition, fuel and boost maps.");
    }
    else if (ImGui::BeginTabBar("##ecu")) {
        if (ImGui::BeginTabItem("Ignition")) {
            ImGui::Checkbox("Use the ignition map", &st.useIgnitionMap);
            ImGui::SameLine();
            ImGui::TextDisabled("degrees before TDC over engine speed and manifold pressure");
            buildMapEditor(st.ignition, const_cast<EcuTable *>(&stock.ignition), 230 * s, state.rpm, state.load, -20, 70, 0.5, "%.1f");
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Fuel")) {
            ImGui::Checkbox("Use the fuel map", &st.useFuelMap);
            ImGui::SameLine();
            ImGui::TextDisabled("lambda: 1.0 is stoichiometric, lower is richer");
            if (!m_engine->isEngineering()) {
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(ui::WarningColor()),
                    "This engine uses the legacy physics model, which ignores the mixture.");
            }
            buildMapEditor(st.lambda, const_cast<EcuTable *>(&stock.lambda), 230 * s, state.rpm, state.load, 0.6, 1.6, 0.01, "%.2f");
            ImGui::EndTabItem();
        }

        if (fi != nullptr && fi->isTurbocharger() && ImGui::BeginTabItem("Boost")) {
            ImGui::Checkbox("Use the boost map", &st.useBoostMap);
            ImGui::SameLine();
            ImGui::TextDisabled("target boost (gauge kPa) over engine speed");
            buildMapEditor(st.boost, const_cast<EcuTable *>(&stock.boost), 220 * s, state.rpm, 0, 0, 400, 5, "%.0f");
            label("Overboost protection");
            ImGui::Checkbox("##overboost", &st.overboostCut);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(160 * s);
            sliderDouble("##obmargin", &st.overboostMargin, 10, 100, "cut %.0f kPa over target");
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Limits & launch")) {
            label("Rev limiter");
            sliderDouble("##revlimit", &st.revLimit, 1000, std::max(20000.0, stock.revLimit * 1.4), "%.0f rpm");
            label("Limiter type");
            int limiter = (st.limiter == Ecu::Limiter::Soft) ? 1 : 0;
            const char *limiters[] = { "Spark cut (hard)", "Soft (retard, then cut)" };
            if (ImGui::Combo("##limiter", &limiter, limiters, 2)) st.limiter = limiter ? Ecu::Limiter::Soft : Ecu::Limiter::SparkCut;
            if (st.limiter == Ecu::Limiter::Soft) {
                label("Soft limit band");
                sliderDouble("##band", &st.softBand, 50, 1500, "%.0f rpm");
            }
            label("Cut duration");
            sliderDouble("##cut", &st.cutTime, 0.01, 0.5, "%.2f s");
            ImGui::SeparatorText("Launch control");
            label("Enabled");
            ImGui::Checkbox("##launch", &st.launchControl);
            label("Launch speed");
            sliderDouble("##launchrpm", &st.launchRpm, 1500, st.revLimit, "%.0f rpm");
            ImGui::TextDisabled("Active while the car stands still at more than half throttle with the clutch out\n"
                "(or the automatic gearbox engaged).");
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Idle & anti-lag")) {
            label("Idle speed control");
            ImGui::Checkbox("##idle", &st.idleControl);
            label("Idle speed");
            sliderDouble("##idlerpm", &st.idleRpm, 400, 3000, "%.0f rpm");
            ImGui::TextDisabled("Adds air through the idle bypass when the engine sags below the idle speed.");
            if (fi != nullptr && fi->isTurbocharger()) {
                ImGui::SeparatorText("Anti-lag");
                label("Enabled");
                ImGui::Checkbox("##antilag", &st.antiLag);
                label("Ignition retard");
                sliderDouble("##alretard", &st.antiLagRetard, 0, 40, "%.0f deg after TDC");
                label("Throttle kick");
                sliderDouble("##alair", &st.antiLagAir, 0, 0.4, "%.0f %%", 100.0);
                label("Active above");
                sliderDouble("##almin", &st.antiLagMinRpm, 1000, st.revLimit, "%.0f rpm");
                ImGui::TextDisabled("Off the throttle the late burning keeps exhaust energy (and the boost) up.");
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Gearbox")) {
            label("Automatic gearbox");
            ImGui::Checkbox("##auto", &st.automatic);
            label("Shift time");
            sliderDouble("##shifttime", &st.shiftTime, 0.05, 1.0, "%.2f s");
            label("Launch clutch engaged by");
            sliderDouble("##launchclutch", &st.launchClutchRpm, 300, 4000, "idle + %.0f rpm");
            ImGui::SeparatorText("Shift points over throttle");
            ImGui::BeginGroup();
            ImGui::TextUnformatted("Upshift");
            buildMapEditor(st.upshift, const_cast<EcuTable *>(&stock.upshift), 150 * s, state.throttle * 100, 0, 500, 25000, 50, "%.0f");
            ImGui::EndGroup();
            ImGui::TextUnformatted("Downshift");
            buildMapEditor(st.downshift, const_cast<EcuTable *>(&stock.downshift), 150 * s, state.throttle * 100, 0, 300, 25000, 50, "%.0f");

            ImGui::SeparatorText("Full throttle shift points per gear");
            bool any = false;
            for (size_t g = 0; g < st.wotUpshift.size(); ++g) {
                if (st.wotUpshift[g] > 0) {
                    ImGui::Text("%zu -> %zu at %.0f rpm", g + 1, g + 2, st.wotUpshift[g]);
                    any = true;
                }
            }
            if (!any) ImGui::TextDisabled("Not set: the upshift curve's full throttle value is used.");
            if (ImGui::Button("Optimize from the dyno curve")) {
                std::vector<double> rpm, torque;
                for (int i = 0; i < DynoBins; ++i) {
                    if (!m_dynoValid[i]) continue;
                    rpm.push_back(i * 100.0 + 50.0);
                    torque.push_back(m_dynoTorque[i]);
                }
                if (rpm.size() < 8) {
                    m_ecuStatus = "Record a dyno sweep first (Dyno on, neutral, full throttle)";
                }
                else {
                    ecu.optimizeShiftPoints(rpm, torque);
                    m_ecuStatus = "Shift points set where the next gear gives more wheel torque";
                }
            }
            ui::HelpMarker("Uses the torque curve recorded by the dynamometer to find, for every gear, the engine "
                "speed above which the next gear pushes the car harder.");
            ImGui::SameLine();
            if (ImGui::Button("Clear")) std::fill(st.wotUpshift.begin(), st.wotUpshift.end(), 0.0);
            if (!state.lastShift.empty()) ImGui::TextDisabled("Last shift: %s", state.lastShift.c_str());
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::Separator();
    if (ImGui::Button("Save tune")) saveTune();
    ui::HelpMarker("Saves the tune to the user data folder (tunes/); it is loaded automatically with this engine.");
    ImGui::SameLine();
    if (ImGui::Button("Load saved tune")) loadTune(false);
    ImGui::SameLine();
    if (ImGui::Button("Copy tune")) ImGui::SetClipboardText(ecu.serialize().c_str());
    ImGui::SameLine();
    if (ImGui::Button("Paste tune")) {
        const char *text = ImGui::GetClipboardText();
        std::string error;
        if (text != nullptr && ecu.deserialize(text, &error)) m_ecuStatus = "Tune pasted";
        else m_ecuStatus = "The clipboard doesn't hold a tune" + (error.empty() ? std::string() : ": " + error);
    }
    if (!m_ecuStatus.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", m_ecuStatus.c_str());
    }

    ImGui::End();
}

// Launch test ---------------------------------------------------------------

void AppUi::updateLaunchTest(double dt) {
    if (m_launchState != LaunchState::Running || m_simulator == nullptr) return;

    const Vehicle *vehicle = m_simulator->getVehicle();
    if (vehicle == nullptr) return;

    m_launchTime += dt;
    const double speed = std::abs(vehicle->getSpeed());
    const double distance = vehicle->getTravelledDistance() - m_launchStartDistance;
    LaunchResult &r = m_launchCurrent;
    if (r.t60 < 0 && speed >= units::distance(60, units::mile) / units::hour) r.t60 = m_launchTime;
    if (r.t100 < 0 && speed >= 100.0 / 3.6) r.t100 = m_launchTime;
    if (r.t200 < 0 && speed >= 200.0 / 3.6) r.t200 = m_launchTime;
    if (r.tQuarter < 0 && distance >= 402.336) {
        r.tQuarter = m_launchTime;
        r.trap = speed;
    }

    if (r.tQuarter > 0 || m_launchTime > 60.0) {
        m_launchState = LaunchState::Done;
    }
}

void AppUi::buildLaunchTestWindow() {
    const float s = m_preferences.uiScale;
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(520 * s, 430 * s), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Launch test", &m_showLaunchTest)) {
        ImGui::End();
        return;
    }

    if (m_engine == nullptr || m_simulator == nullptr || m_simulator->getVehicle() == nullptr) {
        ImGui::TextDisabled("No engine");
        ImGui::End();
        return;
    }

    Ecu &ecu = m_simulator->getEcu();
    const Vehicle *vehicle = m_simulator->getVehicle();
    const double dt = ImGui::GetIO().DeltaTime;

    ImGui::TextWrapped("Times a standing start at full throttle: 0-60 mph, 0-100 km/h, 0-200 km/h and the quarter mile.");

    if (m_launchState == LaunchState::Idle || m_launchState == LaunchState::Done) {
        ImGui::Checkbox("Automatic gearbox", &m_launchAutomatic);
        ImGui::SameLine();
        ImGui::Checkbox("Launch control", &ecu.settings().launchControl);
        if (ecu.settings().launchControl) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(140 * s);
            sliderDouble("##launchrpm", &ecu.settings().launchRpm, 1500, ecu.settings().revLimit, "%.0f rpm");
        }
        if (!m_launchAutomatic) {
            ImGui::TextDisabled("Manual: the clock starts when the car moves; you shift (arrow keys) and work the clutch.");
        }

        const bool running = m_engine->getIgnitionModule()->m_enabled && m_engine->getRpm() > 300;
        if (!running) ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(ui::WarningColor()), "Start the engine first.");
        ImGui::BeginDisabled(!running);
        if (ImGui::Button(m_launchState == LaunchState::Done ? "Run again" : "Start the run", ImVec2(-FLT_MIN, 34 * s))) {
            m_launchWasAutomatic = ecu.settings().automatic;
            m_simulator->m_dyno.m_enabled = false;
            m_launchCurrent = LaunchResult();
            m_launchCurrent.engine = m_engine->getName();
            m_launchCurrent.setup = m_launchAutomatic ? "automatic" : "manual";
            if (ecu.settings().launchControl) m_launchCurrent.setup += ", launch " + std::to_string(static_cast<int>(ecu.settings().launchRpm));
            m_launchStageTimer = 0;
            m_launchState = LaunchState::Staging;
            if (m_launchAutomatic) {
                ecu.settings().enabled = true;
                ecu.settings().automatic = true;
            }
        }
        ImGui::EndDisabled();
    }
    else {
        if (ImGui::Button("Abort", ImVec2(-FLT_MIN, 34 * s))) {
            m_launchState = LaunchState::Idle;
            m_controls.throttle = 0;
            m_controls.brakeHeld = false;
            ecu.settings().automatic = m_launchWasAutomatic;
        }
    }

    // Staging: on the brakes with the throttle open (the launch control holds
    // the revs), then release; manual runs start timing when the car moves
    if (m_launchState == LaunchState::Staging) {
        m_launchStageTimer += dt;
        if (m_launchAutomatic) {
            m_controls.brakeHeld = true;
            m_controls.throttle = 1.0;
            ImGui::Text("Staging... %.1f", std::max(1.5 - m_launchStageTimer, 0.0));
            if (m_launchStageTimer > 1.5 && std::abs(vehicle->getSpeed()) < 0.5) {
                m_controls.brakeHeld = false;
                m_launchStartDistance = vehicle->getTravelledDistance();
                m_launchTime = 0;
                m_launchState = LaunchState::Running;
            }
        }
        else {
            ImGui::TextUnformatted("Ready: launch when you like.");
            if (std::abs(vehicle->getSpeed()) > 0.3) {
                m_launchStartDistance = vehicle->getTravelledDistance();
                m_launchTime = 0;
                m_launchState = LaunchState::Running;
            }
        }
    }
    else if (m_launchState == LaunchState::Running) {
        if (m_launchAutomatic) m_controls.throttle = 1.0;
        ImGui::Text("%.2f s   %.0f km/h   %.0f m", m_launchTime, std::abs(vehicle->getSpeed()) * 3.6,
            vehicle->getTravelledDistance() - m_launchStartDistance);
    }
    else if (m_launchState == LaunchState::Done) {
        if (m_launchCurrent.engine.size() > 0) {
            m_launchResults.insert(m_launchResults.begin(), m_launchCurrent);
            if (m_launchResults.size() > 12) m_launchResults.pop_back();
            log("Launch test: 0-100 km/h " + seconds(m_launchCurrent.t100) + ", quarter mile " + seconds(m_launchCurrent.tQuarter));
            m_launchCurrent.engine.clear();
            if (m_launchAutomatic) m_controls.throttle = 0;
            ecu.settings().automatic = m_launchWasAutomatic;
        }
    }

    // Live values of the current run
    const LaunchResult &now = (m_launchState == LaunchState::Running) ? m_launchCurrent
        : (m_launchResults.empty() ? m_launchCurrent : m_launchResults.front());
    if (ImGui::BeginTable("##now", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchSame)) {
        const char *headers[] = { "0-60 mph", "0-100 km/h", "0-200 km/h", "Quarter mile" };
        for (const char *h : headers) ImGui::TableSetupColumn(h);
        ImGui::TableHeadersRow();
        ImGui::TableNextRow();
        ImGui::PushFont(m_font);
        ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(seconds(now.t60).c_str());
        ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(seconds(now.t100).c_str());
        ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(seconds(now.t200).c_str());
        ImGui::TableSetColumnIndex(3);
        if (now.tQuarter > 0) ImGui::Text("%s @ %.0f %s", seconds(now.tQuarter).c_str(), toSpeed(now.trap), speedUnits().c_str());
        else ImGui::TextUnformatted("-");
        ImGui::PopFont();
        ImGui::EndTable();
    }

    if (!m_launchResults.empty()) {
        ImGui::SeparatorText("Previous runs");
        if (ImGui::BeginTable("##runs", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY,
            ImVec2(-1, ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing())))
        {
            ImGui::TableSetupColumn("Engine / setup", ImGuiTableColumnFlags_WidthStretch, 2.0f);
            ImGui::TableSetupColumn("0-60");
            ImGui::TableSetupColumn("0-100");
            ImGui::TableSetupColumn("0-200");
            ImGui::TableSetupColumn("1/4 mile");
            ImGui::TableHeadersRow();
            for (const LaunchResult &r : m_launchResults) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("%s (%s)", r.engine.c_str(), r.setup.c_str());
                ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(seconds(r.t60).c_str());
                ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(seconds(r.t100).c_str());
                ImGui::TableSetColumnIndex(3); ImGui::TextUnformatted(seconds(r.t200).c_str());
                ImGui::TableSetColumnIndex(4); ImGui::TextUnformatted(seconds(r.tQuarter).c_str());
            }
            ImGui::EndTable();
        }
        if (ImGui::Button("Clear results")) m_launchResults.clear();
    }

    ImGui::End();
}

// Welcome -------------------------------------------------------------------

void AppUi::buildWelcomeWindow() {
    const float s = m_preferences.uiScale;
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(560 * s, 0), ImGuiCond_Appearing);
    if (!ImGui::Begin("Welcome", &m_showWelcome, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    ImGui::PushFont(m_font);
    ImGui::TextUnformatted("Welcome to Engine Simulator");
    ImGui::PopFont();
    ImGui::TextWrapped("Pick how much of the simulator you want to see. You can switch at any time "
        "with the Beginner / Advanced buttons at the top right.");
    ImGui::Spacing();

    const float w = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2;
    ImGui::BeginGroup();
    if (ImGui::Button("Beginner", ImVec2(w, 40 * s))) {
        m_preferences.uiMode = "beginner";
        m_layout = Layout::Instruments;
        m_showWelcome = false;
        if (m_simulator != nullptr) m_simulator->getEcu().settings().automatic = true;
    }
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + w);
    ImGui::TextDisabled("One-click start, automatic gearbox, the main gauges and pictures that explain the design settings.");
    ImGui::PopTextWrapPos();
    ImGui::EndGroup();
    ImGui::SameLine();
    ImGui::BeginGroup();
    if (ImGui::Button("Advanced", ImVec2(w, 40 * s))) {
        m_preferences.uiMode = "advanced";
        m_layout = Layout::Full;
        m_showWelcome = false;
    }
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + w);
    ImGui::TextDisabled("Every control and setting, analysis plots (pressure, p-V, flow, dyno), the ECU maps and the full designer.");
    ImGui::PopTextWrapPos();
    ImGui::EndGroup();

    ImGui::Spacing();
    ImGui::SeparatorText("Getting started");
    ImGui::BulletText("Press Start engine, then open the throttle with the slider or Q/W/E/R.");
    ImGui::BulletText("Engines opens the library of famous engines; Designer builds your own.");
    ImGui::BulletText("ECU tuning changes timing, fuel, boost, limiters and the gearbox.");

    if (!m_showWelcome) {
        m_preferences.firstRun = false;
        m_preferencesDirty = true;
    }

    ImGui::End();
}

// Pictures of the design ------------------------------------------------------

void AppUi::drawDesignDiagrams(const EngineDesign &d, float width) {
    const float s = m_preferences.uiScale;
    ImDrawList *draw = ImGui::GetWindowDrawList();
    const ImU32 metal = colorU32(0.62f, 0.66f, 0.72f);
    const ImU32 dark = colorU32(0.30f, 0.33f, 0.37f);
    const ImU32 text = ImGui::GetColorU32(ImGuiCol_Text);
    const ImU32 faint = ImGui::GetColorU32(ImGuiCol_TextDisabled);
    const ImU32 accent = ui::AccentColor();
    const ImU32 intake = colorU32(0.35f, 0.65f, 0.95f);
    const ImU32 exhaust = colorU32(0.95f, 0.45f, 0.30f);

    auto caption = [&](const char *title) {
        ImGui::TextUnformatted(title);
    };

    // 1. Layout seen from the front
    caption(d.isRotary() ? "Rotors (front view)" : "Layout (front view)");
    {
        const float h = 150 * s;
        const ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##layout", ImVec2(width, h));
        const ImVec2 c(p0.x + width * (d.isRotary() ? 0.5f : 0.28f), p0.y + h * 0.72f);

        if (d.isRotary()) {
            // Housing (epitrochoid) and a rotor per row
            const float R = h * 0.36f;
            const float e = R * static_cast<float>(d.eccentricity / d.generatingRadius);
            for (int k = 0; k < std::min(d.rotors, 4); ++k) {
                const float off = (k - (std::min(d.rotors, 4) - 1) / 2.0f) * 7 * s;
                ImVec2 pts[96];
                for (int i = 0; i < 96; ++i) {
                    const float a = 2 * 3.14159265f * i / 96;
                    pts[i] = ImVec2(c.x + off + e * std::cos(3 * a) + R * std::cos(a),
                        c.y - h * 0.22f + off + e * std::sin(3 * a) + R * std::sin(a));
                }
                draw->AddPolyline(pts, 96, k == 0 ? metal : dark, ImDrawFlags_Closed, 2 * s);
            }
            ImVec2 tri[3];
            for (int i = 0; i < 3; ++i) {
                const float a = 2 * 3.14159265f * i / 3 + 0.3f;
                tri[i] = ImVec2(c.x + R * 0.95f * std::cos(a), c.y - h * 0.22f + R * 0.95f * std::sin(a));
            }
            draw->AddTriangleFilled(tri[0], tri[1], tri[2], colorU32(0.62f, 0.66f, 0.72f, 0.5f));
            char buffer[48];
            std::snprintf(buffer, sizeof(buffer), "%d rotor%s", d.rotors, d.rotors > 1 ? "s" : "");
            draw->AddText(ImVec2(p0.x + 4 * s, p0.y + 2 * s), faint, buffer);
        }
        else {
            const std::vector<double> angles = d.bankAngles();
            const float len = h * 0.62f;
            const float bore = std::clamp(static_cast<float>(len * d.bore / (d.stroke * 2.6)), 10 * s, len * 0.55f);
            for (size_t b = 0; b < angles.size(); ++b) {
                const float a = static_cast<float>(angles[b]);
                const ImVec2 dir(std::sin(a), -std::cos(a));
                const ImVec2 n(dir.y, -dir.x);
                const ImVec2 base(c.x + dir.x * len * 0.25f, c.y + dir.y * len * 0.25f);
                const ImVec2 top(c.x + dir.x * len, c.y + dir.y * len);
                ImVec2 quad[4] = {
                    ImVec2(base.x + n.x * bore / 2, base.y + n.y * bore / 2),
                    ImVec2(top.x + n.x * bore / 2, top.y + n.y * bore / 2),
                    ImVec2(top.x - n.x * bore / 2, top.y - n.y * bore / 2),
                    ImVec2(base.x - n.x * bore / 2, base.y - n.y * bore / 2) };
                draw->AddConvexPolyFilled(quad, 4, dark);
                draw->AddPolyline(quad, 4, metal, ImDrawFlags_Closed, 1.5f * s);
                char count[8];
                std::snprintf(count, sizeof(count), "%d", d.cylindersPerBank());
                const ImVec2 ts = ImGui::CalcTextSize(count);
                draw->AddText(ImVec2(top.x + dir.x * 10 * s - ts.x / 2, top.y + dir.y * 10 * s - ts.y / 2), text, count);
            }
            draw->AddCircleFilled(c, 9 * s, metal);

            // Top view: a row of bores per bank along the crankshaft
            {
                const int banks = static_cast<int>(angles.size());
                const int perBank = std::max(d.cylindersPerBank(), 1);
                const float x0 = p0.x + width * 0.55f, x1 = p0.x + width - 10 * s;
                const float pitch = std::min((x1 - x0) / perBank, 34 * s);
                const float r = pitch * 0.38f;
                const float rowH = std::min((h - 30 * s) / banks, 2.6f * r + 6 * s);
                const float y0 = p0.y + h / 2 - rowH * banks / 2 + rowH / 2;
                draw->AddText(ImVec2(x0, p0.y + 2 * s), faint, "Top view");
                draw->AddLine(ImVec2(x0 - 6 * s, y0 + rowH * (banks - 1) / 2.0f),
                    ImVec2(x0 + pitch * perBank + 6 * s, y0 + rowH * (banks - 1) / 2.0f), faint, 3 * s);
                for (int b = 0; b < banks; ++b) {
                    // Banks of a V or boxer are staggered by a rod width
                    const float stagger = (b % 2) * pitch * 0.18f;
                    for (int i = 0; i < perBank; ++i) {
                        const ImVec2 bore(x0 + stagger + pitch * (i + 0.5f), y0 + rowH * b);
                        draw->AddCircleFilled(bore, r, dark);
                        draw->AddCircle(bore, r, metal, 24, 1.5f * s);
                    }
                }
            }

            char buffer[64];
            if (d.layout == EngineDesign::Layout::V) std::snprintf(buffer, sizeof(buffer), "V%d, %.0f deg", d.cylinders, d.bankAngle / Deg);
            else if (d.layout == EngineDesign::Layout::Flat) std::snprintf(buffer, sizeof(buffer), "Flat %d (boxer)", d.cylinders);
            else if (d.layout == EngineDesign::Layout::W) std::snprintf(buffer, sizeof(buffer), "W%d, %d banks", d.cylinders, d.bankCount());
            else std::snprintf(buffer, sizeof(buffer), "Inline %d", d.cylinders);
            draw->AddText(ImVec2(p0.x + 4 * s, p0.y + 2 * s), faint, buffer);
        }
    }

    // 2. One cylinder to scale: piston at TDC and BDC, rod and crank
    if (!d.isRotary()) {
        caption("Cylinder to scale");
        const float h = 210 * s;
        const ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##cylinder", ImVec2(width, h));

        const double compressionHeight = 0.35 * d.bore;
        const double chamber = d.stroke / (d.compressionRatio - 1);
        const double total = chamber + d.stroke + compressionHeight + d.rodLength + d.stroke / 2 + d.stroke / 2;
        const float k = static_cast<float>((h - 20 * s) / total);
        const float boreW = static_cast<float>(d.bore) * k;
        const float cx = p0.x + width * 0.42f;
        const float deck = p0.y + 10 * s + static_cast<float>(chamber) * k;
        const float crankY = deck + static_cast<float>(d.stroke / 2 + d.rodLength + compressionHeight) * k;
        const float throwR = static_cast<float>(d.stroke / 2) * k;

        // Walls, head and chamber
        draw->AddRectFilled(ImVec2(cx - boreW / 2 - 6 * s, p0.y + 4 * s), ImVec2(cx + boreW / 2 + 6 * s, deck), dark);
        draw->AddRectFilled(ImVec2(cx - boreW / 2, deck - static_cast<float>(chamber) * k), ImVec2(cx + boreW / 2, deck), colorU32(0.95f, 0.55f, 0.2f, 0.35f));
        draw->AddRectFilled(ImVec2(cx - boreW / 2 - 6 * s, deck), ImVec2(cx - boreW / 2, crankY - throwR * 1.2f), dark);
        draw->AddRectFilled(ImVec2(cx + boreW / 2, deck), ImVec2(cx + boreW / 2 + 6 * s, crankY - throwR * 1.2f), dark);

        // Piston at BDC (faint) and TDC
        const float pistonH = static_cast<float>(compressionHeight) * k * 1.6f;
        const float bdc = deck + static_cast<float>(d.stroke) * k;
        draw->AddRect(ImVec2(cx - boreW / 2 + 1, bdc), ImVec2(cx + boreW / 2 - 1, bdc + pistonH), faint, 2 * s);
        draw->AddRectFilled(ImVec2(cx - boreW / 2 + 1, deck), ImVec2(cx + boreW / 2 - 1, deck + pistonH), metal, 2 * s);

        // Rod and crank at TDC
        const ImVec2 pin(cx, deck + static_cast<float>(compressionHeight) * k);
        const ImVec2 journal(cx, crankY - throwR);
        draw->AddLine(pin, journal, metal, std::max(boreW * 0.12f, 3 * s));
        draw->AddCircle(ImVec2(cx, crankY), throwR, faint, 32, 1.5f * s);
        draw->AddCircleFilled(ImVec2(cx, crankY), std::max(throwR * 0.35f, 3 * s), metal);

        // Dimensions
        char buffer[64];
        const float lx = cx + boreW / 2 + 16 * s;
        std::snprintf(buffer, sizeof(buffer), "Bore %.1f mm", d.bore * 1000);
        draw->AddText(ImVec2(lx, p0.y + 8 * s), text, buffer);
        std::snprintf(buffer, sizeof(buffer), "Stroke %.1f mm", d.stroke * 1000);
        draw->AddText(ImVec2(lx, p0.y + 8 * s + ImGui::GetTextLineHeightWithSpacing()), text, buffer);
        std::snprintf(buffer, sizeof(buffer), "Rod %.1f mm (%.2f x stroke)", d.rodLength * 1000, d.rodLength / d.stroke);
        draw->AddText(ImVec2(lx, p0.y + 8 * s + 2 * ImGui::GetTextLineHeightWithSpacing()), text, buffer);
        std::snprintf(buffer, sizeof(buffer), "Compression %.1f : 1", d.compressionRatio);
        draw->AddText(ImVec2(lx, p0.y + 8 * s + 3 * ImGui::GetTextLineHeightWithSpacing()), accent, buffer);
        const char *shape = (d.bore > 1.05 * d.stroke) ? "Oversquare: revs freely"
            : (d.bore < 0.95 * d.stroke) ? "Undersquare: low-down torque" : "Square";
        draw->AddText(ImVec2(lx, p0.y + 8 * s + 4 * ImGui::GetTextLineHeightWithSpacing()), faint, shape);
        // Stroke bracket
        draw->AddLine(ImVec2(cx - boreW / 2 - 12 * s, deck), ImVec2(cx - boreW / 2 - 12 * s, bdc), accent, 1.5f * s);
    }

    // 3. Valve or port timing over one crank revolution
    {
        caption(d.isRotary() ? "Port timing" : "Cam timing");
        const float h = 170 * s;
        const ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##timing", ImVec2(width, h));
        const ImVec2 c(p0.x + h / 2 + 10 * s, p0.y + h / 2);
        const float r = h * 0.4f;

        double io, ic, eo, ec;
        if (d.isRotary()) {
            io = d.intakeOpen / Deg; ic = d.intakeClose / Deg;
            eo = d.exhaustOpen / Deg; ec = d.exhaustClose / Deg;
        }
        else {
            io = d.intakeDuration / 2 / Deg - d.intakeLobeCenter / Deg;     // BTDC
            ic = d.intakeLobeCenter / Deg + d.intakeDuration / 2 / Deg - 180; // ABDC
            eo = d.exhaustLobeCenter / Deg + d.exhaustDuration / 2 / Deg - 180; // BBDC
            ec = d.exhaustDuration / 2 / Deg - d.exhaustLobeCenter / Deg;   // ATDC
        }

        // TDC at the top, crank turning clockwise. Angle a (degrees after
        // TDC) maps to screen angle -90 + a
        auto arc = [&](double from, double to, float radius, ImU32 color) {
            const int n = 48;
            ImVec2 pts[49];
            for (int i = 0; i <= n; ++i) {
                const double a = (from + (to - from) * i / n - 90.0) * Deg;
                pts[i] = ImVec2(c.x + radius * static_cast<float>(std::cos(a)), c.y + radius * static_cast<float>(std::sin(a)));
            }
            draw->AddPolyline(pts, n + 1, color, 0, 7 * s);
        };

        draw->AddCircle(c, r, faint, 64, 1.0f * s);
        arc(-io, 180 + ic, r, intake);
        arc(180 - eo, 360 + ec, r - 9 * s, exhaust);
        draw->AddText(ImVec2(c.x - 12 * s, c.y - r - 18 * s), text, "TDC");
        draw->AddText(ImVec2(c.x - 12 * s, c.y + r + 2 * s), text, "BDC");

        char buffer[96];
        const float lx = c.x + r + 24 * s;
        float ly = p0.y + 10 * s;
        const float line = ImGui::GetTextLineHeightWithSpacing();
        draw->AddText(ImVec2(lx, ly), intake, "Intake"); ly += line;
        std::snprintf(buffer, sizeof(buffer), "opens %.0f deg BTDC", io);
        draw->AddText(ImVec2(lx, ly), text, buffer); ly += line;
        std::snprintf(buffer, sizeof(buffer), "closes %.0f deg ABDC", ic);
        draw->AddText(ImVec2(lx, ly), text, buffer); ly += line * 1.3f;
        draw->AddText(ImVec2(lx, ly), exhaust, "Exhaust"); ly += line;
        std::snprintf(buffer, sizeof(buffer), "opens %.0f deg BBDC", eo);
        draw->AddText(ImVec2(lx, ly), text, buffer); ly += line;
        std::snprintf(buffer, sizeof(buffer), "closes %.0f deg ATDC", ec);
        draw->AddText(ImVec2(lx, ly), text, buffer); ly += line * 1.3f;
        std::snprintf(buffer, sizeof(buffer), "Overlap %.0f deg", io + ec);
        draw->AddText(ImVec2(lx, ly), accent, buffer);
        if (!d.isRotary()) {
            ly += line;
            const double overlap = io + ec;
            draw->AddText(ImVec2(lx, ly), faint, overlap > 60 ? "Wild: lumpy idle, power up high"
                : overlap > 25 ? "Sporty" : "Mild: smooth idle, torque down low");
        }
    }

    // 4. Forced induction
    if (d.isBoosted()) {
        caption("Forced induction");
        const float h = 110 * s;
        const ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##fi", ImVec2(width, h));
        const float y = p0.y + h / 2;
        const bool imperial = (pressureUnits() == "inHg" || pressureUnits() == "psi");
        char boost[48];
        std::snprintf(boost, sizeof(boost), imperial ? "%.1f psi" : "%.2f bar", imperial ? d.boost / 6894.757 : d.boost / 1e5);

        if (d.isTurbocharged()) {
            // Exhaust spins the turbine, the shaft drives the compressor
            const ImVec2 turbine(p0.x + width * 0.25f, y);
            const ImVec2 compressor(p0.x + width * 0.55f, y);
            draw->AddCircleFilled(turbine, 26 * s, colorU32(0.95f, 0.45f, 0.30f, 0.35f));
            draw->AddCircle(turbine, 26 * s, exhaust, 32, 2 * s);
            draw->AddCircleFilled(compressor, 30 * s, colorU32(0.35f, 0.65f, 0.95f, 0.35f));
            draw->AddCircle(compressor, 30 * s, intake, 32, 2 * s);
            draw->AddLine(turbine, compressor, metal, 4 * s);
            draw->AddText(ImVec2(turbine.x - 28 * s, y + 30 * s), exhaust, "turbine");
            draw->AddText(ImVec2(compressor.x - 34 * s, y + 34 * s), intake, "compressor");
            draw->AddText(ImVec2(compressor.x + 40 * s, y - 20 * s), text, EngineDesign::inductionName(d.induction));
            draw->AddText(ImVec2(compressor.x + 40 * s, y), accent, boost);
            draw->AddText(ImVec2(compressor.x + 40 * s, y + 20 * s), faint,
                d.turbineAr < 0.6 ? "small housing: quick spool" : (d.turbineAr > 0.9 ? "large housing: spools late, flows more" : "medium housing"));
        }
        else {
            // Belt from the crank
            const ImVec2 crank(p0.x + width * 0.18f, y + 20 * s);
            const ImVec2 blower(p0.x + width * 0.48f, y - 10 * s);
            draw->AddCircle(crank, 14 * s, metal, 24, 3 * s);
            draw->AddCircle(blower, 9 * s, metal, 24, 3 * s);
            draw->AddLine(ImVec2(crank.x, crank.y - 14 * s), ImVec2(blower.x, blower.y - 9 * s), faint, 2 * s);
            draw->AddLine(ImVec2(crank.x, crank.y + 14 * s), ImVec2(blower.x, blower.y + 9 * s), faint, 2 * s);
            draw->AddRectFilled(ImVec2(blower.x + 16 * s, blower.y - 24 * s), ImVec2(blower.x + 76 * s, blower.y + 24 * s), colorU32(0.35f, 0.65f, 0.95f, 0.35f), 6 * s);
            draw->AddText(ImVec2(crank.x - 14 * s, crank.y + 18 * s), faint, "crank");
            draw->AddText(ImVec2(blower.x + 86 * s, y - 24 * s), text, EngineDesign::inductionName(d.induction));
            draw->AddText(ImVec2(blower.x + 86 * s, y - 4 * s), accent, boost);
            draw->AddText(ImVec2(blower.x + 86 * s, y + 16 * s), faint, "boost from idle, costs crank power");
        }
    }
}
