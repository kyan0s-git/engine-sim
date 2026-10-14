#include "../include/leveling_filter.h"

#include <cmath>

LevelingFilter::LevelingFilter() {
    m_peak = 30000.0;
    m_attenuation = 1.0;
    m_release = 0.99995f;
    p_target = 30000.0;
    p_minLevel = 0.0;
    p_maxLevel = 1.0;
}

LevelingFilter::~LevelingFilter() {
    /* void */
}

void LevelingFilter::setReleaseTime(float seconds, float sampleRate) {
    m_release = (seconds > 0 && sampleRate > 0)
        ? std::exp(-1.0f / (seconds * sampleRate))
        : 0.0f;
}

float LevelingFilter::f(float sample) {
    // Peak follower with instant attack and a slow release. The gain is
    // derived from the peak directly, so the output never exceeds the
    // target; the previous version decayed the peak within ~20 ms and
    // smoothed the gain, which modulated the waveform within each cycle and
    // let peaks overshoot into clipping.
    m_peak *= m_release;
    const float level = std::abs(sample);
    if (level > m_peak) {
        m_peak = level;
    }

    float attenuation = (m_peak > 0) ? p_target / m_peak : p_maxLevel;
    if (attenuation < p_minLevel) attenuation = p_minLevel;
    else if (attenuation > p_maxLevel) attenuation = p_maxLevel;

    m_attenuation = attenuation;
    return sample * attenuation;
}
