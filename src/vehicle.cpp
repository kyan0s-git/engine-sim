#include "../include/vehicle.h"

#include <cmath>

Vehicle::Vehicle() {
    m_rotatingMass = nullptr;
    m_mass = 0;
    m_dragCoefficient = 0;
    m_crossSectionArea = 0;
    m_diffRatio = 0;
    m_tireRadius = 0;
    m_travelledDistance = 0;
    m_rollingResistance = 0;
    m_maxBrakeForce = 0;
    m_brakeLevel = 0;
    m_simulateFlex = false;
}

Vehicle::~Vehicle() {
    /* void */
}

void Vehicle::initialize(const Parameters &params) {
    m_mass = params.mass;
    m_dragCoefficient = params.dragCoefficient;
    m_crossSectionArea = params.crossSectionArea;
    m_diffRatio = params.diffRatio;
    m_tireRadius = params.tireRadius;
    m_rollingResistance = params.rollingResistance;
    m_maxBrakeForce = params.maxBrakeForce;
    m_simulateFlex = params.simulateFlex;

    m_drivetrainSpring.m_ks = params.stiffness;
    m_drivetrainSpring.m_kd = params.damping;
    m_drivetrainSpring.m_maxFlex = params.maxFlex;
    m_drivetrainSpring.m_limitFlex = params.limitFlex;
    m_drivetrainSpring.m_maxTorque = -1.0;
}

void Vehicle::update(double dt) {
    m_travelledDistance += getSpeed() * dt;
    m_drivetrainSpring.update(dt);
}

void Vehicle::addToSystem(
    atg_scs::RigidBodySystem *system,
    atg_scs::RigidBody *rotatingMass,
    atg_scs::RigidBody *driveshaft)
{
    m_rotatingMass = rotatingMass;

    if (driveshaft != nullptr) {
        m_drivetrainSpring.connect(driveshaft, rotatingMass);
        system->addForceGenerator(&m_drivetrainSpring);
    }
    else {
        m_drivetrainSpring.disconnect();
    }
}

double Vehicle::getSpeed() const {
    if (m_rotatingMass == nullptr || m_mass <= 0) return 0.0;

    const double E_r = 0.5 * m_rotatingMass->I * m_rotatingMass->v_theta * m_rotatingMass->v_theta;
    const double vehicleSpeed = std::sqrt(2 * E_r / m_mass);

    return vehicleSpeed;

    // E_r = 0.5 * I * v_theta^2
    // E_k = 0.5 * m * v^2
}

double Vehicle::linearForceToVirtualTorque(double force) const {
    if (m_rotatingMass == nullptr || m_mass <= 0) return 0.0;

    const double rotationToKineticRatio =
        std::sqrt(m_rotatingMass->I / m_mass);
    return rotationToKineticRatio * force;
}
