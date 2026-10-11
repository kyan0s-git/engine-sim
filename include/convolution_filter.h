#ifndef ATG_ENGINE_SIM_CONVOLUTION_FILTER_H
#define ATG_ENGINE_SIM_CONVOLUTION_FILTER_H

#include "filter.h"

#include <complex>
#include <vector>

// Convolves a signal with an impulse response (used to model the acoustic
// character of the exhaust system).
//
// Two code paths are provided:
//  - f(): direct per-sample convolution with no latency, O(N) per sample
//  - process(): uniformly partitioned overlap-save FFT convolution which is
//    one to two orders of magnitude faster for long impulse responses at the
//    cost of a fixed latency of getBlockLatency() samples
class ConvolutionFilter : public Filter {
    public:
        static constexpr int DefaultPartitionSize = 128;

    public:
        ConvolutionFilter();
        virtual ~ConvolutionFilter();

        void initialize(int samples, int partitionSize = DefaultPartitionSize);
        virtual float f(float sample) override;
        virtual void destroy() override;

        // Must be called after the impulse response has been modified;
        // process() calls it automatically the first time it is used
        void prepare();

        // Block processing. Produces `n` output samples for `n` input samples
        // where `wet` controls the mix between the convolved signal and the
        // (equally delayed) dry signal.
        void process(const float *input, float *output, int n, float wet = 1.0f);

        int getBlockLatency() const { return m_partitionSize; }

        int getSampleCount() const { return m_sampleCount; }
        float *getImpulseResponse() { return m_impulseResponse.data(); }

    protected:
        void processPartition();

    protected:
        // Direct convolution state
        std::vector<float> m_shiftRegister;
        int m_shiftOffset;

        std::vector<float> m_impulseResponse;
        int m_sampleCount;

        // Partitioned FFT convolution state
        int m_partitionSize;
        int m_fftSize;
        int m_partitionCount;
        bool m_prepared;

        std::vector<std::complex<float>> m_twiddles;
        std::vector<int> m_bitReverse;

        // Spectra of the impulse response partitions
        std::vector<std::complex<float>> m_partitions;

        // Frequency domain delay line of past input blocks (ring buffer)
        std::vector<std::complex<float>> m_inputSpectra;
        int m_currentSpectrum;

        std::vector<float> m_inputWindow;
        std::vector<std::complex<float>> m_workBuffer;
        std::vector<std::complex<float>> m_accumulator;

        std::vector<float> m_pendingInput;
        std::vector<float> m_pendingDry;
        std::vector<float> m_outputWet;
        std::vector<float> m_outputDry;
        int m_pendingCount;
        int m_outputRead;
};

#endif /* ATG_ENGINE_SIM_CONVOLUTION_FILTER_H */
