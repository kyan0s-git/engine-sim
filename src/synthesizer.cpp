#include "../include/synthesizer.h"

#include "../include/utilities.h"
#include "../include/delta.h"

#include <cassert>
#include <algorithm>
#include <chrono>
#include <cmath>

#undef min
#undef max

Synthesizer::Synthesizer() {
    m_inputChannels = nullptr;
    m_inputChannelCount = 0;
    m_auxChannelCount = 0;
    m_totalChannelCount = 0;
    m_inputBufferSize = 0;
    m_inputWriteOffset = 0.0;
    m_inputSamplesRead = 0;

    m_audioBufferSize = 0;

    m_inputSampleRate = 0.0;
    m_audioSampleRate = 0.0;

    m_lastInputSampleOffset = 0.0;

    m_run = true;
    m_thread = nullptr;
    m_filters = nullptr;

    m_latency = 0;
    m_noiseState = 0x9E3779B9u;

    m_whinePhase = 0;
    m_mixEnvelope = 0;
    m_hissLow = 0;
    m_hissHigh = 0;
}

Synthesizer::~Synthesizer() {
    assert(m_inputChannels == nullptr);
    assert(m_thread == nullptr);
    assert(m_filters == nullptr);
}

void Synthesizer::initialize(const Parameters &p) {
    m_inputChannelCount = p.inputChannelCount;
    m_auxChannelCount = p.auxChannelCount;
    m_totalChannelCount = p.inputChannelCount + p.auxChannelCount;
    m_inputBufferSize = p.inputBufferSize;
    m_inputWriteOffset = p.inputBufferSize;
    m_audioBufferSize = p.audioBufferSize;
    m_inputSampleRate = p.inputSampleRate;
    m_audioSampleRate = p.audioSampleRate;
    m_audioParameters = p.initialAudioParameters;

    m_inputSamplesRead = 0;

    m_inputWriteOffset = 0;

    m_audioBuffer.initialize(p.audioBufferSize);
    m_inputChannels = new InputChannel[m_totalChannelCount];
    for (int i = 0; i < m_totalChannelCount; ++i) {
        m_inputChannels[i].transferBuffer = new float[p.inputBufferSize];
        m_inputChannels[i].data.initialize(p.inputBufferSize);
    }

    m_filters = new ProcessingFilters[p.inputChannelCount];
    for (int i = 0; i < p.inputChannelCount; ++i) {
        m_filters[i].airNoiseLowPass.setCutoffFrequency(
            m_audioParameters.airNoiseFrequencyCutoff, m_audioSampleRate);

        m_filters[i].derivative.m_dt = 1 / m_audioSampleRate;

        m_filters[i].inputDcFilter.setCutoffFrequency(10.0);
        m_filters[i].inputDcFilter.m_dt = 1 / m_audioSampleRate;

        m_filters[i].jitterFilter.initialize(
            10,
            m_audioParameters.inputSampleNoiseFrequencyCutoff,
            m_audioSampleRate);

        m_filters[i].antialiasing.setCutoffFrequency(1900.0f, m_audioSampleRate);
    }

    m_levelingFilter.p_target = m_audioParameters.levelerTarget;
    m_levelingFilter.setReleaseTime(0.4f, m_audioSampleRate);
    m_levelingFilter.p_maxLevel = m_audioParameters.levelerMaxGain;
    m_levelingFilter.p_minLevel = m_audioParameters.levelerMinGain;
    m_antialiasing.setCutoffFrequency(m_audioSampleRate * 0.45f, m_audioSampleRate);

}

void Synthesizer::initializeImpulseResponse(
    const int16_t *impulseResponse,
    unsigned int samples,
    float volume,
    int index)
{
    unsigned int clippedLength = 0;
    for (unsigned int i = 0; i < samples; ++i) {
        if (std::abs(impulseResponse[i]) > 100) {
            clippedLength = i + 1;
        }
    }

    const unsigned int sampleCount = std::max(1U, std::min(10000U, clippedLength));
    m_filters[index].convolution.initialize(sampleCount);
    for (unsigned int i = 0; i < sampleCount && i < samples; ++i) {
        m_filters[index].convolution.getImpulseResponse()[i] =
            volume * impulseResponse[i] / INT16_MAX;
    }

    m_filters[index].convolution.prepare();
}

void Synthesizer::startAudioRenderingThread() {
    m_run = true;
    m_thread = new std::thread(&Synthesizer::audioRenderingThread, this);
}

void Synthesizer::endAudioRenderingThread() {
    if (m_thread != nullptr) {
        {
            std::lock_guard<std::mutex> lock(m_lock0);
            m_run = false;
        }
        m_cv0.notify_all();

        m_thread->join();
        delete m_thread;

        m_thread = nullptr;
    }
}

void Synthesizer::destroy() {
    m_audioBuffer.destroy();

    for (int i = 0; i < m_totalChannelCount; ++i) {
        m_inputChannels[i].data.destroy();
        delete[] m_inputChannels[i].transferBuffer;
        m_inputChannels[i].transferBuffer = nullptr;
    }

    for (int i = 0; i < m_inputChannelCount; ++i) {
        m_filters[i].convolution.destroy();
        m_filters[i].jitterFilter.destroy();
    }

    delete[] m_inputChannels;
    delete[] m_filters;

    m_inputChannels = nullptr;
    m_filters = nullptr;

    m_inputChannelCount = 0;
    m_auxChannelCount = 0;
    m_totalChannelCount = 0;
}

int Synthesizer::readAudioOutput(int samples, int16_t *buffer) {
    std::lock_guard<std::mutex> lock(m_lock0);

    const int newDataLength = m_audioBuffer.size();
    if (newDataLength >= samples) {
        m_audioBuffer.readAndRemove(samples, buffer);
    }
    else {
        m_audioBuffer.readAndRemove(newDataLength, buffer);
        memset(
            buffer + newDataLength,
            0,
            sizeof(int16_t) * ((size_t)samples - newDataLength));
    }
    
    const int samplesConsumed = std::min(samples, newDataLength);

    // There is room for more rendered audio now
    m_cv0.notify_one();

    return samplesConsumed;
}

void Synthesizer::writeInput(const double *data) {
    std::lock_guard<std::mutex> lock(m_inputLock);

    m_inputWriteOffset += (double)m_audioSampleRate / m_inputSampleRate;
    if (m_inputWriteOffset >= (double)m_inputBufferSize) {
        m_inputWriteOffset -= (double)m_inputBufferSize;
    }

    for (int i = 0; i < m_totalChannelCount; ++i) {
        RingBuffer<float> &buffer = m_inputChannels[i].data;
        const bool aux = (i >= m_inputChannelCount);

        // Never let the ring overflow (its size would wrap around); drop
        // the oldest input instead. The simulator keeps the queue far below
        // this unless audio isn't being consumed at all.
        if (buffer.size() + 256 >= static_cast<size_t>(m_inputBufferSize)) {
            buffer.removeBeginning(1024);
        }
        const double lastInputSample = m_inputChannels[i].lastInputSample;
        const size_t baseIndex = buffer.writeIndex();
        const double distance =
            inputDistance(m_inputWriteOffset, m_lastInputSampleOffset);
        double s =
            inputDistance(baseIndex, m_lastInputSampleOffset);
        for (; s <= distance; s += 1.0) {
            if (s >= m_inputBufferSize) s -= m_inputBufferSize;

            const double f = s / distance;
            const double sample = lastInputSample * (1 - f) + data[i] * f;

            buffer.write(aux
                ? static_cast<float>(sample)
                : static_cast<float>(m_filters[i].antialiasing.fast_f(sample)));
        }

        m_inputChannels[i].lastInputSample = data[i];
    }

    m_lastInputSampleOffset = m_inputWriteOffset;
}

void Synthesizer::endInputBlock() {
    {
        std::lock_guard<std::mutex> lock(m_inputLock);
        m_latency = (m_inputChannelCount != 0)
            ? static_cast<int>(m_inputChannels[0].data.size())
            : 0;
    }

    m_cv0.notify_one();
}

void Synthesizer::audioRenderingThread() {
    while (m_run) {
        renderAudio();
    }
}

void Synthesizer::renderAudio() {
    // Renders whatever input is available as soon as there is room for it.
    // Rendering used to happen at most once per frame and was capped at
    // 2000 samples, so frames longer than ~45 ms starved the output.
    static constexpr int MinBlock = 64;

    int n = 0;
    AudioParameters parameters;
    {
        std::unique_lock<std::mutex> lk0(m_lock0);

        auto ready = [this] {
            if (!m_run) return true;
            if (m_inputChannelCount == 0) return false;

            const int space = MaxRenderedSamples - static_cast<int>(m_audioBuffer.size());
            if (space <= 0) return false;

            std::lock_guard<std::mutex> lock(m_inputLock);
            const int available = static_cast<int>(m_inputChannels[0].data.size());
            return available >= std::min(MinBlock, space) && available > 0;
        };

        // The timeout covers notifications that arrive between the check
        // and the wait (the input is written without holding m_lock0)
        m_cv0.wait_for(lk0, std::chrono::milliseconds(2), ready);
        if (!m_run || !ready()) return;

        const int space = MaxRenderedSamples - static_cast<int>(m_audioBuffer.size());
        parameters = m_audioParameters;

        std::lock_guard<std::mutex> lock(m_inputLock);
        n = std::min(space, static_cast<int>(m_inputChannels[0].data.size()));
        for (int i = 0; i < m_totalChannelCount; ++i) {
            m_inputChannels[i].data.read(n, m_inputChannels[i].transferBuffer);
            m_inputChannels[i].data.removeBeginning(n);
        }
    }

    renderBlock(n, parameters);

    // Publish the rendered samples
    std::lock_guard<std::mutex> lock(m_lock0);
    for (int i = 0; i < n; ++i) {
        m_audioBuffer.write(m_renderedAudio[i]);
    }
}

void Synthesizer::renderBlock(int n, const AudioParameters &parameters) {
    if (n <= 0) return;

    if ((int)m_mixBuffer.size() < n) {
        m_channelInput.resize(n);
        m_channelOutput.resize(n);
        m_mixBuffer.resize(n);
        m_renderedAudio.resize(n);
    }

    const float airNoise = parameters.airNoise;
    const float dF_F_mix = parameters.dF_F_mix;

    std::fill(m_mixBuffer.begin(), m_mixBuffer.begin() + n, 0.0f);

    for (int i = 0; i < m_inputChannelCount; ++i) {
        ProcessingFilters &filters = m_filters[i];
        filters.airNoiseLowPass.setCutoffFrequency(
            static_cast<float>(parameters.airNoiseFrequencyCutoff), m_audioSampleRate);
        filters.jitterFilter.setJitterScale(parameters.inputSampleNoise);

        const float *transfer = m_inputChannels[i].transferBuffer;
        for (int j = 0; j < n; ++j) {
            const float f_in = filters.jitterFilter.fast_f(transfer[j]);
            const float f_dc = filters.inputDcFilter.fast_f(f_in);
            const float f = f_in - f_dc;
            const float f_p = filters.derivative.f(f_in);

            // Each channel has its own noise filter; previously every channel
            // shared the first channel's filter which skewed its cutoff
            // frequency for engines with multiple exhaust systems
            const float r = filters.airNoiseLowPass.fast_f(noise());
            const float r_mixed = airNoise * r + (1 - airNoise);

            float v_in =
                f_p * dF_F_mix
                + f * r_mixed * (1 - dF_F_mix);
            if (std::fpclassify(v_in) == FP_SUBNORMAL) {
                v_in = 0;
            }

            m_channelInput[j] = v_in;
        }

        filters.convolution.process(
            m_channelInput.data(), m_channelOutput.data(), n, parameters.convolution);

        for (int j = 0; j < n; ++j) {
            m_mixBuffer[j] += m_channelOutput[j];
        }
    }

    renderForcedInduction(n, parameters);

    m_levelingFilter.p_target = parameters.levelerTarget;
    for (int j = 0; j < n; ++j) {
        const float signal = m_antialiasing.fast_f(m_mixBuffer[j]);
        const float v_leveled = m_levelingFilter.f(signal) * parameters.volume;

        long r_int = std::lround(v_leveled);
        if (r_int > INT16_MAX) {
            r_int = INT16_MAX;
        }
        else if (r_int < INT16_MIN) {
            r_int = INT16_MIN;
        }

        m_renderedAudio[j] = static_cast<int16_t>(r_int);
    }
}

void Synthesizer::renderForcedInduction(int n, const AudioParameters &parameters) {
    if (m_auxChannelCount < 3) return;

    const float *frequency = m_inputChannels[m_inputChannelCount + 0].transferBuffer;
    const float *whine = m_inputChannels[m_inputChannelCount + 1].transferBuffer;
    const float *blowoff = m_inputChannels[m_inputChannelCount + 2].transferBuffer;

    const float volume = parameters.forcedInductionVolume;
    const float envelopeDecay = 1.0f - 1.0f / (0.3f * m_audioSampleRate);
    const float floor = 0.1f * parameters.levelerTarget;
    const double twoPi = 2 * 3.14159265358979323846;
    const double nyquist = 0.45 * m_audioSampleRate;

    for (int j = 0; j < n; ++j) {
        // The forced induction sounds follow the loudness of the engine
        const float a = std::abs(m_mixBuffer[j]);
        m_mixEnvelope = std::max(a * (1 - envelopeDecay) + m_mixEnvelope * envelopeDecay, 0.0f);

        float out = 0;
        const double f = std::min(static_cast<double>(std::max(frequency[j], 0.0f)), nyquist / 2);
        m_whinePhase += twoPi * f / m_audioSampleRate;
        if (m_whinePhase > twoPi) m_whinePhase -= twoPi;

        const float whineLevel = std::max(whine[j], 0.0f);
        if (whineLevel > 0) {
            const float tone = static_cast<float>(
                std::sin(m_whinePhase) + 0.35 * std::sin(2 * m_whinePhase));
            out += whineLevel * tone * (0.8f * m_mixEnvelope + floor * 0.3f);
        }

        // Blow-off: band-limited hiss
        const float r = noise();
        m_hissLow += 0.35f * (r - m_hissLow);
        m_hissHigh += 0.02f * (m_hissLow - m_hissHigh);
        const float hiss = m_hissLow - m_hissHigh;
        const float blowoffLevel = std::max(blowoff[j], 0.0f);
        out += blowoffLevel * hiss * std::max(1.2f * m_mixEnvelope, 2.5f * floor);

        m_mixBuffer[j] += volume * out;
    }
}

double Synthesizer::getLatency() const {
    if (m_audioSampleRate <= 0) return 0;

    std::lock_guard<std::mutex> lock0(m_lock0);
    std::lock_guard<std::mutex> lock(m_inputLock);
    const size_t input = (m_inputChannelCount != 0) ? m_inputChannels[0].data.size() : 0;
    return static_cast<double>(input + m_audioBuffer.size()) / m_audioSampleRate;
}

int Synthesizer::inputDelta(int s1, int s0) const {
    return (s1 < s0)
        ? m_inputBufferSize - s0 + s1
        : s1 - s0;
}

double Synthesizer::inputDistance(double s1, double s0) const {
    return (s1 < s0)
        ? (double)m_inputBufferSize - s0 + s1
        : s1 - s0;
}

void Synthesizer::setInputSampleRate(double sampleRate) {
    if (sampleRate != m_inputSampleRate) {
        std::lock_guard<std::mutex> lock(m_lock0);
        m_inputSampleRate = sampleRate;
    }
}

double Synthesizer::getLevelerGain() {
    std::lock_guard<std::mutex> lock(m_lock0);
    return m_levelingFilter.getAttenuation();
}

Synthesizer::AudioParameters Synthesizer::getAudioParameters() {
    std::lock_guard<std::mutex> lock(m_lock0);
    return m_audioParameters;
}

void Synthesizer::setAudioParameters(const AudioParameters &params) {
    std::lock_guard<std::mutex> lock(m_lock0);
    m_audioParameters = params;
}
