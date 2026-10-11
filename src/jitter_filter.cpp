#include "../include/jitter_filter.h"

JitterFilter::JitterFilter() {
    m_history = nullptr;
    m_maxJitter = 0;
    m_offset = 0;
    m_jitterScale = 0.0f;
}

JitterFilter::~JitterFilter() {
    destroy();
}

void JitterFilter::destroy() {
    delete[] m_history;
    m_history = nullptr;
    m_maxJitter = 0;
}

void JitterFilter::initialize(
    int maxJitter,
    float cutoffFrequency,
    float audioFrequency)
{
    destroy();

    m_maxJitter = maxJitter;
    m_history = new float[maxJitter];
    m_offset = 0;
    memset(m_history, 0, sizeof(float) * maxJitter);

    m_noiseFilter.setCutoffFrequency(cutoffFrequency, audioFrequency);
}

float JitterFilter::f(float sample) {
    return fast_f(sample);
}
