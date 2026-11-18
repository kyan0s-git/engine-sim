#ifndef ATG_ENGINE_SIM_ENGINE_SYNTHESIZER_H
#define ATG_ENGINE_SIM_ENGINE_SYNTHESIZER_H

#include "convolution_filter.h"
#include "leveling_filter.h"
#include "derivative_filter.h"
#include "low_pass_filter.h"
#include "jitter_filter.h"
#include "ring_buffer.h"
#include "butterworth_low_pass_filter.h"

#include <cinttypes>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <vector>

class Synthesizer {
    public:
        struct AudioParameters {
            float volume = 1.0f;
            float convolution = 1.0f;
            float dF_F_mix = 0.01f;
            float inputSampleNoise = 0.5f;
            float inputSampleNoiseFrequencyCutoff = 10000.0f;
            float airNoise = 1.0f;
            float airNoiseFrequencyCutoff = 2000.0f;
            float levelerTarget = 27000.0f;
            float levelerMaxGain = 1.9f;
            float levelerMinGain = 0.00001f;

            // Turbocharger/supercharger whine and blow-off noise
            float forcedInductionVolume = 1.0f;

            // Engine layers besides the exhaust: induction roar (follows
            // the throttle opening), valvetrain clatter, and pops and
            // crackles from the exhaust on the overrun [0, 1]
            float intakeVolume = 1.0f;
            float mechanicalVolume = 1.0f;
            float overrunCrackle = 0.35f;

            // Where the listener is: 0 outside behind the car, 1 in the
            // cockpit, 2 by the engine
            int perspective = 0;
        };

        // Control channels after the exhaust channels
        static constexpr int ForcedInductionChannels = 7;
        static constexpr int EngineChannels = 4;
        static constexpr int AuxChannels = ForcedInductionChannels + EngineChannels;

        struct Parameters {
            int inputChannelCount = 1;

            // Control channels written after the exhaust channels:
            // see Synthesizer::renderForcedInduction()
            int auxChannelCount = 0;
            int inputBufferSize = 1024;
            int audioBufferSize = 44100;
            float inputSampleRate = 10000;
            float audioSampleRate = 44100;
            AudioParameters initialAudioParameters;
        };

        struct InputChannel {
            RingBuffer<float> data;
            float *transferBuffer = nullptr;
            double lastInputSample = 0.0f;
        };

        struct ProcessingFilters {
            ConvolutionFilter convolution;
            DerivativeFilter derivative;
            JitterFilter jitterFilter;
            ButterworthLowPassFilter<float> airNoiseLowPass;
            LowPassFilter inputDcFilter;
            ButterworthLowPassFilter<double> antialiasing;
        };

    public:
        Synthesizer();
        ~Synthesizer();

        void initialize(const Parameters &p);
        void initializeImpulseResponse(
            const int16_t *impulseResponse,
            unsigned int samples,
            float volume,
            int index);
        void startAudioRenderingThread();
        void endAudioRenderingThread();
        void destroy();

        int readAudioOutput(int samples, int16_t *buffer);

        void writeInput(const double *data);
        void endInputBlock();

        void audioRenderingThread();
        void renderAudio();

        // Audio queued in the synthesizer (input waiting to be rendered and
        // rendered output waiting to be read) [s]
        double getLatency() const;

        // Most rendered audio kept ready for readAudioOutput() [samples]
        static constexpr int MaxRenderedSamples = 11025;

        int inputDelta(int s1, int s0) const;
        double inputDistance(double s1, double s0) const;

        void setInputSampleRate(double sampleRate);
        double getInputSampleRate() const { return m_inputSampleRate; }


        double getLevelerGain();
        AudioParameters getAudioParameters();
        void setAudioParameters(const AudioParameters &params);

    //protected:
        ButterworthLowPassFilter<float> m_antialiasing;
        LevelingFilter m_levelingFilter;
        InputChannel *m_inputChannels;
        AudioParameters m_audioParameters;
        int m_inputChannelCount;
        int m_auxChannelCount;
        int m_totalChannelCount;
        int m_inputBufferSize;
        int m_inputSamplesRead;
        int m_latency;
        double m_inputWriteOffset;
        double m_lastInputSampleOffset;

        RingBuffer<int16_t> m_audioBuffer;
        int m_audioBufferSize;

        float m_inputSampleRate;
        float m_audioSampleRate;

        std::thread *m_thread;
        std::atomic<bool> m_run;

        // m_inputLock guards the input channels, m_lock0 the rendered audio
        // and the parameters. Lock order: m_lock0, then m_inputLock.
        mutable std::mutex m_inputLock;
        mutable std::mutex m_lock0;
        std::condition_variable m_cv0;

        ProcessingFilters *m_filters;

    protected:
        // Fast, thread-local white noise in the range [-1, 1]
        inline float noise() {
            m_noiseState ^= m_noiseState << 13;
            m_noiseState ^= m_noiseState >> 17;
            m_noiseState ^= m_noiseState << 5;
            return static_cast<float>(m_noiseState) * (2.0f / 4294967295.0f) - 1.0f;
        }

        void renderBlock(int n, const AudioParameters &parameters);
        void renderEngineLayers(int n, const AudioParameters &parameters);
        float overrunPop(float overrun, float amount);
        void updateBandwidth();

        struct OnePole {
            float a = 1, z = 0;
            void set(float fc, float fs);
            float process(float x) { z += a * (x - z); return z; }
        };

        uint32_t m_noiseState;

        // Forced induction sounds
        void renderForcedInduction(int n, const AudioParameters &parameters);
        struct Bandpass {
            float b0 = 0, b2 = 0, a1 = 0, a2 = 0;
            float x1 = 0, x2 = 0, y1 = 0, y2 = 0;
            void set(float f, float q, float fs);
            float process(float x);
        };

        double m_whinePhase;
        double m_flutterPhase;
        unsigned int m_fiCounter;
        float m_mixEnvelope;
        Bandpass m_whistleBand, m_whooshBand, m_flutterBand;
        Bandpass m_blowoffBand1, m_blowoffBand2, m_wastegateBand;
        Bandpass m_whistleBand2, m_inducerBand;
        float m_whistleRms = 0;
        Bandpass m_hissBand;
        OnePole m_hissLow, m_turbineLow1, m_turbineLow2;
        float m_blowoffSlow = -1, m_blowoffFollow = 0;
        float m_spoolMemory = 0, m_spoolMemoryRelease = 1;
        float m_whineSmooth = 0, m_whineSmoothing = 0;
        float m_blowoffEnvelope = 0, m_blowoffAttack = 1, m_blowoffDecay = 0;
        OnePole m_wastegateSmooth, m_wastegateChatter, m_screamerLow;
        float m_screamerRms = 0;
        std::vector<float> m_rawExhaust;
        const float *m_auxPointers[AuxChannels] = {};

        // Engine layers and listener perspective
        Bandpass m_intakeBox, m_intakeSnorkel, m_intakeRoar, m_valveBand, m_cabinBoom, m_raspBand;
        OnePole m_intakeDc, m_cabinLow1, m_cabinLow2, m_layerLow1, m_layerLow2, m_bayLow;
        float m_intakePeak = 1e-6f;
        double m_valvePhase = 0;
        float m_valveEnvelope = 0, m_valveAmplitude = 0;
        int m_perspective = -1;
        OnePole m_whistleDrift, m_whistlePulse;

        // Overrun pops: a decaying burst injected ahead of the exhaust's
        // impulse response, so the exhaust colours it
        float m_popEnvelope = 0, m_popAmplitude = 0, m_popDecay = 0;
        std::vector<float> m_inputPeak;

        // Output: a slow, look-ahead loudness stage and a soft clipper
        std::vector<float> m_lookahead;
        int m_lookaheadIndex = 0;
        float m_outputPeak = 1.0f;
        float m_outputReference = 1.0f;
        float m_outputGain = 1.0f;
        float m_peakRelease = 0, m_referenceAttack = 0, m_referenceRelease = 0;
        float m_gainAttack = 0, m_gainSmoothing = 0;

        // Scratch buffers used while rendering a block of audio
        std::vector<float> m_channelInput;
        std::vector<float> m_channelOutput;
        std::vector<float> m_mixBuffer;
        std::vector<float> m_layerBuffer;      // intake, valvetrain and forced induction
        std::vector<float> m_envelope;         // loudness of the exhaust, per sample
        std::vector<float> m_popBuffer;
        std::vector<int16_t> m_renderedAudio;
};

#endif /* ATG_ENGINE_SIM_ENGINE_SYNTHESIZER_H */
