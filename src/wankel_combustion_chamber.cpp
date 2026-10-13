#include "../include/wankel_combustion_chamber.h"

#include "../include/wankel_rotor.h"
#include "../include/units.h"

#include <cmath>

WankelCombustionChamber::WankelCombustionChamber() {
    m_rotor = nullptr;
    m_face = 0;
}

WankelCombustionChamber::~WankelCombustionChamber() {
    /* void */
}

void WankelCombustionChamber::setRotor(WankelRotor *rotor, int face) {
    m_rotor = rotor;
    m_face = face;
}

double WankelCombustionChamber::shaftAngle() const {
    return m_rotor->getCrankshaft()->m_body.theta;
}

void WankelCombustionChamber::apply(atg_scs::SystemState *system) {
    // Virtual work: the gas does p * dV of work on the rotor which is
    // transmitted to the eccentric shaft as a torque of p * dV/d(theta). The
    // three chamber volumes of a rotor always add up to the same total so a
    // uniform reference pressure produces no net torque.
    const int index = m_rotor->getCrankshaft()->m_body.index;
    const double dV_dtheta =
        m_rotor->getChamberVolumeDerivative(m_face, system->theta[index]);
    const double pressureDifferential = m_system.pressure() - m_crankcasePressure;

    system->t[index] += pressureDifferential * dV_dtheta;
}

double WankelCombustionChamber::getFrictionForce() const {
    // Apex and side seal friction is part of the eccentric shaft's friction
    // torque
    return 0.0;
}

double WankelCombustionChamber::getVolume() const {
    return m_rotor->getChamberVolume(m_face, shaftAngle());
}

double WankelCombustionChamber::getSweptVolume() const {
    return m_rotor->getFaceDisplacement();
}

double WankelCombustionChamber::pistonSpeed() const {
    // Characteristic gas velocity used by the turbulence model. The charge
    // is carried around the housing by the rotor so the speed at which the
    // apexes sweep along the housing (about R * omega / 3) plays the role of
    // the mean piston speed.
    const double omega = m_rotor->getCrankshaft()->m_body.v_theta;
    return m_rotor->getGeneratingRadius() * std::abs(omega) / 3.0;
}

double WankelCombustionChamber::calculateIntakeFlowRate() const {
    const WankelRotor::Port &port = m_rotor->getIntakePort();
    if (port.flowRate == nullptr) return 0.0;

    return port.flowRate->sampleTriangle(
        m_rotor->getPortOpening(m_face, port, shaftAngle()));
}

double WankelCombustionChamber::calculateExhaustFlowRate() const {
    const WankelRotor::Port &port = m_rotor->getExhaustPort();
    if (port.flowRate == nullptr) return 0.0;

    return port.flowRate->sampleTriangle(
        m_rotor->getPortOpening(m_face, port, shaftAngle()));
}

double WankelCombustionChamber::getIntakeOpening() const {
    // Reported on the same scale as valve lift so that the oscilloscopes
    // show something sensible
    return units::distance(400, units::thou) * m_rotor->getPortOpening(
        m_face, m_rotor->getIntakePort(), shaftAngle());
}

double WankelCombustionChamber::getExhaustOpening() const {
    return units::distance(400, units::thou) * m_rotor->getPortOpening(
        m_face, m_rotor->getExhaustPort(), shaftAngle());
}
