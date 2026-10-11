#include "../include/transmission.h"

#include "../include/units.h"

#include <cmath>

Transmission::Transmission() {
    m_gear = -1;
    m_newGear = -1;
    m_gearCount = 0;
    m_gearRatios = nullptr;
    m_maxClutchTorque = units::torque(1000.0, units::ft_lb);
    m_rotatingMass = nullptr;
    m_output = nullptr;
    m_vehicle = nullptr;
    m_clutchPressure = 0.0;
    m_simulateFlex = false;
}

Transmission::~Transmission() {
    if (m_gearRatios != nullptr) {
        delete[] m_gearRatios;
    }

    m_gearRatios = nullptr;
}

void Transmission::initialize(const Parameters &params) {
    m_gearCount = params.GearCount;
    m_maxClutchTorque = params.MaxClutchTorque;

    delete[] m_gearRatios;
    m_gearRatios = new double[params.GearCount];
    memcpy(m_gearRatios, params.GearRatios, sizeof(double) * m_gearCount);

    m_simulateFlex = params.SimulateFlex;
    m_clutchSpring.m_ks = params.ClutchStiffness;
    m_clutchSpring.m_kd = params.ClutchDamping;
    m_clutchSpring.m_maxFlex = params.MaxClutchFlex;
    m_clutchSpring.m_limitFlex = params.LimitClutchFlex;
    m_clutchSpring.m_maxTorque = 0;
}

void Transmission::update(double dt) {
    const double maxTorque = (m_gear == -1)
        ? 0.0
        : m_maxClutchTorque * m_clutchPressure;

    m_clutchConstraint.m_minTorque = -maxTorque;
    m_clutchConstraint.m_maxTorque = maxTorque;

    m_clutchSpring.m_maxTorque = maxTorque;
    m_clutchSpring.update(dt);
}

void Transmission::addToSystem(
    atg_scs::RigidBodySystem *system,
    atg_scs::RigidBody *rotatingMass,
    Vehicle *vehicle,
    Engine *engine,
    atg_scs::RigidBody *output)
{
    m_rotatingMass = rotatingMass;
    m_output = (output != nullptr) ? output : rotatingMass;
    m_vehicle = vehicle;

    if (m_simulateFlex) {
        m_clutchSpring.connect(&engine->getOutputCrankshaft()->m_body, m_output);
        system->addForceGenerator(&m_clutchSpring);
    }
    else {
        m_clutchConstraint.setBody1(&engine->getOutputCrankshaft()->m_body);
        m_clutchConstraint.setBody2(m_output);
        system->addConstraint(&m_clutchConstraint);
    }
}

void Transmission::changeGear(int newGear) {
    if (newGear < -1 || newGear >= m_gearCount) return;
    else if (newGear != -1) {
        const double m_car = m_vehicle->getMass();
        const double gear_ratio = m_gearRatios[newGear];
        const double diff_ratio = m_vehicle->getDiffRatio();
        const double tire_radius = m_vehicle->getTireRadius();
        const double f = tire_radius / (diff_ratio * gear_ratio);

        const double new_I = m_car * f * f;
        const double E_r =
            0.5 * m_rotatingMass->I * m_rotatingMass->v_theta * m_rotatingMass->v_theta;
        const double new_v_theta = m_rotatingMass->v_theta < 0
            ? -std::sqrt(E_r * 2 / new_I)
            : std::sqrt(E_r * 2 / new_I);

        m_rotatingMass->I = new_I;
        m_rotatingMass->p_x = m_rotatingMass->p_y = 0;
        m_rotatingMass->m = m_car;
        m_rotatingMass->v_theta = new_v_theta;

        // Keep the driveshaft in step with the wheels after the ratio change
        if (m_output != nullptr && m_output != m_rotatingMass) {
            m_output->v_theta = new_v_theta;
        }
    }

    m_gear = newGear;
}
