// Headless engine dynamometer.
//
// Loads an engine script, starts the engine and holds it at a series of
// speeds with the dynamometer while the throttle is fixed. At each speed the
// engine is allowed to settle and then the brake torque and the engine
// metrics (IMEP, PMEP, FMEP, BMEP, efficiencies, BSFC, volumetric
// efficiency, air-fuel ratio and burn angles) are averaged and written as a
// CSV row. Power and torque are also given corrected to SAE J1349 standard
// conditions.
//
// Example:
//     engine-sim-dyno --data . --from 1000 --to 7000 --step 500 engine.mr > out.csv
//
// The work exchanged between the gas and the pistons is integrated with the
// simulation time step; --frequency 20000 or higher reduces that error for
// engineering use (at the cost of running slower than real time).

#include "../include/engine.h"
#include "../include/engine_metrics.h"
#include "../include/simulator.h"
#include "../include/transmission.h"
#include "../include/vehicle.h"
#include "../include/units.h"
#include "../scripting/include/compiler.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

    struct Options {
        std::string dataRoot;
        std::string script;
        double from = 0;            // rpm, 0 = engine's dyno range
        double to = 0;
        double step = 500;
        double throttle = 1.0;
        double settle = 1.5;        // seconds of simulated time
        double measure = 1.0;
        double frequency = 0;       // 0 = the engine's simulation frequency
        bool header = true;
        bool perCylinder = false;
    };

    struct Loaded {
        Engine *engine = nullptr;
        Vehicle *vehicle = nullptr;
        Transmission *transmission = nullptr;
    };

    bool load(const Options &options, Loaded *loaded) {
        const std::string script = fs::absolute(options.script).string();
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
        if (compiled) output = compiler.execute();
        compiler.destroy();

        if (!compiled || output.engine == nullptr) {
            std::fprintf(stderr, "%s: %s\n%s",
                script.c_str(),
                compiled ? "script did not produce an engine" : "compilation errors",
                log.str().c_str());
            delete output.vehicle;
            delete output.transmission;
            return false;
        }

        loaded->engine = output.engine;
        loaded->vehicle = output.vehicle;
        loaded->transmission = output.transmission;

        if (loaded->vehicle == nullptr) {
            Vehicle::Parameters params;
            params.mass = units::mass(1500, units::kg);
            params.diffRatio = 3.42;
            params.tireRadius = units::distance(10, units::inch);
            params.dragCoefficient = 0.25;
            params.crossSectionArea = units::area(2.0, units::m2);
            params.rollingResistance = 2000.0;
            loaded->vehicle = new Vehicle;
            loaded->vehicle->initialize(params);
        }

        if (loaded->transmission == nullptr) {
            static const double ratios[] = { 3.0, 2.0, 1.5, 1.0 };
            Transmission::Parameters params;
            params.GearCount = 4;
            params.GearRatios = ratios;
            params.MaxClutchTorque = units::torque(500.0, units::ft_lb);
            loaded->transmission = new Transmission;
            loaded->transmission->initialize(params);
        }

        return true;
    }

    // Runs the simulation for (at least) the given simulated time, calling
    // `sample` after every frame with the frame's simulated duration
    template <typename Sample>
    void run(Simulator *simulator, double seconds, Sample sample) {
        constexpr double frameDt = 1 / 60.0;
        std::vector<int16_t> audio(4096);

        double t = 0;
        while (t < seconds) {
            simulator->startFrame(frameDt);
            while (simulator->simulateStep()) { /* void */ }
            simulator->endFrame();
            simulator->readAudioOutput(static_cast<int>(44100 * frameDt), audio.data());

            const double dt = simulator->getFrameIterationCount() * simulator->getTimestep();
            t += dt;
            sample(dt);
        }
    }

    // Applies f(field of a, field of b) to every averaged metric
    template <typename F>
    void forEachMetric(EngineMetrics &a, const EngineMetrics &b, F f) {
        double EngineMetrics::*fields[] = {
            &EngineMetrics::speed, &EngineMetrics::imepGross, &EngineMetrics::imepNet,
            &EngineMetrics::pmep, &EngineMetrics::bmep, &EngineMetrics::fmep,
            &EngineMetrics::fmepModel, &EngineMetrics::indicatedTorque,
            &EngineMetrics::brakeTorque, &EngineMetrics::indicatedPower,
            &EngineMetrics::brakePower, &EngineMetrics::mechanicalEfficiency,
            &EngineMetrics::indicatedEfficiency, &EngineMetrics::brakeEfficiency,
            &EngineMetrics::fuelFlow, &EngineMetrics::airFlow, &EngineMetrics::isfc,
            &EngineMetrics::bsfc, &EngineMetrics::volumetricEfficiency, &EngineMetrics::afr,
            &EngineMetrics::lambda, &EngineMetrics::sparkAdvance, &EngineMetrics::ca10,
            &EngineMetrics::ca50, &EngineMetrics::ca90, &EngineMetrics::peakPressure,
            &EngineMetrics::peakPressureAngle, &EngineMetrics::peakTemperature,
            &EngineMetrics::heatLossFraction
        };

        for (double EngineMetrics::*field : fields) {
            f(a.*field, b.*field);
        }
    }

    // Time-weighted average over the measurement window
    struct Average {
        double weight = 0;
        double metricsWeight = 0;
        double torque = 0;
        double map = 0;
        double rpm = 0;
        EngineMetrics metrics;

        void add(const EngineMetrics &m, double torque_, double map_, double rpm_, double w) {
            weight += w;
            torque += w * torque_;
            map += w * map_;
            rpm += w * rpm_;

            if (!m.valid) return;

            metricsWeight += w;
            forEachMetric(metrics, m, [w](double &sum, double value) { sum += w * value; });
        }

        void finish() {
            if (weight > 0) {
                torque /= weight;
                map /= weight;
                rpm /= weight;
            }

            if (metricsWeight > 0) {
                const double inv = 1 / metricsWeight;
                forEachMetric(metrics, metrics, [inv](double &sum, double) { sum *= inv; });
            }
        }
    };

    // The current directory if it holds the data files, otherwise the
    // installation the executable belongs to (<root>/bin/engine-sim-dyno)
    std::string defaultDataRoot(const char *argv0) {
        std::error_code ec;
        const fs::path cwd = fs::current_path(ec);
        if (fs::is_directory(cwd / "es", ec)) return cwd.string();

        fs::path exe = fs::canonical("/proc/self/exe", ec);
        if (ec) exe = fs::weakly_canonical(fs::absolute(argv0, ec), ec);

        for (fs::path dir = exe.parent_path(); !dir.empty() && dir != dir.root_path(); dir = dir.parent_path()) {
            if (fs::is_directory(dir / "es", ec) && fs::is_directory(dir / "assets", ec)) return dir.string();
            if (fs::is_directory(dir / "Resources" / "es", ec)) return (dir / "Resources").string();
        }

        return cwd.string();
    }

} /* namespace */

int main(int argc, char *argv[]) {
    Options options;
    options.dataRoot = defaultDataRoot(argv[0]);

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto next = [&]() { return (i + 1 < argc) ? std::atof(argv[++i]) : 0.0; };

        if (arg == "--data" && i + 1 < argc) options.dataRoot = argv[++i];
        else if (arg == "--from") options.from = next();
        else if (arg == "--to") options.to = next();
        else if (arg == "--step") options.step = next();
        else if (arg == "--throttle") options.throttle = next();
        else if (arg == "--settle") options.settle = next();
        else if (arg == "--measure") options.measure = next();
        else if (arg == "--frequency") options.frequency = next();
        else if (arg == "--no-header") options.header = false;
        else if (arg == "--per-cylinder") options.perCylinder = true;
        else if (arg == "--help" || arg == "-h") {
            std::printf(
                "Usage: %s [--data <dir>] [--from <rpm>] [--to <rpm>] [--step <rpm>]\n"
                "          [--throttle <0-1>] [--settle <s>] [--measure <s>] [--frequency <Hz>]\n"
                "          [--no-header] [--per-cylinder] <script.mr>\n",
                argv[0]);
            return 0;
        }
        else options.script = arg;
    }

    if (options.script.empty()) {
        std::fprintf(stderr, "No script given\n");
        return 2;
    }

    Loaded loaded;
    if (!load(options, &loaded)) return 1;

    Engine *engine = loaded.engine;
    engine->calculateDisplacement();
    for (const std::string &warning : engine->getWarnings()) {
        std::fprintf(stderr, "warning: %s\n", warning.c_str());
    }

    const double from = (options.from > 0) ? options.from : units::toRpm(engine->getDynoMinSpeed());
    const double to = (options.to > 0)
        ? options.to
        : units::toRpm(std::min(engine->getDynoMaxSpeed(), engine->getRedline()));
    const double step = std::max(options.step, 1.0);

    Simulator *simulator = engine->createSimulator(loaded.vehicle, loaded.transmission);
    simulator->setSimulationFrequency(static_cast<int>(
        (options.frequency > 0) ? options.frequency : engine->getSimulationFrequency()));

    const int16_t unitImpulse = INT16_MAX;
    for (int i = 0; i < engine->getExhaustSystemCount(); ++i) {
        simulator->synthesizer().initializeImpulseResponse(&unitImpulse, 1, 1.0f, i);
    }

    simulator->startAudioRenderingThread();

    // Start the engine
    engine->getIgnitionModule()->m_enabled = true;
    engine->setSpeedControl(0.0);
    simulator->m_starterMotor.m_enabled = true;
    run(simulator, 1.5, [](double) {});
    simulator->m_starterMotor.m_enabled = false;

    engine->setSpeedControl(options.throttle);
    simulator->m_dyno.m_enabled = true;
    simulator->m_dyno.m_hold = true;

    std::fprintf(stderr, "%s: %.2f L, %d cylinders, %s physics, friction model %s\n",
        engine->getName().c_str(),
        units::convert(engine->getDisplacement(), units::L),
        engine->getCylinderCount(),
        engine->isEngineering() ? "engineering" : "legacy",
        engine->usesFrictionModel() ? "on" : "off");

    const double cf = saeJ1349CorrectionFactor(SimulationAmbientPressure, SimulationAmbientTemperature);

    if (options.header) {
        std::printf(
            "rpm,throttle,torque_Nm,power_kW,torque_j1349_Nm,power_j1349_kW,"
            "imep_gross_bar,imep_net_bar,pmep_bar,fmep_bar,fmep_model_bar,bmep_bar,"
            "mechanical_eff,indicated_eff,brake_eff,isfc_g_kWh,bsfc_g_kWh,"
            "volumetric_eff,afr,lambda,map_kPa,spark_btdc_deg,ca10_deg,ca50_deg,ca90_deg,"
            "pmax_bar,pmax_angle_deg,tmax_K,wall_heat_fraction\n");
    }

    const double bar = units::pressure(1.0, units::bar);
    const double g_kWh = 1E3 * 3.6E6;     // kg/J -> g/kWh

    for (double rpm = from; rpm <= to + 1E-6; rpm += step) {
        simulator->m_dyno.m_rotationSpeed = units::rpm(rpm);
        run(simulator, options.settle, [](double) {});

        Average average;
        run(simulator, options.measure, [&](double dt) {
            const double torque = simulator->getFilteredDynoTorque();
            const EngineMetrics m = calculateEngineMetrics(*engine, torque);
            average.add(m, torque, engine->getManifoldPressure(), engine->getRpm(), dt);
        });
        average.finish();

        const EngineMetrics &m = average.metrics;
        const double torque = average.torque;
        const double power = torque * units::rpm(average.rpm);

        std::printf(
            "%.0f,%.2f,%.2f,%.3f,%.2f,%.3f,"
            "%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,"
            "%.3f,%.3f,%.3f,%.1f,%.1f,"
            "%.3f,%.2f,%.3f,%.1f,%.1f,%.1f,%.1f,%.1f,"
            "%.1f,%.1f,%.0f,%.3f\n",
            average.rpm, options.throttle,
            torque, power / 1000.0, torque * cf, power * cf / 1000.0,
            m.imepGross / bar, m.imepNet / bar, m.pmep / bar, m.fmep / bar, m.fmepModel / bar, m.bmep / bar,
            m.mechanicalEfficiency, m.indicatedEfficiency, m.brakeEfficiency,
            m.isfc * g_kWh, m.bsfc * g_kWh,
            m.volumetricEfficiency, m.afr, m.lambda,
            units::convert(average.map, units::kPa),
            m.sparkAdvance, m.ca10, m.ca50, m.ca90,
            m.peakPressure / bar, m.peakPressureAngle, m.peakTemperature, m.heatLossFraction);
        std::fflush(stdout);

        if (options.perCylinder) {
            // Last cycle of every cylinder (to stderr so the CSV stays clean)
            for (int i = 0; i < engine->getCylinderCount(); ++i) {
                const CombustionChamber::CycleMetrics &c = engine->getChamber(i)->getLastCycleMetrics();
                std::fprintf(stderr,
                    "  %5.0f rpm cyl %2d: imep %6.2f bar, spark %6.1f BTDC, CA50 %6.1f, pmax %6.1f bar @ %5.1f, VE %.3f\n",
                    average.rpm, i + 1, c.imepNet / bar, c.sparkAdvance, c.ca50,
                    c.peakPressure / bar, c.peakPressureAngle,
                    c.airInducted / (SimulationAmbientPressure * thermo::air::molarMass
                        / (constants::R * SimulationAmbientTemperature) * engine->getChamber(i)->getSweptVolume()));
            }
        }
    }

    simulator->endAudioRenderingThread();
    simulator->releaseSimulation();
    delete simulator;

    engine->destroy();
    delete engine;
    delete loaded.vehicle;
    delete loaded.transmission;

    return 0;
}
