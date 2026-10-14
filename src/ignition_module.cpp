#include "../include/ignition_module.h"

#include "../include/utilities.h"
#include "../include/constants.h"
#include "../include/units.h"

#include <cmath>

IgnitionModule::IgnitionModule() {
    m_plugs = nullptr;
    m_crankshaft = nullptr;
    m_timingCurve = nullptr;
    m_cylinderCount = 0;
    m_lastCrankshaftAngle = 0.0;
    m_enabled = false;
    m_revLimitTimer = 0.0;
    m_revLimit = 0;
    m_limiterDuration = 0;
}

IgnitionModule::~IgnitionModule() {
    assert(m_plugs == nullptr);
}

void IgnitionModule::destroy() {
    delete[] m_plugs;

    m_plugs = nullptr;
    m_cylinderCount = 0;
}

void IgnitionModule::initialize(const Parameters &params) {
    m_cylinderCount = params.cylinderCount;
    m_plugs = new SparkPlug[m_cylinderCount];
    m_crankshaft = params.crankshaft;
    m_timingCurve = params.timingCurve;
    m_revLimit = params.revLimit;
    m_limiterDuration = params.limiterDuration;
}

void IgnitionModule::setFiringOrder(int cylinderIndex, double angle) {
    assert(cylinderIndex < m_cylinderCount);

    m_plugs[cylinderIndex].angles.push_back(angle);
    m_plugs[cylinderIndex].enabled = true;
}

void IgnitionModule::reset() {
    m_lastCrankshaftAngle = m_crankshaft->getCycleAngle();
    resetIgnitionEvents();
}

void IgnitionModule::update(double dt) {
    const double cycleAngle = m_crankshaft->getCycleAngle();

    if (m_enabled && m_revLimitTimer == 0) {
        const double cycleLength = m_crankshaft->getCycleLength();
        const double advance = getTimingAdvance();

        for (int i = 0; i < m_cylinderCount; ++i) {
            if (!m_plugs[i].enabled) continue;

            for (const double angle : m_plugs[i].angles) {
                const double adjustedAngle = positiveMod(angle - advance, cycleLength);

                // Fire if the plug's angle was crossed during this step. The
                // comparison is done modulo the cycle length so that angles
                // just before the wrap-around point are not skipped.
                bool crossed;
                if (m_crankshaft->m_body.v_theta < 0) {
                    const double travel = positiveMod(cycleAngle - m_lastCrankshaftAngle, cycleLength);
                    const double d = positiveMod(adjustedAngle - m_lastCrankshaftAngle, cycleLength);
                    crossed = d < travel;
                }
                else {
                    const double travel = positiveMod(m_lastCrankshaftAngle - cycleAngle, cycleLength);
                    const double d = positiveMod(m_lastCrankshaftAngle - adjustedAngle, cycleLength);
                    crossed = d > 0 && d <= travel;
                }

                if (crossed) {
                    m_plugs[i].ignitionEvent = true;
                }
            }
        }
    }

    m_revLimitTimer -= dt;
    if (std::fabs(m_crankshaft->m_body.v_theta) > m_revLimit) {
        m_revLimitTimer = m_limiterDuration;
    }

    if (m_revLimitTimer < 0) {
        m_revLimitTimer = 0;
    }

    m_lastCrankshaftAngle = cycleAngle;
}

bool IgnitionModule::getIgnitionEvent(int index) const {
    return m_plugs[index].ignitionEvent;
}

void IgnitionModule::resetIgnitionEvents() {
    for (int i = 0; i < m_cylinderCount; ++i) {
        m_plugs[i].ignitionEvent = false;
    }
}

double IgnitionModule::getTimingAdvance() {
    return m_timingCurve->sampleTriangle(-m_crankshaft->m_body.v_theta) + m_timingOffset;
}

IgnitionModule::SparkPlug *IgnitionModule::getPlug(int i) {
    return &m_plugs[((i % m_cylinderCount) + m_cylinderCount) % m_cylinderCount];
}
