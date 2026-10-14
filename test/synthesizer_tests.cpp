#include <gtest/gtest.h>

#include "../include/synthesizer.h"
#include "../include/convolution_filter.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>
#include <cstdint>
#include <random>
#include <vector>

namespace {

    void setupSynthesizer(Synthesizer &synth, int channels, float noise) {
        Synthesizer::Parameters params;
        params.audioBufferSize = 44100;
        params.audioSampleRate = 44100;
        params.inputBufferSize = 44100;
        params.inputChannelCount = channels;
        params.inputSampleRate = 10000;

        Synthesizer::AudioParameters audioParams;
        audioParams.airNoise = noise;
        audioParams.inputSampleNoise = noise;
        audioParams.dF_F_mix = 0.0f;
        params.initialAudioParameters = audioParams;

        synth.initialize(params);

        // Simple decaying impulse response
        std::vector<int16_t> ir(2000);
        for (int i = 0; i < (int)ir.size(); ++i) {
            ir[i] = static_cast<int16_t>(INT16_MAX * std::exp(-i / 200.0) * ((i % 2) ? -1 : 1));
        }

        for (int i = 0; i < channels; ++i) {
            synth.initializeImpulseResponse(ir.data(), (unsigned int)ir.size(), 1.0f, i);
        }
    }

    // Writes `inputSamples` samples of a sine wave and renders them on the
    // calling thread. Returns the samples that were produced.
    std::vector<int16_t> renderSine(Synthesizer &synth, int channels, int inputSamples, double amplitude) {
        std::vector<int16_t> output;
        std::vector<double> data(channels);
        std::vector<int16_t> block(4096);

        for (int i = 0; i < inputSamples;) {
            for (int j = 0; j < 100 && i < inputSamples; ++j, ++i) {
                for (int c = 0; c < channels; ++c) {
                    data[c] = amplitude * std::sin(i * 0.05);
                }
                synth.writeInput(data.data());
            }

            synth.endInputBlock();
            synth.renderAudio();

            const int read = synth.readAudioOutput((int)block.size(), block.data());
            output.insert(output.end(), block.begin(), block.begin() + read);
        }

        return output;
    }

} /* namespace */

TEST(SynthesizerTests, SynthesizerSanityCheck) {
    Synthesizer synth;
    setupSynthesizer(synth, 1, 0.0f);
    synth.destroy();
}

TEST(SynthesizerTests, SilenceInSilenceOut) {
    Synthesizer synth;
    setupSynthesizer(synth, 2, 0.0f);

    const std::vector<int16_t> output = renderSine(synth, 2, 5000, 0.0);
    ASSERT_FALSE(output.empty());

    for (const int16_t sample : output) {
        EXPECT_EQ(sample, 0);
    }

    synth.destroy();
}

TEST(SynthesizerTests, ProducesBoundedAudio) {
    Synthesizer synth;
    setupSynthesizer(synth, 3, 0.5f);

    const std::vector<int16_t> output = renderSine(synth, 3, 20000, 1000.0);

    // The input is resampled from 10 kHz to 44.1 kHz
    EXPECT_GT(output.size(), 20000u * 4);

    int nonZero = 0;
    for (const int16_t sample : output) {
        if (sample != 0) ++nonZero;
    }

    EXPECT_GT(nonZero, (int)output.size() / 2);

    // The leveler limits the peaks to its target so nothing clips
    int peak = 0;
    for (const int16_t sample : output) peak = std::max(peak, std::abs((int)sample));
    EXPECT_LE(peak, (int)Synthesizer::AudioParameters().levelerTarget + 1);

    synth.destroy();
}

TEST(SynthesizerTests, RenderingThread) {
    Synthesizer synth;
    setupSynthesizer(synth, 1, 0.0f);
    synth.startAudioRenderingThread();

    std::vector<double> data(1);
    std::vector<int16_t> block(4096);
    int total = 0;

    for (int i = 0; i < 20000;) {
        // Like the simulator, only produce input while the queue is short
        for (int wait = 0; wait < 1000 && synth.getLatency() > 0.1; ++wait) {
            total += synth.readAudioOutput((int)block.size(), block.data());
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        for (int j = 0; j < 100; ++j, ++i) {
            data[0] = 1000.0 * std::sin(i * 0.05);
            synth.writeInput(data.data());
        }

        synth.endInputBlock();
        total += synth.readAudioOutput(441, block.data());
    }

    // The rendering thread works independently of the input blocks; all of
    // the input should come out (20000 samples at 10 kHz = 2 s of audio)
    const int expected = static_cast<int>(20000 * 44100.0 / 10000);
    for (int wait = 0; wait < 400 && total < expected - 100; ++wait) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        total += synth.readAudioOutput((int)block.size(), block.data());
    }

    synth.endAudioRenderingThread();

    EXPECT_GT(total, expected - 100);
    EXPECT_NEAR(synth.getLatency(), 0.0, 0.01);

    synth.destroy();
}

TEST(ConvolutionFilterTests, FftMatchesDirectConvolution) {
    std::mt19937 rng(1234);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    for (const int irLength : { 1, 7, 128, 129, 1000, 4099 }) {
        ConvolutionFilter direct, fast;
        direct.initialize(irLength);
        fast.initialize(irLength, 64);

        for (int i = 0; i < irLength; ++i) {
            const float h = dist(rng) * std::exp(-i / 500.0f);
            direct.getImpulseResponse()[i] = h;
            fast.getImpulseResponse()[i] = h;
        }

        fast.prepare();

        const int samples = 5000;
        std::vector<float> input(samples), expected(samples), actual(samples);
        for (int i = 0; i < samples; ++i) {
            input[i] = dist(rng);
            expected[i] = direct.f(input[i]);
        }

        // Process in irregular block sizes
        int offset = 0;
        int blockSize = 1;
        while (offset < samples) {
            const int n = std::min(blockSize, samples - offset);
            fast.process(input.data() + offset, actual.data() + offset, n);
            offset += n;
            blockSize = (blockSize * 7) % 300 + 1;
        }

        const int latency = fast.getBlockLatency();
        for (int i = 0; i + latency < samples; ++i) {
            ASSERT_NEAR(actual[i + latency], expected[i], 1E-3f) << "IR length " << irLength << ", sample " << i;
        }

        for (int i = 0; i < latency; ++i) {
            EXPECT_EQ(actual[i], 0.0f);
        }

        direct.destroy();
        fast.destroy();
    }
}

TEST(ConvolutionFilterTests, DryWetMix) {
    ConvolutionFilter filter;
    filter.initialize(16, 8);
    filter.getImpulseResponse()[0] = 2.0f;

    std::vector<float> input(64), output(64);
    for (int i = 0; i < 64; ++i) input[i] = (float)i;

    filter.process(input.data(), output.data(), 64, 0.25f);

    const int latency = filter.getBlockLatency();
    for (int i = latency; i < 64; ++i) {
        const float x = input[i - latency];
        EXPECT_NEAR(output[i], 0.25f * 2.0f * x + 0.75f * x, 1E-3f);
    }

    filter.destroy();
}
