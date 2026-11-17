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
#include "../include/engine_design.h"
#include "../include/simulator.h"
#include "../include/transmission.h"
#include "../include/vehicle.h"
#include "../include/wankel_engine.h"
#include "../include/units.h"
#include "../scripting/include/compiler.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <cstdlib>
#include <filesystem>
#include <random>
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
        unsigned seed = 1;                  // --random-designs (give --seed first)
        bool regenerate = false;            // rebuild designed engines from their @design lines
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
            if (!std::isfinite(rpm) || simulator->hasDiverged()) {
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

    // A design with every parameter drawn from the ranges the designer's
    // controls allow
    EngineDesign randomDesign(std::mt19937 &rng, int index) {
        auto uniform = [&](double a, double b) { return std::uniform_real_distribution<double>(a, b)(rng); };
        auto pick = [&](int n) { return std::uniform_int_distribution<int>(0, n - 1)(rng); };
        constexpr double Deg = 3.14159265358979323846 / 180.0;

        EngineDesign d;
        d.name = "Random " + std::to_string(index);
        d.layout = static_cast<EngineDesign::Layout>(pick(5));
        const int cylinderChoices[] = { 1, 2, 3, 4, 5, 6, 8, 10, 12, 16 };
        d.cylinders = cylinderChoices[pick(10)];
        d.bankAngle = uniform(15, 180) * Deg;
        d.wBanks = 3 + pick(2);
        d.vrAngle = uniform(10, 20) * Deg;
        d.bore = uniform(0.05, 0.12);
        d.stroke = uniform(0.04, 0.12);
        d.rodLength = d.stroke * uniform(1.4, 2.2);
        d.compressionRatio = uniform(6, 16);
        d.rotors = 1 + pick(4);
        d.valvesPerCylinder = 2 + pick(4);
        const char *valvetrains[] = { "dohc_bucket", "sohc_finger", "sohc_rocker", "pushrod", "pushrod_roller", "dohc_bucket_roller" };
        d.valvetrain = valvetrains[pick(6)];
        d.intakeDuration = uniform(180, 320) * Deg;
        d.exhaustDuration = uniform(180, 320) * Deg;
        d.intakeLift = uniform(0.005, 0.016);
        d.exhaustLift = uniform(0.005, 0.016);
        d.intakeLobeCenter = uniform(95, 125) * Deg;
        d.exhaustLobeCenter = uniform(95, 125) * Deg;
        d.runnerLength = uniform(0.05, 0.6);
        d.primaryLength = uniform(0.1, 1.5);
        d.exhaustLength = uniform(0.5, 5.0);
        d.exhaustDiameter = uniform(0.03, 0.1);
        const auto &sounds = EngineDesign::exhaustSounds();
        d.exhaustSound = sounds[pick(static_cast<int>(sounds.size()))].first;
        d.induction = static_cast<EngineDesign::Induction>(pick(7));
        d.boost = uniform(0.2e5, 3.5e5);
        d.turbineAr = uniform(0.35, 1.4);
        d.compressorScale = uniform(0.5, 2.0);
        d.ballBearings = pick(2) == 0;
        d.blowoffValve = pick(3) != 0;
        const char *blowoffs[] = { "atmospheric", "recirculating", "none" };
        d.blowoffType = blowoffs[pick(3)];
        d.boostControl = pick(2) ? "spring" : "electronic";
        d.externalWastegate = pick(2) == 0;
        d.twinScroll = pick(2) == 0;
        d.whistlePitch = uniform(0.5, 2.0);
        d.intercoolerEffectiveness = uniform(0, 0.95);
        d.driveRatio = pick(2) ? 0.0 : uniform(0.8, 4.0);
        d.idleAdvance = uniform(0, 30) * Deg;
        d.maxAdvance = d.idleAdvance + uniform(0, 30) * Deg;
        d.maxAdvanceRpm = uniform(1500, 12000);
        d.redline = uniform(2000, 18000);
        d.engineering = pick(4) != 0;
        const char *fuels[] = { "gasoline", "e85", "ethanol", "methanol", "iso-octane", "propane", "methane", "hydrogen" };
        d.fuel = fuels[pick(8)];
        d.lambda = uniform(0.7, 1.4);
        d.vehicleMass = uniform(300, 5000);
        const char *drives[] = { "rwd", "fwd", "awd" };
        d.drivetrain = drives[pick(3)];
        d.tireFriction = uniform(0.9, 1.6);
        d.validate();
        return d;
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
        else if (arg == "--presets" && i + 1 < argc) {
            // Writes the designer's built-in presets to a folder and checks them
            const fs::path directory = argv[++i];
            std::error_code ec;
            fs::create_directories(directory, ec);
            for (const EngineDesign &preset : builtinEnginePresets()) {
                const fs::path path = directory / (designFileName(preset.name) + ".mr");
                std::ofstream(path) << generateEngineScript(preset);
                options.scripts.push_back(path.string());
            }
        }
        else if (arg == "--random-designs" && i + 2 < argc) {
            // Random designs across the designer's ranges (a stress test)
            const int count = std::atoi(argv[++i]);
            const fs::path directory = argv[++i];
            std::error_code ec;
            fs::create_directories(directory, ec);
            std::mt19937 rng(options.seed);
            for (int k = 0; k < count; ++k) {
                const EngineDesign design = randomDesign(rng, k);
                const fs::path path = directory / ("random_" + std::to_string(k) + ".mr");
                std::ofstream(path) << generateEngineScript(design);
                options.scripts.push_back(path.string());
            }
        }
        else if (arg == "--regenerate") options.regenerate = true;
        else if (arg == "--seed" && i + 1 < argc) options.seed = static_cast<unsigned>(std::atoi(argv[++i]));
        else if (arg == "--help" || arg == "-h") {
            std::printf(
                "Usage: %s [--data <dir>] [--seconds <s>] [--require-running] [--crank <s>] [--trace]\n"
                "          [--presets <output dir>] [--seed <n>] [--random-designs <count> <output dir>]\n"
                "          [--regenerate]\n"
                "          <script.mr>...\n",
                argv[0]);
            return 0;
        }
        else options.scripts.push_back(arg);
    }

    if (options.scripts.empty()) {
        std::fprintf(stderr, "No scripts given\n");
        return 2;
    }

    if (options.regenerate) {
        // Designed engines are rebuilt from their design (to try edits of
        // the @design lines)
        for (const std::string &script : options.scripts) {
            std::stringstream text;
            text << std::ifstream(script).rdbuf();
            EngineDesign design;
            if (readEngineDesign(text.str(), &design)) {
                design.validate();
                std::ofstream(script) << generateEngineScript(design);
            }
        }
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
