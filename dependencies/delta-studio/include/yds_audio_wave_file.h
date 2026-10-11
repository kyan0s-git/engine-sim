#ifndef YDS_AUDIO_WAVE_FILE_H
#define YDS_AUDIO_WAVE_FILE_H

#include "yds_audio_file.h"

#include <cstdio>

// Portable RIFF/WAVE reader (PCM only). Replaces the Win32 multimedia I/O
// based reader that was previously used on Windows.
class ysAudioWaveFile : public ysAudioFile {
public:
    ysAudioWaveFile();
    virtual ~ysAudioWaveFile();

    virtual Error OpenFile(const char *fname) override;
    virtual Error CloseFile() override;

protected:
    virtual Error GenericRead(SampleOffset offset, SampleOffset size, void *buffer) override;

    FILE *m_file;
    long m_dataSegmentOffset;
};

// Kept for source compatibility with code written against the original
// Windows-only implementation
typedef ysAudioWaveFile ysWindowsAudioWaveFile;

#endif /* YDS_AUDIO_WAVE_FILE_H */
