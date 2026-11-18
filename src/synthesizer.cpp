#include "../include/synthesizer.h"

#include "../include/utilities.h"
#include "../include/delta.h"

#include <cassert>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>

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
    m_flutterPhase = 0;
    m_fiCounter = 0;
    m_mixEnvelope = 0;
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

    }
    updateBandwidth();
    m_inputPeak.assign(p.inputChannelCount, 0.0f);

    m_levelingFilter.p_target = m_audioParameters.levelerTarget;
    m_levelingFilter.setReleaseTime(0.4f, m_audioSampleRate);
    m_levelingFilter.p_maxLevel = m_audioParameters.levelerMaxGain;
    m_levelingFilter.p_minLevel = m_audioParameters.levelerMinGain;
    m_antialiasing.setCutoffFrequency(m_audioSampleRate * 0.45f, m_audioSampleRate);

    // Output stage: the gain follows the loudest recent peak with a 2 ms
    // attack, seen 3 ms ahead of the output so that it is in place before
    // the peak arrives, and recovers over seconds; the old leveler jumped
    // to each new peak within a sample (an audible step) and recovered in
    // 0.4 s, which pumped idle and part throttle up to full-throttle volume
    m_lookahead.assign(static_cast<size_t>(0.004f * m_audioSampleRate) + 1, 0.0f);
    m_lookaheadIndex = 0;
    m_outputPeak = 1e-3f;
    m_outputReference = 1e-3f;
    m_outputGain = 1.0f;
    m_peakRelease = std::exp(-1.0f / (0.35f * m_audioSampleRate));
    m_referenceRelease = std::exp(-1.0f / (40.0f * m_audioSampleRate));
    m_referenceAttack = 1.0f - std::exp(-1.0f / (0.8f * m_audioSampleRate));
    m_gainAttack = 1.0f - std::exp(-1.0f / (0.0005f * m_audioSampleRate));
    m_gainSmoothing = 1.0f - std::exp(-1.0f / (0.05f * m_audioSampleRate));

    const float fs = m_audioSampleRate;
    m_intakeDc.set(25.0f, fs);
    m_intakeBox.set(170.0f, 1.4f, fs);
    m_intakeSnorkel.set(560.0f, 2.2f, fs);
    m_intakeRoar.set(1700.0f, 0.7f, fs);
    m_valveBand.set(3800.0f, 1.3f, fs);
    m_whistleDrift.set(3.0f, fs);
    m_whistlePulse.set(600.0f, fs);
    m_hissLow.set(1300.0f, fs);
    m_hissBand.set(3000.0f, 0.7f, fs);
    m_turbineLow1.set(1700.0f, fs);
    m_turbineLow2.set(1700.0f, fs);
    m_blowoffFollow = 1.0f - std::exp(-1.0f / (0.2f * fs));
    m_whineSmoothing = 1.0f - std::exp(-1.0f / (0.1f * fs));
    m_spoolMemoryRelease = std::exp(-1.0f / (0.6f * fs));
    m_raspBand.set(2400.0f, 0.6f, fs);
    m_cabinBoom.set(95.0f, 1.3f, fs);
    m_cabinLow1.set(1500.0f, fs);
    m_cabinLow2.set(1500.0f, fs);
    m_layerLow1.set(2500.0f, fs);
    m_layerLow2.set(2500.0f, fs);
    m_bayLow.set(3200.0f, fs);

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
        m_layerBuffer.resize(n);
        m_envelope.resize(n);
        m_popBuffer.resize(n);
        m_renderedAudio.resize(n);
    }

    const float airNoise = parameters.airNoise;
    const float dF_F_mix = parameters.dF_F_mix;
    const bool engineChannels = (m_auxChannelCount >= AuxChannels);

    std::fill(m_mixBuffer.begin(), m_mixBuffer.begin() + n, 0.0f);
    std::fill(m_layerBuffer.begin(), m_layerBuffer.begin() + n, 0.0f);

    // Overrun pops, shared by the exhaust channels (each scales them to its
    // own level)
    const float *overrun = engineChannels
        ? m_inputChannels[m_inputChannelCount + ForcedInductionChannels + 2].transferBuffer
        : nullptr;
    for (int j = 0; j < n; ++j) {
        m_popBuffer[j] = (overrun != nullptr) ? overrunPop(overrun[j], parameters.overrunCrackle) : 0.0f;
    }

    const float peakRelease = std::exp(-1.0f / (8.0f * m_audioSampleRate));
    for (int i = 0; i < m_inputChannelCount; ++i) {
        ProcessingFilters &filters = m_filters[i];
        filters.airNoiseLowPass.setCutoffFrequency(
            static_cast<float>(parameters.airNoiseFrequencyCutoff), m_audioSampleRate);
        filters.jitterFilter.setJitterScale(parameters.inputSampleNoise);

        const float *transfer = m_inputChannels[i].transferBuffer;
        float &inputPeak = m_inputPeak[i];
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
            if (!std::isfinite(v_in) || std::fpclassify(v_in) == FP_SUBNORMAL) {
                v_in = 0;
            }

            // The pops are sized by the loudest recent exhaust pulses
            inputPeak = std::max(std::abs(v_in), inputPeak * peakRelease);
            v_in += m_popBuffer[j] * inputPeak;

            m_channelInput[j] = v_in;
        }

        filters.convolution.process(
            m_channelInput.data(), m_channelOutput.data(), n, parameters.convolution);

        for (int j = 0; j < n; ++j) {
            m_mixBuffer[j] += m_channelOutput[j];
        }
    }

    renderEngineLayers(n, parameters);
    renderForcedInduction(n, parameters);

    // Listener perspective: the exhaust heard through the cabin is muffled
    // and booms; by the engine it is distant. The other layers (intake,
    // valvetrain, forced induction) are mostly heard through the firewall
    // from the cockpit.
    const int perspective = std::clamp(parameters.perspective, 0, 2);
    if (perspective != m_perspective) {
        m_cabinLow1.z = m_cabinLow2.z = m_layerLow1.z = m_layerLow2.z = m_bayLow.z = 0;
        m_perspective = perspective;
    }
    for (int j = 0; j < n; ++j) {
        float exhaust = m_mixBuffer[j];
        float layers = m_layerBuffer[j];
        if (perspective == 1) {
            const float muffled = m_cabinLow2.process(m_cabinLow1.process(exhaust));
            exhaust = 0.75f * muffled + 1.1f * m_cabinBoom.process(exhaust);
            layers = m_layerLow2.process(m_layerLow1.process(layers));
        }
        else if (perspective == 2) {
            exhaust = 0.55f * m_bayLow.process(exhaust);
        }
        m_mixBuffer[j] = exhaust + layers;
    }

    // Output: look-ahead loudness stage, then a soft clipper so that what
    // still gets through is rounded instead of cut flat
    const float target = parameters.levelerTarget;
    const int lookahead = static_cast<int>(m_lookahead.size());
    for (int j = 0; j < n; ++j) {
        float signal = m_antialiasing.fast_f(m_mixBuffer[j]);
        if (!std::isfinite(signal)) signal = 0;

        // Two followers: the recent peak and the loudest the engine has
        // been lately (sustained for about a second, released over ~40 s,
        // so that a start-up flare or a pop doesn't set it). The loudest is
        // brought to the
        // target; quieter sound is lifted only part of the way (60 % in dB),
        // so idle stays quieter than full throttle without dropping out
        const float level = std::abs(signal);
        // (the gain itself is smoothed and looks ahead, so the follower can
        // take each peak at once)
        m_outputPeak = std::max(level, std::max(m_outputPeak * m_peakRelease, 1e-3f));
        if (m_outputPeak > m_outputReference) m_outputReference += (m_outputPeak - m_outputReference) * m_referenceAttack;
        else m_outputReference = std::max(m_outputReference * m_referenceRelease, 1e-3f);

        const float wanted = std::clamp(
            std::min(target / m_outputReference * std::pow(m_outputReference / m_outputPeak, 0.6f), target / m_outputPeak),
            parameters.levelerMinGain, 64.0f);
        m_outputGain += (wanted - m_outputGain) * ((wanted < m_outputGain) ? m_gainAttack : m_gainSmoothing);

        float delayed = signal;
        if (lookahead > 1) {
            delayed = m_lookahead[m_lookaheadIndex];
            m_lookahead[m_lookaheadIndex] = signal;
            m_lookaheadIndex = (m_lookaheadIndex + 1) % lookahead;
        }

        float v = delayed * m_outputGain * parameters.volume;
        constexpr float Knee = 24000.0f;
        constexpr float Room = 32767.0f - Knee;
        const float a = std::abs(v);
        if (a > Knee) {
            v = std::copysign(Knee + Room * std::tanh((a - Knee) / Room), v);
        }

        m_renderedAudio[j] = static_cast<int16_t>(std::clamp(std::lround(v), -32767L, 32767L));
    }
}

float Synthesizer::overrunPop(float overrun, float amount) {
    // Unburnt fuel lighting off in the exhaust after a lift: random pops at
    // up to ~25 per second, each a sharp burst that dies within a few ms
    if (m_popEnvelope > 1e-3f) {
        m_popEnvelope *= m_popDecay;
    }
    else {
        m_popEnvelope = 0;
        const float rate = 25.0f * std::clamp(overrun, 0.0f, 1.0f) * std::clamp(amount, 0.0f, 1.0f);
        if (rate > 0 && 0.5f * (noise() + 1.0f) < rate / m_audioSampleRate) {
            const float u = 0.5f * (noise() + 1.0f);
            m_popEnvelope = 1.0f;
            m_popAmplitude = (0.5f + 1.5f * u * u) * std::clamp(amount, 0.0f, 1.0f) * 1.6f;
            m_popDecay = std::exp(-1.0f / ((0.0015f + 0.006f * 0.5f * (noise() + 1.0f)) * m_audioSampleRate));
        }
    }
    return m_popAmplitude * m_popEnvelope * noise();
}

void Synthesizer::OnePole::set(float fc, float fs) {
    a = 1.0f - std::exp(-2.0f * 3.14159265f * fc / fs);
}

// Intake and valvetrain layers, driven by control channels from the
// simulation: [0] flow through the throttle, [1] throttle opening (as flow,
// 0 to 1), [2] overrun (0 to 1), [3] firing frequency [Hz]. Their loudness
// follows the exhaust's, so they need no calibration per engine.
void Synthesizer::renderEngineLayers(int n, const AudioParameters &parameters) {
    const float fs = m_audioSampleRate;
    const float envelopeDecay = 1.0f - 1.0f / (0.3f * fs);
    for (int j = 0; j < n; ++j) {
        const float a = std::abs(m_mixBuffer[j]);
        m_mixEnvelope = std::max(a * (1 - envelopeDecay) + m_mixEnvelope * envelopeDecay, 0.0f);
        m_envelope[j] = m_mixEnvelope;
    }

    if (m_auxChannelCount < AuxChannels) return;
    const float *intake = m_inputChannels[m_inputChannelCount + ForcedInductionChannels + 0].transferBuffer;
    const float *opening = m_inputChannels[m_inputChannelCount + ForcedInductionChannels + 1].transferBuffer;
    const float *firing = m_inputChannels[m_inputChannelCount + ForcedInductionChannels + 3].transferBuffer;

    const int perspective = std::clamp(parameters.perspective, 0, 2);
    const float intakeWeights[] = { 0.3f, 0.55f, 1.1f };
    const float valveWeights[] = { 0.06f, 0.14f, 0.35f };
    const float intakeGain = parameters.intakeVolume * intakeWeights[perspective];
    const float valveGain = parameters.mechanicalVolume * valveWeights[perspective];
    const float peakRelease = std::exp(-1.0f / (4.0f * fs));
    const float valveDecay = std::exp(-1.0f / (0.0012f * fs));

    const float raspWeights[] = { 1.0f, 0.5f, 0.6f };
    const float raspGain = 0.22f * raspWeights[perspective];
    for (int j = 0; j < n; ++j) {
        // Exhaust rasp: turbulent flow noise at the tailpipe, pulsing with
        // the exhaust and growing with the load
        const float open = std::clamp(opening[j], 0.0f, 1.0f);
        const float pulse = std::abs(m_mixBuffer[j]);
        m_mixBuffer[j] += raspGain * (0.25f + 0.75f * open) * pulse * m_raspBand.process(noise());

        // Induction: the pulsing flow through the throttle excites the air
        // box and the inlet snorkel; with the throttle open the roar of the
        // air rushing in joins them
        const float x = intake[j] - m_intakeDc.process(intake[j]);
        const float resonant = m_intakeBox.process(x) + 0.6f * m_intakeSnorkel.process(x);
        const float roar = m_intakeRoar.process(noise()) * std::abs(x);
        const float raw = resonant + 2.0f * roar;
        m_intakePeak = std::max(std::abs(raw), m_intakePeak * peakRelease);
        const float load = open * std::sqrt(open);
        const float induction = (m_intakePeak > 1e-9f) ? raw / m_intakePeak : 0.0f;

        // Valvetrain: a tick for every valve event (two per firing)
        const float f = std::max(firing[j], 0.0f);
        m_valvePhase += 2.0 * f / fs;
        if (m_valvePhase >= 1.0) {
            m_valvePhase -= std::floor(m_valvePhase);
            m_valveEnvelope = 1.0f;
            m_valveAmplitude = 0.6f + 0.4f * std::abs(noise());
        }
        m_valveEnvelope *= valveDecay;
        const float speed = std::clamp(f / 400.0f, 0.0f, 1.5f);
        const float tick = m_valveBand.process(noise()) * m_valveEnvelope * m_valveAmplitude * speed;

        m_layerBuffer[j] += m_envelope[j] * (
            intakeGain * (0.15f + 0.85f * load) * induction
            + valveGain * 6.0f * tick);
    }
}

void Synthesizer::Bandpass::set(float f, float q, float fs) {
    const float w0 = 2.0f * 3.14159265f * std::clamp(f, 20.0f, 0.45f * fs) / fs;
    const float alpha = std::sin(w0) / (2.0f * q);
    const float a0 = 1.0f + alpha;
    b0 = alpha / a0;
    b2 = -alpha / a0;
    a1 = -2.0f * std::cos(w0) / a0;
    a2 = (1.0f - alpha) / a0;
}

float Synthesizer::Bandpass::process(float x) {
    const float y = b0 * x + b2 * x2 - a1 * y1 - a2 * y2;
    x2 = x1; x1 = x;
    y2 = y1; y1 = y;
    if (std::fpclassify(y1) == FP_SUBNORMAL) y1 = 0;
    return y;
}

// Turbocharger and supercharger sounds, driven by control channels from the
// simulation: [0] shaft or rotor frequency [Hz] (times the whistle pitch),
// [1] whine level, [2] blow-off flow, [3] compressor flow, [4] surge,
// [5] external wastegate venting, [6] mode (10 * type + blow-off type;
// types 1 turbo, 2 Roots, 3 twin-screw, 4 centrifugal; blow-off 0 vented,
// 1 recirculating, 2 none)
namespace {
    float smoothstep(float a, float b, float x) {
        const float t = std::clamp((x - a) / (b - a), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }
}

void Synthesizer::renderForcedInduction(int n, const AudioParameters &parameters) {
    if (m_auxChannelCount < ForcedInductionChannels) return;

    const float *const *aux = m_auxPointers;
    for (int k = 0; k < ForcedInductionChannels; ++k) m_auxPointers[k] = m_inputChannels[m_inputChannelCount + k].transferBuffer;

    const int mode = static_cast<int>(std::lround(aux[6][0]));
    const int type = mode / 10;
    const int blowoffType = mode % 10;
    if (type == 0) return;

    const float perspectiveWeights[] = { 1.0f, 0.8f, 1.35f };
    const float volume = parameters.forcedInductionVolume * perspectiveWeights[std::clamp(parameters.perspective, 0, 2)];
    const float fs = m_audioSampleRate;
    const float floor = 0.08f * parameters.levelerTarget;
    const double twoPi = 2 * 3.14159265358979323846;

    for (int j = 0; j < n; ++j) {
        // The sounds follow the loudness of the engine (with a floor so that
        // a blow-off on a quiet overrun is still heard)
        const float envelope = m_envelope[j];
        const float reference = 0.8f * envelope + floor;

        const float shaft = std::max(aux[0][j], 0.0f);
        const float whine = std::max(aux[1][j], 0.0f);
        const float blowoff = std::max(aux[2][j], 0.0f);
        const float flow = std::max(aux[3][j], 0.0f);
        const float surge = std::max(aux[4][j], 0.0f);
        const float wastegate = std::max(aux[5][j], 0.0f);

        // Filter centres follow the shaft speed; updated every 16 samples
        float tonal = 0;
        if (type == 1 || type == 4) {
            // Turbocharger (and centrifugal supercharger) sound, as heard on
            // recordings of real cars:
            // - the whistle: a single narrow band of rushing air at a pitch
            //   that follows the shaft, from about 1.5 kHz as the turbo
            //   wakes up to 5-6 kHz on full boost, with a faint wandering
            //   tone in it and a much weaker inharmonic shimmer above (the
            //   blades). It is silent until the turbo is really working:
            //   at idle and light cruise a real turbo isn't heard. (It was
            //   two tones a fifth apart, which sounded like a chord on a
            //   slide whistle, and was heard at idle.)
            // - the breathing: broadband hiss from the compressor inlet
            //   that grows with the air flowing through it
            const float k = (type == 1) ? 3.6f : 2.8f;
            const float pitch = std::clamp(900.0f + k * shaft, 900.0f, 9500.0f);
            if ((m_fiCounter++ & 15) == 0) {
                m_whistleBand.set(pitch, 7.0f, fs);
                m_inducerBand.set(2.37f * pitch, 12.0f, fs);
            }

            const float air = m_whistleBand.process(noise());
            m_whistleRms += (air * air - m_whistleRms) * (1.0f / (0.05f * fs));
            const float airNormalized = air / std::sqrt(std::max(m_whistleRms, 1e-12f));

            // The tone's pitch drifts by a few tenths of a percent
            const float drift = m_whistleDrift.process(noise());
            m_whinePhase += twoPi * pitch * (1.0 + 0.3 * drift) / fs;
            if (m_whinePhase > twoPi) m_whinePhase -= twoPi;
            const float tone = static_cast<float>(std::sin(m_whinePhase));
            const float shimmer = (2.37f * pitch < 0.45f * fs) ? m_inducerBand.process(noise()) * 6.0f : 0.0f;

            // A turbo's turbine is driven by the exhaust pulses, so its
            // whistle flutters a little with the engine
            float pulse = 1.0f;
            if (type == 1) {
                const float ratio = std::clamp(std::abs(m_mixBuffer[j]) / std::max(envelope, 1e-6f), 0.0f, 3.0f);
                pulse = 0.85f + 0.15f * m_whistlePulse.process(ratio);
            }

            // The whistle is loudest with air going through the compressor:
            // when the throttle shuts, it falls away and spools down
            const float spool = smoothstep(0.04f, 0.6f, whine) * (0.3f + 0.7f * smoothstep(0.08f, 0.4f, flow));
            tonal += 0.42f * spool * pulse * (0.65f * airNormalized + 0.3f * tone + 0.12f * shimmer);

            // Inlet hiss: high-passed noise, peaking around 3 kHz
            const float r = noise();
            const float hiss = m_hissBand.process(r - m_hissLow.process(r));
            tonal += 0.3f * std::pow(flow, 1.5f) * hiss;
        }
        else {
            // Supercharger rotor whine: lobe passing frequency with harmonics
            const double lobes = (type == 2) ? 6.0 : 5.0;
            const double f = std::min(shaft * lobes, 0.2 * fs);
            m_whinePhase += twoPi * f / fs;
            if (m_whinePhase > twoPi) m_whinePhase -= twoPi;
            const double p = m_whinePhase;
            float tone = 0;
            if (type == 2) {
                tone = static_cast<float>(std::sin(p) + 0.55 * std::sin(2 * p) + 0.35 * std::sin(3 * p) + 0.2 * std::sin(4 * p));
            }
            else {
                tone = static_cast<float>(std::sin(p) + 0.45 * std::sin(3 * p) + 0.25 * std::sin(5 * p));
            }

            if ((m_fiCounter++ & 31) == 0) {
                m_whistleBand.set(static_cast<float>(2.0 * f + 200.0), 6.0f, fs);
            }
            tonal += whine * (0.45f * tone + 1.5f * m_whistleBand.process(noise()));
        }

        // Compressor surge without a blow-off valve: a fluttering chop
        float bursts = 0;
        if (surge > 0.01f) {
            m_flutterPhase += twoPi * (11.0 + 7.0 * surge) / fs;
            if (m_flutterPhase > twoPi) m_flutterPhase -= twoPi;
            const float gate = static_cast<float>(std::pow(std::max(std::sin(m_flutterPhase), 0.0), 3.0));
            if ((m_fiCounter & 31) == 1) m_flutterBand.set(380.0f, 1.6f, fs);
            bursts += 2.0f * surge * gate * m_flutterBand.process(noise());
        }

        // Blow-off: a sharp vented "pssh" or a soft recirculating whoosh.
        // The valve stays open while the throttle is shut and the spinning
        // compressor keeps pumping air through it, so its flow alone hissed
        // on through every overrun and at idle; what is heard is the burst
        // of boost escaping when it opens, so the sound follows the rise
        // in flow above its recent level (a burst of a few tenths of a second)
        // (it starts at the valve's flow, so that an engine that idles
        // with the valve open doesn't puff when the sound starts)
        if (m_blowoffSlow < 0) m_blowoffSlow = blowoff;
        m_blowoffSlow += (blowoff - m_blowoffSlow) * m_blowoffFollow;
        // Only boost makes the burst: the valve opening without boost
        // behind it (an engine starting, or a blip that never built any)
        // is silent
        m_whineSmooth += (whine - m_whineSmooth) * m_whineSmoothing;
        m_spoolMemory = std::max(m_whineSmooth, m_spoolMemory * m_spoolMemoryRelease);
        const float burst = std::clamp((blowoff - m_blowoffSlow) * 1.6f, 0.0f, 1.0f)
            * smoothstep(0.08f, 0.4f, m_spoolMemory);
        if (burst > 0.001f) {
            if ((m_fiCounter & 31) == 2) {
                m_blowoffBand1.set(blowoffType == 1 ? 1200.0f : 3200.0f, blowoffType == 1 ? 1.0f : 0.8f, fs);
                m_blowoffBand2.set(7000.0f, 1.1f, fs);
            }
            const float r = noise();
            bursts += (blowoffType == 1)
                ? 2.0f * burst * m_blowoffBand1.process(r)
                : burst * (4.0f * m_blowoffBand1.process(r) + 1.6f * m_blowoffBand2.process(r));
        }

        // External wastegate venting through its own pipe
        if (wastegate > 0.01f) {
            if ((m_fiCounter & 31) == 3) m_wastegateBand.set(1500.0f, 2.5f, fs);
            bursts += 0.6f * wastegate * m_wastegateBand.process(noise());
        }

        m_layerBuffer[j] += volume * (tonal * (1.1f * envelope + 0.4f * floor) + bursts * reference * 1.5f);

        // The turbine sits in the exhaust and soaks up the sharp edge of
        // each pulse: a turbocharged engine's exhaust is smoother and
        // duller than the same engine's without one
        if (type == 1) {
            const float x = m_mixBuffer[j];
            m_mixBuffer[j] = 0.45f * x + 0.7f * m_turbineLow2.process(m_turbineLow1.process(x));
        }
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

void Synthesizer::updateBandwidth() {
    // The exhaust signal is band limited to 40 % of the simulation's sample
    // rate (above that the linear upsampling only adds images), at most
    // 7 kHz. It used to be cut at 1.9 kHz whatever the rate, which removed
    // the rasp and crackle of a real exhaust and left a boom.
    if (m_filters == nullptr || m_audioSampleRate <= 0) return;
    const float cutoff = std::clamp(0.4f * m_inputSampleRate, 400.0f, 7000.0f);
    for (int i = 0; i < m_inputChannelCount; ++i) {
        m_filters[i].antialiasing.setCutoffFrequency(cutoff, m_audioSampleRate);
    }
}

void Synthesizer::setInputSampleRate(double sampleRate) {
    if (sampleRate != m_inputSampleRate) {
        std::lock_guard<std::mutex> lock(m_lock0);
        std::lock_guard<std::mutex> inputLock(m_inputLock);
        m_inputSampleRate = sampleRate;

        // The input filters run in writeInput(), under the input lock
        updateBandwidth();
    }
}

double Synthesizer::getLevelerGain() {
    std::lock_guard<std::mutex> lock(m_lock0);
    return m_outputGain;
}

Synthesizer::AudioParameters Synthesizer::getAudioParameters() {
    std::lock_guard<std::mutex> lock(m_lock0);
    return m_audioParameters;
}

void Synthesizer::setAudioParameters(const AudioParameters &params) {
    std::lock_guard<std::mutex> lock(m_lock0);
    m_audioParameters = params;
}
