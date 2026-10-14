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
            float levelerTarget = 30000.0f;
            float levelerMaxGain = 1.9f;
            float levelerMinGain = 0.00001f;
        };

        struct Parameters {
            int inputChannelCount = 1;
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

        uint32_t m_noiseState;

        // Scratch buffers used while rendering a block of audio
        std::vector<float> m_channelInput;
        std::vector<float> m_channelOutput;
        std::vector<float> m_mixBuffer;
        std::vector<int16_t> m_renderedAudio;
};

#endif /* ATG_ENGINE_SIM_ENGINE_SYNTHESIZER_H */
