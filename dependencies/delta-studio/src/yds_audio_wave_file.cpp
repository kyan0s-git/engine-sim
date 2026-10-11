#include "../include/yds_audio_wave_file.h"

#include <cstdint>
#include <cstring>

namespace {

    bool readU32(FILE *file, uint32_t *value) {
        uint8_t b[4];
        if (std::fread(b, 1, 4, file) != 4) return false;
        *value = uint32_t(b[0]) | (uint32_t(b[1]) << 8) | (uint32_t(b[2]) << 16) | (uint32_t(b[3]) << 24);
        return true;
    }

    bool readU16(FILE *file, uint16_t *value) {
        uint8_t b[2];
        if (std::fread(b, 1, 2, file) != 2) return false;
        *value = uint16_t(b[0] | (b[1] << 8));
        return true;
    }

} /* namespace */

ysAudioWaveFile::ysAudioWaveFile() : ysAudioFile(Platform::Unknown, AudioFormat::Wave) {
    m_file = nullptr;
    m_dataSegmentOffset = 0;
}

ysAudioWaveFile::~ysAudioWaveFile() {
    DestroyInternalBuffer();
    if (m_file != nullptr) std::fclose(m_file);
}

ysAudioFile::Error ysAudioWaveFile::OpenFile(const char *fname) {
    const Error error = ysAudioFile::OpenFile(fname);
    if (error != Error::None) return error;

    FILE *file = std::fopen(fname, "rb");
    if (file == nullptr) return Error::CouldNotOpenFile;

    char id[4];
    uint32_t size = 0;

    // RIFF header
    if (std::fread(id, 1, 4, file) != 4 || std::memcmp(id, "RIFF", 4) != 0
        || !readU32(file, &size)
        || std::fread(id, 1, 4, file) != 4 || std::memcmp(id, "WAVE", 4) != 0)
    {
        std::fclose(file);
        return Error::InvalidFileFormat;
    }

    bool foundFormat = false;
    uint16_t formatTag = 0, channels = 0, blockAlign = 0, bitsPerSample = 0;
    uint32_t sampleRate = 0, byteRate = 0;

    // Walk the chunk list until both the format and data chunks are found
    while (std::fread(id, 1, 4, file) == 4 && readU32(file, &size)) {
        const long chunkStart = std::ftell(file);

        if (std::memcmp(id, "fmt ", 4) == 0) {
            if (!readU16(file, &formatTag) || !readU16(file, &channels)
                || !readU32(file, &sampleRate) || !readU32(file, &byteRate)
                || !readU16(file, &blockAlign) || !readU16(file, &bitsPerSample))
            {
                break;
            }

            foundFormat = true;
        }
        else if (std::memcmp(id, "data", 4) == 0) {
            // 1 = PCM, 0xFFFE = WAVE_FORMAT_EXTENSIBLE (PCM sub-format)
            if (!foundFormat || channels == 0 || blockAlign == 0
                || (formatTag != 1 && formatTag != 0xFFFE) || size == 0)
            {
                break;
            }

            m_file = file;
            m_dataSegmentOffset = chunkStart;

            m_audioParameters.m_channelCount = channels;
            m_audioParameters.m_sampleRate = static_cast<int>(sampleRate);
            m_audioParameters.m_bitsPerSample = (blockAlign / channels) * 8;

            m_sampleCount = size / blockAlign;
            m_fileOpen = true;

            return Error::None;
        }

        // Chunks are word aligned
        if (std::fseek(file, chunkStart + static_cast<long>(size + (size & 1)), SEEK_SET) != 0) break;
    }

    std::fclose(file);
    return Error::InvalidFileFormat;
}

ysAudioFile::Error ysAudioWaveFile::CloseFile() {
    const Error error = ysAudioFile::CloseFile();
    if (error != Error::None) return error;

    if (m_file != nullptr) {
        std::fclose(m_file);
        m_file = nullptr;
    }

    m_fileOpen = false;

    return Error::None;
}

ysAudioFile::Error ysAudioWaveFile::GenericRead(SampleOffset offset, SampleOffset size, void *buffer) {
    const Error error = ysAudioFile::GenericRead(offset, size, buffer);
    if (error != Error::None) return error;

    const long fileOffset = static_cast<long>(m_audioParameters.GetSizeFromSamples(offset)) + m_dataSegmentOffset;
    const size_t bytes = m_audioParameters.GetSizeFromSamples(size);

    if (std::fseek(m_file, fileOffset, SEEK_SET) != 0) return Error::FileReadError;
    if (std::fread(buffer, 1, bytes, m_file) != bytes) return Error::FileReadError;

    return Error::None;
}
