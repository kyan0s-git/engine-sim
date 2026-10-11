#include "../include/wankel_engine_simulator.h"

#include "../include/wankel_engine.h"

WankelEngineSimulator::WankelEngineSimulator() {
    /* void */
}

WankelEngineSimulator::~WankelEngineSimulator() {
    /* void */
}

WankelEngine *WankelEngineSimulator::getWankelEngine() const {
    return static_cast<WankelEngine *>(m_engine);
}

void WankelEngineSimulator::addEngineParts() {
    WankelEngine *engine = getWankelEngine();

    // The rotors aren't simulated as separate bodies: their inertia is
    // carried by the eccentric shaft and the chambers apply their torque
    // to it directly
    for (int i = 0; i < engine->getRotorCount(); ++i) {
        WankelRotor *rotor = engine->getRotor(i);
        rotor->getCrankshaft()->m_body.I += rotor->getEffectiveInertia();
    }

    for (int i = 0; i < engine->getCylinderCount(); ++i) {
        m_system->addForceGenerator(engine->getChamber(i));
    }
}

void WankelEngineSimulator::placeParts() {
    /* void */
}

void WankelEngineSimulator::processIgnitionEvents() {
    WankelEngine *engine = getWankelEngine();
    IgnitionModule *im = engine->getIgnitionModule();

    // Each rotor has one spark plug (wired to its first chamber). It ignites
    // whichever face is passing it when it fires.
    for (int i = 0; i < engine->getRotorCount(); ++i) {
        const WankelRotor *rotor = engine->getRotor(i);
        if (!im->getIgnitionEvent(rotor->getFirstChamber())) continue;

        const int face = rotor->getFaceAtAngle(
            rotor->getSparkPlugAngle(),
            rotor->getCrankshaft()->m_body.theta);
        engine->getChamber(rotor->getFirstChamber() + face)->ignite();
    }
}
