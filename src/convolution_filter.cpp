#include "../include/convolution_filter.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

    // In-place iterative radix-2 FFT. `inverse` computes the unnormalized
    // inverse transform.
    void fft(
        std::complex<float> *data,
        int n,
        const std::complex<float> *twiddles,
        const int *bitReverse,
        bool inverse)
    {
        for (int i = 0; i < n; ++i) {
            const int j = bitReverse[i];
            if (i < j) std::swap(data[i], data[j]);
        }

        for (int size = 2; size <= n; size *= 2) {
            const int half = size / 2;
            const int step = n / size;
            for (int start = 0; start < n; start += size) {
                for (int k = 0; k < half; ++k) {
                    std::complex<float> w = twiddles[k * step];
                    if (inverse) w = std::conj(w);

                    const std::complex<float> a = data[start + k];
                    const std::complex<float> b = data[start + k + half] * w;
                    data[start + k] = a + b;
                    data[start + k + half] = a - b;
                }
            }
        }
    }

} /* namespace */

ConvolutionFilter::ConvolutionFilter() {
    m_shiftOffset = 0;
    m_sampleCount = 0;

    m_partitionSize = DefaultPartitionSize;
    m_fftSize = 0;
    m_partitionCount = 0;
    m_prepared = false;
    m_currentSpectrum = 0;
    m_pendingCount = 0;
    m_outputRead = 0;
}

ConvolutionFilter::~ConvolutionFilter() {
    /* void */
}

void ConvolutionFilter::initialize(int samples, int partitionSize) {
    m_sampleCount = std::max(1, samples);
    m_shiftOffset = 0;
    m_shiftRegister.assign(m_sampleCount, 0.0f);
    m_impulseResponse.assign(m_sampleCount, 0.0f);

    // Partition size must be a power of two
    m_partitionSize = 1;
    while (m_partitionSize < partitionSize) m_partitionSize *= 2;

    m_prepared = false;
}

void ConvolutionFilter::destroy() {
    m_shiftRegister.clear();
    m_impulseResponse.clear();
    m_partitions.clear();
    m_inputSpectra.clear();
    m_twiddles.clear();
    m_bitReverse.clear();
    m_inputWindow.clear();
    m_workBuffer.clear();
    m_accumulator.clear();
    m_pendingInput.clear();
    m_pendingDry.clear();
    m_outputWet.clear();
    m_outputDry.clear();

    m_shiftRegister.shrink_to_fit();
    m_impulseResponse.shrink_to_fit();
    m_partitions.shrink_to_fit();
    m_inputSpectra.shrink_to_fit();

    m_sampleCount = 0;
    m_partitionCount = 0;
    m_prepared = false;
}

float ConvolutionFilter::f(float sample) {
    if (m_sampleCount == 0 || m_shiftRegister.empty()) return 0.0f;

    m_shiftRegister[m_shiftOffset] = sample;

    float result = 0;
    for (int i = 0; i < m_sampleCount - m_shiftOffset; ++i) {
        result += m_impulseResponse[i] * m_shiftRegister[i + m_shiftOffset];
    }

    for (int i = m_sampleCount - m_shiftOffset; i < m_sampleCount; ++i) {
        result += m_impulseResponse[i] * m_shiftRegister[i - (m_sampleCount - m_shiftOffset)];
    }

    m_shiftOffset = (m_shiftOffset - 1 + m_sampleCount) % m_sampleCount;

    return result;
}

void ConvolutionFilter::prepare() {
    const int B = m_partitionSize;
    m_fftSize = 2 * B;
    m_partitionCount = (m_sampleCount + B - 1) / B;

    // Twiddle factors and bit reversal table
    m_twiddles.resize(m_fftSize / 2);
    for (int k = 0; k < m_fftSize / 2; ++k) {
        const double angle = -2.0 * 3.14159265358979323846 * k / m_fftSize;
        m_twiddles[k] = std::complex<float>((float)std::cos(angle), (float)std::sin(angle));
    }

    int bits = 0;
    while ((1 << bits) < m_fftSize) ++bits;
    m_bitReverse.resize(m_fftSize);
    for (int i = 0; i < m_fftSize; ++i) {
        int r = 0;
        for (int b = 0; b < bits; ++b) {
            if (i & (1 << b)) r |= 1 << (bits - 1 - b);
        }
        m_bitReverse[i] = r;
    }

    // Transform each partition of the impulse response
    m_partitions.assign((size_t)m_partitionCount * m_fftSize, std::complex<float>(0, 0));
    for (int p = 0; p < m_partitionCount; ++p) {
        std::complex<float> *partition = &m_partitions[(size_t)p * m_fftSize];
        for (int i = 0; i < B; ++i) {
            const int index = p * B + i;
            partition[i] = (index < m_sampleCount)
                ? std::complex<float>(m_impulseResponse[index], 0)
                : std::complex<float>(0, 0);
        }

        fft(partition, m_fftSize, m_twiddles.data(), m_bitReverse.data(), false);
    }

    m_inputSpectra.assign((size_t)m_partitionCount * m_fftSize, std::complex<float>(0, 0));
    m_currentSpectrum = 0;

    m_inputWindow.assign(m_fftSize, 0.0f);
    m_workBuffer.assign(m_fftSize, std::complex<float>(0, 0));
    m_accumulator.assign(m_fftSize, std::complex<float>(0, 0));

    m_pendingInput.assign(B, 0.0f);
    m_pendingDry.assign(B, 0.0f);
    m_outputWet.assign(B, 0.0f);
    m_outputDry.assign(B, 0.0f);
    m_pendingCount = 0;
    m_outputRead = 0;

    m_prepared = true;
}

void ConvolutionFilter::processPartition() {
    const int B = m_partitionSize;

    // Slide the input window: [previous block, current block]
    std::memmove(m_inputWindow.data(), m_inputWindow.data() + B, sizeof(float) * B);
    std::memcpy(m_inputWindow.data() + B, m_pendingInput.data(), sizeof(float) * B);

    // Transform the current input window into the frequency domain delay line
    m_currentSpectrum = (m_currentSpectrum + m_partitionCount - 1) % m_partitionCount;
    std::complex<float> *spectrum = &m_inputSpectra[(size_t)m_currentSpectrum * m_fftSize];
    for (int i = 0; i < m_fftSize; ++i) {
        spectrum[i] = std::complex<float>(m_inputWindow[i], 0);
    }
    fft(spectrum, m_fftSize, m_twiddles.data(), m_bitReverse.data(), false);

    // Multiply-accumulate every partition against the matching past input.
    // The signals are real so the spectra are conjugate symmetric and only
    // the first half (plus the Nyquist bin) needs to be computed.
    std::fill(m_accumulator.begin(), m_accumulator.end(), std::complex<float>(0, 0));
    for (int p = 0; p < m_partitionCount; ++p) {
        const int slot = (m_currentSpectrum + p) % m_partitionCount;
        const std::complex<float> *x = &m_inputSpectra[(size_t)slot * m_fftSize];
        const std::complex<float> *h = &m_partitions[(size_t)p * m_fftSize];

        for (int k = 0; k <= B; ++k) {
            m_accumulator[k] += x[k] * h[k];
        }
    }

    for (int k = B + 1; k < m_fftSize; ++k) {
        m_accumulator[k] = std::conj(m_accumulator[m_fftSize - k]);
    }

    // Back to the time domain; the second half contains the valid samples
    std::copy(m_accumulator.begin(), m_accumulator.end(), m_workBuffer.begin());
    fft(m_workBuffer.data(), m_fftSize, m_twiddles.data(), m_bitReverse.data(), true);

    const float scale = 1.0f / m_fftSize;
    for (int i = 0; i < B; ++i) {
        m_outputWet[i] = m_workBuffer[B + i].real() * scale;
    }

    std::copy(m_pendingDry.begin(), m_pendingDry.end(), m_outputDry.begin());
}

void ConvolutionFilter::process(const float *input, float *output, int n, float wet) {
    if (m_sampleCount == 0 || m_impulseResponse.empty()) {
        for (int i = 0; i < n; ++i) output[i] = (1 - wet) * input[i];
        return;
    }

    if (!m_prepared) {
        prepare();
    }

    const int B = m_partitionSize;
    for (int i = 0; i < n; ++i) {
        m_pendingInput[m_pendingCount] = input[i];
        m_pendingDry[m_pendingCount] = input[i];

        // Output lags the input by exactly one partition
        output[i] = wet * m_outputWet[m_outputRead] + (1 - wet) * m_outputDry[m_outputRead];

        ++m_pendingCount;
        ++m_outputRead;

        if (m_pendingCount == B) {
            processPartition();
            m_pendingCount = 0;
            m_outputRead = 0;
        }
    }
}
