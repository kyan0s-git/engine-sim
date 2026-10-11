// Headless validation tool for engine scripts.
//
// Compiles each script, builds the engine it describes and runs the
// simulation for a short time (ignition on, starter engaged, then throttle
// applied) without opening a window or audio device. Used by CI to make sure
// that every bundled engine still loads and runs.
//
// Theme scripts (scripts that only call set_application_settings()) are
// compiled and their unit names validated.

#include "../include/engine.h"
#include "../include/simulator.h"
#include "../include/transmission.h"
#include "../include/vehicle.h"
#include "../include/wankel_engine.h"
#include "../include/units.h"
#include "../scripting/include/compiler.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <initializer_list>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

    struct Options {
        std::string dataRoot;
        double simulateSeconds = 3.0;
        bool requireRunning = false;
        bool trace = false;
        double crankSeconds = 1.5;
        std::vector<std::string> scripts;
    };

    bool isOneOf(const std::string &value, std::initializer_list<const char *> options) {
        for (const char *option : options) {
            if (value == option) return true;
        }

        return false;
    }

    bool runScript(const Options &options, const std::string &script) {
        std::printf("%s\n", script.c_str());

        std::vector<std::string> searchPaths = {
            fs::path(script).parent_path().string() + "/",
            (fs::path(options.dataRoot) / "assets").string() + "/",
            (fs::path(options.dataRoot) / "es").string() + "/"
        };

        std::stringstream log;
        es_script::Compiler compiler;
        compiler.initialize(searchPaths);
        const bool compiled = compiler.compile(script, log);

        es_script::Compiler::Output output;
        if (compiled) {
            output = compiler.execute();
        }

        compiler.destroy();

        if (!compiled) {
            std::printf("  FAILED: compilation errors\n%s", log.str().c_str());
            return false;
        }

        Engine *engine = output.engine;
        if (engine == nullptr && output.applicationSettingsSet) {
            // Theme script: only check that the unit names are understood
            const ApplicationSettings &settings = output.applicationSettings;
            const bool valid =
                isOneOf(settings.powerUnits, { "hp", "kW" })
                && isOneOf(settings.torqueUnits, { "lb-ft", "Nm" })
                && isOneOf(settings.speedUnits, { "mph", "kph" })
                && isOneOf(settings.pressureUnits, { "inHg", "kPa", "mbar", "bar", "psi" })
                && isOneOf(settings.boostUnits, { "inHg", "kPa", "mbar", "bar", "psi" });

            delete output.vehicle;
            delete output.transmission;

            std::printf("  theme: power %s, torque %s, speed %s, pressure %s, boost %s\n",
                settings.powerUnits.c_str(), settings.torqueUnits.c_str(),
                settings.speedUnits.c_str(), settings.pressureUnits.c_str(),
                settings.boostUnits.c_str());

            if (!valid) {
                std::printf("  FAILED: unknown unit name\n");
            }

            return valid;
        }
        else if (engine == nullptr) {
            std::printf("  FAILED: script did not produce an engine\n");
            delete output.vehicle;
            delete output.transmission;
            return false;
        }

        Vehicle *vehicle = output.vehicle;
        if (vehicle == nullptr) {
            Vehicle::Parameters params;
            params.mass = units::mass(1500, units::kg);
            params.diffRatio = 3.42;
            params.tireRadius = units::distance(10, units::inch);
            params.dragCoefficient = 0.25;
            params.crossSectionArea = units::area(2.0, units::m2);
            params.rollingResistance = 2000.0;
            vehicle = new Vehicle;
            vehicle->initialize(params);
        }

        Transmission *transmission = output.transmission;
        if (transmission == nullptr) {
            const double ratios[] = { 3.0, 2.0, 1.5, 1.0 };
            Transmission::Parameters params;
            params.GearCount = 4;
            params.GearRatios = ratios;
            params.MaxClutchTorque = units::torque(500.0, units::ft_lb);
            transmission = new Transmission;
            transmission->initialize(params);
        }

        engine->calculateDisplacement();
        const WankelEngine *wankel = dynamic_cast<const WankelEngine *>(engine);
        std::printf("  engine: %s (%.2f L, %d %s)\n",
            engine->getName().c_str(),
            units::convert(engine->getDisplacement(), units::L),
            wankel != nullptr ? wankel->getRotorCount() : engine->getCylinderCount(),
            wankel != nullptr
                ? (wankel->getRotorCount() == 1 ? "rotor" : "rotors")
                : "cylinders");

        Simulator *simulator = engine->createSimulator(vehicle, transmission);
        simulator->setSimulationFrequency(static_cast<int>(engine->getSimulationFrequency()));

        // Impulse responses are only needed for audio; use a unit impulse
        const int16_t unitImpulse = INT16_MAX;
        for (int i = 0; i < engine->getExhaustSystemCount(); ++i) {
            simulator->synthesizer().initializeImpulseResponse(&unitImpulse, 1, 1.0f, i);
        }

        simulator->startAudioRenderingThread();

        engine->getIgnitionModule()->m_enabled = true;

        std::vector<int16_t> audio(2048);
        const double frameDt = 1 / 60.0;
        const int frames = static_cast<int>(options.simulateSeconds / frameDt);
        double peakRpm = 0.0;
        bool finite = true;

        for (int frame = 0; frame < frames; ++frame) {
            const double t = frame * frameDt;
            simulator->m_starterMotor.m_enabled = t < options.crankSeconds;
            engine->setSpeedControl(t < options.crankSeconds + 0.5 ? 0.0 : 0.6);

            simulator->startFrame(frameDt);
            while (simulator->simulateStep()) { /* void */ }
            simulator->endFrame();

            simulator->readAudioOutput(static_cast<int>(44100 * frameDt), audio.data());

            const double rpm = engine->getRpm();
            if (!std::isfinite(rpm)) {
                finite = false;
                break;
            }

            peakRpm = std::max(peakRpm, rpm);

            if (options.trace) {
                CombustionChamber *chamber = engine->getChamber(0);
                std::printf(
                    "  t=%.3f rpm=%.0f V0=%.1fcc p0=%.1fkPa T0=%.0fK lit0=%d "
                    "map=%.1fkPa in0=%.3f ex0=%.3f fuel=%.4f o2=%.3f plenum_o2=%.3f "
                    "qin=%.2e qex=%.2e\n",
                    t,
                    rpm,
                    units::convert(chamber->getVolume(), units::cc),
                    units::convert(chamber->m_system.pressure(), units::kPa),
                    chamber->m_system.temperature(),
                    chamber->isLit() ? 1 : 0,
                    units::convert(engine->getManifoldPressure(), units::kPa),
                    chamber->getIntakeOpening(),
                    chamber->getExhaustOpening(),
                    chamber->m_system.mix().p_fuel,
                    chamber->m_system.mix().p_o2,
                    engine->getIntake(0)->m_system.mix().p_o2,
                    chamber->getLastTimestepIntakeFlow(),
                    chamber->getLastTimestepExhaustFlow());
            }
        }

        const double finalRpm = engine->getRpm();

        simulator->endAudioRenderingThread();
        simulator->releaseSimulation();
        delete simulator;

        engine->destroy();
        delete engine;
        delete vehicle;
        delete transmission;

        bool success = true;
        if (!finite) {
            std::printf("  FAILED: simulation became unstable\n");
            success = false;
        }
        else {
            std::printf("  peak %.0f rpm, final %.0f rpm\n", peakRpm, finalRpm);
            if (options.requireRunning && finalRpm < 300) {
                std::printf("  FAILED: engine did not keep running\n");
                success = false;
            }
        }

        return success;
    }

} /* namespace */

int main(int argc, char *argv[]) {
    Options options;
    options.dataRoot = fs::current_path().string();

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--data" && i + 1 < argc) options.dataRoot = argv[++i];
        else if (arg == "--seconds" && i + 1 < argc) options.simulateSeconds = std::atof(argv[++i]);
        else if (arg == "--require-running") options.requireRunning = true;
        else if (arg == "--trace") options.trace = true;
        else if (arg == "--crank" && i + 1 < argc) options.crankSeconds = std::atof(argv[++i]);
        else if (arg == "--help" || arg == "-h") {
            std::printf(
                "Usage: %s [--data <dir>] [--seconds <s>] [--require-running] [--crank <s>] [--trace] <script.mr>...\n",
                argv[0]);
            return 0;
        }
        else options.scripts.push_back(arg);
    }

    if (options.scripts.empty()) {
        std::fprintf(stderr, "No scripts given\n");
        return 2;
    }

    int failures = 0;
    for (const std::string &script : options.scripts) {
        if (!runScript(options, fs::absolute(script).string())) {
            ++failures;
        }
    }

    std::printf("\n%d of %d scripts passed\n",
        (int)options.scripts.size() - failures, (int)options.scripts.size());

    return failures == 0 ? 0 : 1;
}
