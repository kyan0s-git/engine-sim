#include "../include/wankel_engine.h"

#include "../include/wankel_combustion_chamber.h"
#include "../include/wankel_engine_simulator.h"

#include <assert.h>

WankelEngine::WankelEngine() {
    m_rotors = nullptr;
    m_rotorCount = 0;
}

WankelEngine::~WankelEngine() {
    assert(m_rotors == nullptr);
}

void WankelEngine::initialize(const Parameters &params) {
    assert(params.cylinderCount == params.cylinderBanks * WankelRotor::FaceCount);

    Engine::initialize(params);

    m_rotorCount = params.cylinderBanks;
    m_rotors = new WankelRotor[m_rotorCount];
}

void WankelEngine::destroy() {
    Engine::destroy();

    delete[] m_rotors;
    m_rotors = nullptr;
    m_rotorCount = 0;
}

void WankelEngine::calculateDisplacement() {
    // Rotary engines are conventionally rated by the swept volume of a
    // single face per rotor (e.g. 2 x 654 cc = 1.3 L for a Mazda 13B)
    double displacement = 0.0;
    for (int i = 0; i < m_rotorCount; ++i) {
        displacement += m_rotors[i].getFaceDisplacement();
    }

    m_displacement = displacement;
}

Simulator *WankelEngine::createSimulator(Vehicle *vehicle, Transmission *transmission) {
    WankelEngineSimulator *simulator = new WankelEngineSimulator;
    Simulator::Parameters simulatorParams;
    simulatorParams.systemType = Simulator::SystemType::NsvOptimized;
    simulator->initialize(simulatorParams);

    simulator->loadSimulation(this, vehicle, transmission);
    simulator->setFluidSimulationSteps(m_fluidSimulationSteps);
    simulator->setMaxSleSolverSteps(m_maxSleSolverSteps);

    return simulator;
}

CombustionChamber *WankelEngine::createCombustionChamber(int index) {
    return new WankelCombustionChamber;
}
