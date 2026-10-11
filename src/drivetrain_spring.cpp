#include "../include/drivetrain_spring.h"

#include <algorithm>
#include <cmath>

namespace {
    // Stiffness of the end stop relative to the spring itself
    constexpr double EndStopStiffnessFactor = 50.0;
} /* namespace */

DrivetrainSpring::DrivetrainSpring() {
    m_ks = 0.0;
    m_kd = 0.0;
    m_maxFlex = 0.0;
    m_limitFlex = false;
    m_maxTorque = -1.0;

    m_body1 = nullptr;
    m_body2 = nullptr;

    m_windup = 0.0;
    m_lastTorque = 0.0;
}

DrivetrainSpring::~DrivetrainSpring() {
    /* void */
}

void DrivetrainSpring::connect(atg_scs::RigidBody *body1, atg_scs::RigidBody *body2) {
    m_body1 = body1;
    m_body2 = body2;
    m_windup = 0.0;
    m_lastTorque = 0.0;
}

void DrivetrainSpring::disconnect() {
    m_body1 = m_body2 = nullptr;
}

double DrivetrainSpring::calculateTorque(double windup, double relativeVelocity) const {
    double torque = m_ks * windup + m_kd * relativeVelocity;

    if (m_limitFlex && m_maxFlex > 0 && std::abs(windup) > m_maxFlex) {
        const double overshoot = windup - std::copysign(m_maxFlex, windup);
        torque += EndStopStiffnessFactor * m_ks * overshoot;
    }

    if (m_maxTorque >= 0) {
        torque = std::max(-m_maxTorque, std::min(m_maxTorque, torque));
    }

    return torque;
}

void DrivetrainSpring::apply(atg_scs::SystemState *state) {
    if (m_body1 == nullptr || m_body2 == nullptr) return;

    const int i1 = m_body1->index;
    const int i2 = m_body2->index;
    if (i1 < 0 || i2 < 0) return;

    const double relativeVelocity = state->v_theta[i1] - state->v_theta[i2];
    const double torque = calculateTorque(m_windup, relativeVelocity);

    state->t[i1] -= torque;
    state->t[i2] += torque;

    m_lastTorque = torque;
}

void DrivetrainSpring::update(double dt) {
    if (m_body1 == nullptr || m_body2 == nullptr) return;

    m_windup += (m_body1->v_theta - m_body2->v_theta) * dt;

    // Slip: the spring can't hold more than the maximum torque so the excess
    // wind-up is released (e.g. the clutch disc slipping on the flywheel)
    if (m_maxTorque >= 0) {
        if (m_ks <= 0) {
            m_windup = 0;
        }
        else {
            const double maxWindup = m_maxTorque / m_ks;
            m_windup = std::max(-maxWindup, std::min(maxWindup, m_windup));
        }
    }
}
