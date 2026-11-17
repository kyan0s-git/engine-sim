// Headless standing-start acceleration test.
//
// Loads an engine script, starts the engine and launches the vehicle at full
// throttle, either with the ECU's automatic gearbox (--auto) or with manual
// shifts at a fixed engine speed. Prints a trace and the 0-60 mph,
// 0-100 km/h and quarter mile results.
//
// Example:
//     engine-sim-drive --data . --auto --preset performance engine.mr
//
// With --wav the synthesized sound is rendered offline (with the engine's
// impulse responses, as in the application) and written to a file; --rev
// replaces the launch with a free-revving routine in neutral, for listening
// to and analysing the sound.

#include "../include/ecu.h"
#include "../include/engine.h"
#include "../include/exhaust_system.h"
#include "../include/impulse_response.h"
#include "../include/forced_induction.h"
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
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

    struct Options {
        std::string dataRoot;
        std::string script;
        bool automatic = false;
        std::string preset;
        std::string tune;
        double shiftRpm = 0;        // manual: 0 = ECU rev limit - 300
        double launchRpm = 0;       // launch control (0 = off)
        double duration = 25;       // [s]
        double brakeAt = -1;        // [s] lift and brake to a stop
        double trace = 0.5;         // [s] between trace lines, 0 = none
        bool antiLag = false;
        double idle = 1.0;          // [s] of idling before the launch
        std::string wav;            // write the rendered sound here
        bool rev = false;           // free-revving routine instead of a launch
        int perspective = 0;        // 0 outside, 1 cockpit, 2 engine bay
        float crackle = -1;         // overrun pops, -1 = default
    };

    // Offline audio: rendered after every frame instead of by the audio
    // thread, so the result doesn't depend on timing
    std::vector<int16_t> g_audio;
    bool g_renderAudio = false;

    void collectAudio(Simulator *simulator) {
        Synthesizer &synthesizer = simulator->synthesizer();
        int16_t buffer[4096];
        for (;;) {
            synthesizer.renderAudio();
            const int n = synthesizer.readAudioOutput(4096, buffer);
            if (n <= 0) break;
            g_audio.insert(g_audio.end(), buffer, buffer + n);
            if (n < 4096) break;
        }
    }

    bool readWav16(const std::string &path, std::vector<int16_t> *samples) {
        std::ifstream file(path, std::ios::binary);
        if (!file) return false;
        char riff[12];
        file.read(riff, 12);
        if (!file || std::string(riff, 4) != "RIFF" || std::string(riff + 8, 4) != "WAVE") return false;

        uint16_t channels = 1, bits = 16;
        while (file) {
            char id[4];
            uint32_t size = 0;
            file.read(id, 4);
            file.read(reinterpret_cast<char *>(&size), 4);
            if (!file) return false;
            const std::string chunk(id, 4);
            if (chunk == "fmt ") {
                std::vector<char> fmt(size);
                file.read(fmt.data(), size);
                channels = *reinterpret_cast<uint16_t *>(&fmt[2]);
                bits = *reinterpret_cast<uint16_t *>(&fmt[14]);
            }
            else if (chunk == "data") {
                if (bits != 16) return false;
                std::vector<int16_t> data(size / 2);
                file.read(reinterpret_cast<char *>(data.data()), size);
                for (size_t i = 0; i < data.size(); i += channels) samples->push_back(data[i]);
                return !samples->empty();
            }
            else {
                file.seekg(size + (size & 1), std::ios::cur);
            }
        }
        return false;
    }

    void writeWav16(const std::string &path, const std::vector<int16_t> &samples, int rate) {
        std::ofstream file(path, std::ios::binary);
        const uint32_t dataSize = static_cast<uint32_t>(samples.size() * 2);
        const uint32_t riffSize = 36 + dataSize;
        const uint16_t format = 1, channels = 1, bits = 16, align = 2;
        const uint32_t sampleRate = rate, byteRate = rate * 2, fmtSize = 16;
        file.write("RIFF", 4); file.write(reinterpret_cast<const char *>(&riffSize), 4);
        file.write("WAVEfmt ", 8); file.write(reinterpret_cast<const char *>(&fmtSize), 4);
        file.write(reinterpret_cast<const char *>(&format), 2); file.write(reinterpret_cast<const char *>(&channels), 2);
        file.write(reinterpret_cast<const char *>(&sampleRate), 4); file.write(reinterpret_cast<const char *>(&byteRate), 4);
        file.write(reinterpret_cast<const char *>(&align), 2); file.write(reinterpret_cast<const char *>(&bits), 2);
        file.write("data", 4); file.write(reinterpret_cast<const char *>(&dataSize), 4);
        file.write(reinterpret_cast<const char *>(samples.data()), dataSize);
    }

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
            if (g_renderAudio) collectAudio(simulator);
            else simulator->readAudioOutput(static_cast<int>(44100 * frameDt), audio.data());

            const double dt = simulator->getFrameIterationCount() * simulator->getTimestep();
            t += dt;
            sample(dt);
        }
    }

} /* namespace */

int main(int argc, char *argv[]) {
    Options options;
    options.dataRoot = fs::current_path().string();

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const bool value = (i + 1) < argc;
        if (arg == "--data" && value) options.dataRoot = argv[++i];
        else if (arg == "--auto") options.automatic = true;
        else if (arg == "--preset" && value) options.preset = argv[++i];
        else if (arg == "--tune" && value) options.tune = argv[++i];
        else if (arg == "--shift" && value) options.shiftRpm = std::atof(argv[++i]);
        else if (arg == "--launch" && value) options.launchRpm = std::atof(argv[++i]);
        else if (arg == "--time" && value) options.duration = std::atof(argv[++i]);
        else if (arg == "--brake-at" && value) options.brakeAt = std::atof(argv[++i]);
        else if (arg == "--trace" && value) options.trace = std::atof(argv[++i]);
        else if (arg == "--anti-lag") options.antiLag = true;
        else if (arg == "--idle" && value) options.idle = std::atof(argv[++i]);
        else if (arg == "--wav" && value) options.wav = argv[++i];
        else if (arg == "--rev") options.rev = true;
        else if (arg == "--perspective" && value) options.perspective = std::atoi(argv[++i]);
        else if (arg == "--crackle" && value) options.crackle = static_cast<float>(std::atof(argv[++i]));
        else if (arg == "--help" || arg == "-h") {
            std::printf(
                "Usage: %s [--data <dir>] [--auto] [--preset economy|performance|race] [--tune <file>]\n"
                "          [--shift <rpm>] [--launch <rpm>] [--anti-lag] [--time <s>] [--trace <s>] [--brake-at <s>]\n"
                "          [--wav <file>] [--rev] [--perspective 0|1|2] [--crackle <0-1>] <script.mr>\n",
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
    Simulator *simulator = engine->createSimulator(loaded.vehicle, loaded.transmission);
    simulator->setSimulationFrequency(static_cast<int>(engine->getSimulationFrequency()));

    const int16_t unitImpulse = INT16_MAX;
    g_renderAudio = !options.wav.empty();
    for (int i = 0; i < engine->getExhaustSystemCount(); ++i) {
        std::vector<int16_t> response;
        ImpulseResponse *ir = engine->getExhaustSystem(i)->getImpulseResponse();
        if (g_renderAudio && ir != nullptr && readWav16(ir->getFilename(), &response)) {
            simulator->synthesizer().initializeImpulseResponse(
                response.data(), static_cast<unsigned int>(response.size()), static_cast<float>(ir->getVolume()), i);
        }
        else {
            simulator->synthesizer().initializeImpulseResponse(&unitImpulse, 1, 1.0f, i);
        }
    }
    if (g_renderAudio) {
        // The application's initial sound settings for this engine
        Synthesizer::AudioParameters audio = simulator->synthesizer().getAudioParameters();
        audio.inputSampleNoise = static_cast<float>(engine->getInitialJitter());
        audio.airNoise = static_cast<float>(engine->getInitialNoise());
        audio.dF_F_mix = static_cast<float>(engine->getInitialHighFrequencyGain());
        audio.convolution = static_cast<float>(engine->getInitialConvolution());
        audio.perspective = options.perspective;
        if (options.crackle >= 0) audio.overrunCrackle = options.crackle;
        simulator->synthesizer().setAudioParameters(audio);
    }
    else {
        simulator->startAudioRenderingThread();
    }

    Ecu &ecu = simulator->getEcu();
    if (!options.preset.empty()) ecu.applyPreset(options.preset);
    if (!options.tune.empty()) {
        std::ifstream file(options.tune);
        std::stringstream ss;
        ss << file.rdbuf();
        std::string error;
        if (!ecu.deserialize(ss.str(), &error)) {
            std::fprintf(stderr, "%s: %s\n", options.tune.c_str(), error.c_str());
            return 1;
        }
    }
    if (options.launchRpm > 0) {
        ecu.settings().launchControl = true;
        ecu.settings().launchRpm = options.launchRpm;
    }
    if (options.antiLag) ecu.settings().antiLag = true;
    ecu.settings().automatic = options.automatic;
    if (std::getenv("DRIVE_NO_IDLE_CONTROL") != nullptr) ecu.settings().idleControl = false;

    Transmission *transmission = loaded.transmission;
    Vehicle *vehicle = loaded.vehicle;

    // Start the engine and let it idle
    engine->getIgnitionModule()->m_enabled = true;
    engine->setSpeedControl(0.0);
    const bool idleTrace = (std::getenv("DRIVE_IDLE_TRACE") != nullptr);
    double traceClock = 0, lastTrace = -1;
    auto idleSample = [&](double dt) {
        traceClock += dt;
        if (idleTrace && traceClock - lastTrace >= 0.1) {
            lastTrace = traceClock;
            std::fprintf(stderr, "  %.2f s %5.0f rpm idle air %.3f map %.0f kPa\n",
                traceClock, engine->getRpm(), simulator->getEcu().state().idleAir,
                simulator->getEcu().state().load);
        }
    };
    simulator->m_starterMotor.m_enabled = true;
    run(simulator, 1.5, idleSample);
    simulator->m_starterMotor.m_enabled = false;
    run(simulator, options.idle, idleSample);

    const double idleRpm = engine->getRpm();
    std::fprintf(stderr, "%s: %.2f L, idle %.0f rpm, %s gearbox\n",
        engine->getName().c_str(), units::convert(engine->getDisplacement(), units::L),
        idleRpm, options.automatic ? "automatic" : "manual");

    auto finish = [&](int code) {
        if (g_renderAudio) {
            writeWav16(options.wav, g_audio, 44100);
            std::fprintf(stderr, "wrote %.1f s of sound to %s\n", g_audio.size() / 44100.0, options.wav.c_str());
        }
        simulator->endAudioRenderingThread();
        simulator->releaseSimulation();
        delete simulator;
        engine->destroy();
        delete engine;
        delete loaded.vehicle;
        delete loaded.transmission;
        return code;
    };

    if (options.rev) {
        // Free revving in neutral: blips, a pull against the limiter, lifts
        // (overrun and blow-off), part throttle, idle. One line per 0.25 s:
        // time, phase, rpm, throttle, boost
        transmission->changeGear(-1);
        struct Phase { const char *name; double throttle; double seconds; };
        const double limiterHold = 2.5;
        const Phase phases[] = {
            { "idle", 0.0, 2.0 },
            { "blip", 1.0, 0.25 }, { "lift", 0.0, 1.5 },
            { "blip", 1.0, 0.4 }, { "lift", 0.0, 1.8 },
            { "quarter", 0.25, 2.0 }, { "lift", 0.0, 1.5 },
            { "half", 0.5, 2.0 }, { "lift", 0.0, 1.5 },
            { "full", 1.0, limiterHold }, { "lift", 0.0, 3.0 },
            { "idle", 0.0, 1.5 } };
        double clock = 0, next = 0;
        for (const Phase &phase : phases) {
            engine->setSpeedControl(phase.throttle);
            run(simulator, phase.seconds, [&](double dt) {
                clock += dt;
                if (options.trace > 0 && clock >= next) {
                    next += options.trace;
                    const ForcedInduction *fi = engine->getForcedInduction();
                    std::printf("%.2f,%s,%.0f,%.2f,%.1f,%.0f\n", clock, phase.name, engine->getRpm(), phase.throttle,
                        fi != nullptr ? fi->getBoost() / 1000.0 : 0.0,
                        fi != nullptr ? fi->getShaftSpeed() * 60.0 / (2 * 3.14159265358979) : 0.0);
                }
            });
        }
        return finish(engine->getRpm() > 300 ? 0 : 1);
    }

    // Launch: build revs against the launch limiter (or to 2/3 of the
    // shift speed) with the clutch out, then go
    const double shiftRpm = (options.shiftRpm > 0) ? options.shiftRpm : ecu.settings().revLimit - 300;
    engine->setSpeedControl(1.0);
    if (!options.automatic) {
        transmission->changeGear(0);
        transmission->setClutchPressure(0.0);
        run(simulator, 1.0, [](double) {});
    }

    std::printf("t_s,rpm,gear,speed_kmh,distance_m,boost_kPa,target_kPa,clutch,throttle,idle_air,flags\n");

    double t = 0, nextTrace = 0;
    double t60 = -1, t100 = -1, tQuarter = -1, trap = 0, peakSpeed = 0;
    int shifts = 0, lastGear = transmission->getGear(), overboostCuts = 0;
    bool lastOverboost = false;
    double clutchRamp = 0, shiftTimer = 0;
    const double startDistance = vehicle->getTravelledDistance();

    double stopTime = -1;
    bool braking = false;
    while (t < options.duration) {
        run(simulator, 1 / 60.0, [&](double dt) {
            t += dt;

            if (options.brakeAt >= 0 && t >= options.brakeAt && !braking) {
                braking = true;
                engine->setSpeedControl(0.0);
                vehicle->setBrakeLevel(1.0);
            }
            if (braking && stopTime < 0 && std::abs(vehicle->getSpeed()) < 0.1) stopTime = t;

            if (!options.automatic) {
                // Manual driver: release the clutch over 0.6 s, shift at the
                // shift speed with a 0.2 s clutch-in pause
                if (shiftTimer > 0) {
                    shiftTimer -= dt;
                    transmission->setClutchPressure(shiftTimer > 0.1 ? 0.0 : 1.0 - shiftTimer / 0.1);
                }
                else {
                    clutchRamp = std::min(clutchRamp + dt / 0.6, 1.0);
                    transmission->setClutchPressure(clutchRamp);
                    if (engine->getRpm() > shiftRpm && transmission->getGear() < transmission->getGearCount() - 1
                        && clutchRamp >= 1.0)
                    {
                        transmission->changeGear(transmission->getGear() + 1);
                        shiftTimer = 0.2;
                    }
                }
            }

            const double speed = std::abs(vehicle->getSpeed());
            const double distance = vehicle->getTravelledDistance() - startDistance;
            peakSpeed = std::max(peakSpeed, speed);
            if (t60 < 0 && speed >= units::distance(60, units::mile) / units::hour) t60 = t;
            if (t100 < 0 && speed >= 100.0 / 3.6) t100 = t;
            if (tQuarter < 0 && distance >= 402.336) { tQuarter = t; trap = speed; }

            const bool overboost = simulator->getEcu().state().overboost;
            if (overboost && !lastOverboost) ++overboostCuts;
            lastOverboost = overboost;

            if (transmission->getGear() != lastGear) {
                lastGear = transmission->getGear();
                ++shifts;
            }
        });

        if (options.trace > 0 && t >= nextTrace) {
            nextTrace += options.trace;
            const ForcedInduction *fi = engine->getForcedInduction();
            const Ecu::State &ecuState = simulator->getEcu().state();
            std::printf("%.2f,%.0f,%d,%.1f,%.1f,%.1f,%.1f,%.2f,%.2f,%.3f,%s%s%s%s\n",
                t, engine->getRpm(), transmission->getGear() + 1,
                std::abs(vehicle->getSpeed()) * 3.6,
                vehicle->getTravelledDistance() - startDistance,
                (fi != nullptr) ? fi->getBoost() / 1000.0 : 0.0,
                ecuState.boostTarget,
                transmission->getClutchPressure(),
                engine->getSpeedControl(),
                ecuState.idleAir,
                ecuState.overboost ? "O" : "", ecuState.limiting ? "L" : "",
                ecuState.shifting ? "S" : "", ecuState.launchActive ? "C" : "");
        }

        if (options.brakeAt < 0 && tQuarter > 0 && t > tQuarter + 0.5) break;
    }

    auto result = [](double v) { return v < 0 ? std::string("-") : std::to_string(v).substr(0, 5) + " s"; };
    std::fprintf(stderr,
        "0-60 mph %s, 0-100 km/h %s, quarter mile %s at %.0f km/h, top speed reached %.0f km/h, %d gear changes, %d overboost cuts\n",
        result(t60).c_str(), result(t100).c_str(), result(tQuarter).c_str(),
        trap * 3.6, peakSpeed * 3.6, shifts, overboostCuts);

    if (options.brakeAt >= 0) {
        std::fprintf(stderr, "braking from %.0f s: %s, engine %s at %.0f rpm, gear %d\n",
            options.brakeAt,
            stopTime < 0 ? "still moving" : ("stopped after " + std::to_string(stopTime - options.brakeAt).substr(0, 4) + " s").c_str(),
            engine->getRpm() > 300 ? "running" : "STALLED", engine->getRpm(), transmission->getGear() + 1);
    }

    return finish((t100 > 0) ? 0 : 1);
}
