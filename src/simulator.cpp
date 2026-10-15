#include "../include/simulator.h"

#include <algorithm>

Simulator::Simulator() {
    m_engine = nullptr;
    m_vehicle = nullptr;
    m_transmission = nullptr;
    m_system = nullptr;
    m_sleSolver = nullptr;

    m_physicsProcessingTime = 0;

    m_simulationSpeed = 1.0;
    m_targetSynthesizerLatency = 0.1;
    m_simulationFrequency = 10000;
    m_steps = 0;

    m_currentIteration = 0;

    m_filteredEngineSpeed = 0.0;
    m_dynoTorqueSamples = nullptr;
    m_lastDynoTorqueSample = 0;
}

Simulator::~Simulator() {
    assert(m_system == nullptr);
    assert(m_dynoTorqueSamples == nullptr);
}

void Simulator::initialize(const Parameters &params) {
    if (params.systemType == SystemType::NsvOptimized) {
        atg_scs::OptimizedNsvRigidBodySystem *system =
            new atg_scs::OptimizedNsvRigidBodySystem;
        m_sleSolver = new atg_scs::GaussSeidelSleSolver;
        system->initialize(m_sleSolver);
        m_system = system;
    }
    else {
        atg_scs::GenericRigidBodySystem *system =
            new atg_scs::GenericRigidBodySystem;
        system->initialize(
            new atg_scs::GaussianEliminationSleSolver,
            new atg_scs::NsvOdeSolver);
        m_system = system;
    }

    m_dynoTorqueSamples = new double[DynoTorqueSamples];
    for (int i = 0; i < DynoTorqueSamples; ++i) {
        m_dynoTorqueSamples[i] = 0.0;
    }
}

void Simulator::setMaxSleSolverSteps(int steps) {
    if (m_sleSolver != nullptr) {
        m_sleSolver->m_maxIterations = std::max(1, steps);
    }
}

int Simulator::getMaxSleSolverSteps() const {
    return (m_sleSolver != nullptr)
        ? m_sleSolver->m_maxIterations
        : 0;
}

void Simulator::loadSimulation(Engine *engine, Vehicle *vehicle, Transmission *transmission) {
    m_engine = engine;
    m_vehicle = vehicle;
    m_transmission = transmission;
}

void Simulator::releaseSimulation() {
    m_synthesizer.endAudioRenderingThread();
    if (m_system != nullptr) m_system->reset();

    destroy();
}

void Simulator::startFrame(double dt) {
    if (m_engine == nullptr) {
        m_steps = 0;
        return;
    }

    m_simulationStart = std::chrono::steady_clock::now();
    m_currentIteration = 0;
    m_synthesizer.setInputSampleRate(m_simulationFrequency * m_simulationSpeed);

    // Simulate the elapsed time, sped up or slowed down a little to keep the
    // audio queued in the synthesizer at its target: too little crackles,
    // too much adds latency. The correction is proportional to the elapsed
    // time so that it is the same however often this is called. (It used
    // to correct half of the error every frame; the queue fluctuates as
    // the audio is read in blocks, so the speed of the simulation jittered
    // from frame to frame.)
    const double timestep = getTimestep();
    const double frameTime = std::clamp(dt, 0.0, 0.1);
    const double target = getSynthesizerInputLatencyTarget();
    const double latency = m_synthesizer.getLatency();
    double audioTime;
    const auto now = std::chrono::steady_clock::now();
    if (m_lastAudioRead.time_since_epoch().count() == 0) m_lastAudioRead = now;
    const double sinceRead = std::chrono::duration<double>(now - m_lastAudioRead).count();
    if (sinceRead > 0.3 && latency >= target) {
        // Nobody is reading the audio (no output device): keep simulating
        // in real time; the synthesizer drops the oldest input instead of
        // overflowing
        audioTime = frameTime;
    }
    else {
        const double error = target - latency;

        // About to run dry: catch up quickly
        const double timeConstant = (latency < 0.4 * target) ? 0.1 : 0.6;
        const double correction = std::clamp(error * frameTime / timeConstant, -0.5 * frameTime, 0.5 * frameTime + 0.01);
        audioTime = std::clamp(frameTime + correction, 0.0, 0.15);
    }

    // Fractional steps carry over to the next call
    const double steps = audioTime * m_simulationSpeed / timestep + m_stepRemainder;
    m_steps = static_cast<int>(std::floor(steps));
    m_stepRemainder = std::clamp(steps - m_steps, 0.0, 1.0);

    if (m_steps > 0) {
        for (int i = 0; i < m_engine->getIntakeCount(); ++i) {
            m_engine->getIntake(i)->m_flowRate = 0;
        }
    }
}

bool Simulator::simulateStep() {
    if (getCurrentIteration() >= simulationSteps()) {
        auto s1 = std::chrono::steady_clock::now();

        const long long lastFrame =
            std::chrono::duration_cast<std::chrono::microseconds>(s1 - m_simulationStart).count();
        m_physicsProcessingTime = m_physicsProcessingTime * 0.98 + 0.02 * lastFrame;

        return false;
    }

    const double timestep = getTimestep();
    m_system->process(timestep, 1);

    m_engine->update(timestep);
    m_vehicle->update(timestep);
    m_transmission->update(timestep);

    updateFilteredEngineSpeed(timestep);

    Crankshaft *outputShaft = m_engine->getOutputCrankshaft();
    outputShaft->resetAngle();

    for (int i = 0; i < m_engine->getCrankshaftCount(); ++i) {
        Crankshaft *shaft = m_engine->getCrankshaft(i);

        // Correct drift (temporary hack)
        shaft->m_body.theta = outputShaft->m_body.theta;
    }

    const int index =
        static_cast<int>(std::floor(DynoTorqueSamples * outputShaft->getCycleAngle() / outputShaft->getCycleLength()));
    const int step = m_engine->isSpinningCw() ? 1 : -1;
    m_dynoTorqueSamples[index] = m_dyno.getTorque();

    if (m_lastDynoTorqueSample != index) {
        for (int i = m_lastDynoTorqueSample + step; i != index; i += step) {
            if (i >= DynoTorqueSamples) {
                i = -1;
                continue;
            }
            else if (i < 0) {
                i = DynoTorqueSamples;
                continue;
            }

            m_dynoTorqueSamples[i] = m_dyno.getTorque();
        }

        m_lastDynoTorqueSample = index;
    }

    simulateStep_();

    writeToSynthesizer();

    ++m_currentIteration;
    return true;
}

double Simulator::getTotalExhaustFlow() const {
    return 0.0;
}

int Simulator::readAudioOutput(int samples, int16_t *target) {
    const int read = m_synthesizer.readAudioOutput(samples, target);
    if (read > 0) m_lastAudioRead = std::chrono::steady_clock::now();
    return read;
}

void Simulator::endFrame() {
    m_synthesizer.endInputBlock();
}

void Simulator::destroy() {
    m_synthesizer.destroy();

    delete[] m_dynoTorqueSamples;
    m_dynoTorqueSamples = nullptr;
}

void Simulator::startAudioRenderingThread() {
    m_synthesizer.startAudioRenderingThread();
}

void Simulator::endAudioRenderingThread() {
    m_synthesizer.endAudioRenderingThread();
}

double Simulator::getSynthesizerInputLatencyTarget() const {
    return m_targetSynthesizerLatency;
}

double Simulator::getFilteredDynoTorque() const {
    if (m_dynoTorqueSamples == nullptr) return 0;

    double averageTorque = 0;
    for (int i = 0; i < DynoTorqueSamples; ++i) {
        averageTorque += m_dynoTorqueSamples[i];
    }

    return averageTorque / DynoTorqueSamples;
}

double Simulator::getDynoPower() const {
    return (m_engine != nullptr)
        ? getFilteredDynoTorque() * m_engine->getSpeed()
        : 0;
}

double Simulator::getAverageOutputSignal() const {
    return 0.0;
}

void Simulator::initializeSynthesizer() {
    Synthesizer::Parameters synthParams;
    synthParams.audioBufferSize = 44100;
    synthParams.audioSampleRate = 44100;
    synthParams.inputBufferSize = 44100;
    synthParams.inputChannelCount = m_engine->getExhaustSystemCount();
    synthParams.auxChannelCount = 3;
    synthParams.inputSampleRate = static_cast<float>(getSimulationFrequency());
    m_synthesizer.initialize(synthParams);
}

void Simulator::simulateStep_() {
}

void Simulator::updateFilteredEngineSpeed(double dt) {
    const double alpha = dt / (100 + dt);
    m_filteredEngineSpeed = alpha * m_filteredEngineSpeed + (1 - alpha) * m_engine->getRpm();
}
