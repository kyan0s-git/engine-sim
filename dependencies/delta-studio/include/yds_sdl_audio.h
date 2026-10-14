#ifndef YDS_SDL_AUDIO_H
#define YDS_SDL_AUDIO_H

#include "yds_audio_system.h"
#include "yds_audio_device.h"
#include "yds_audio_source.h"
#include "yds_audio_buffer.h"

#include <atomic>
#include <cstdint>
#include <vector>

// Cross-platform audio backend built on SDL2.
//
// Sources behave like DirectSound secondary buffers: each one owns a ring of
// PCM samples that the application writes into, and the SDL audio callback
// mixes every playing source into the output stream while advancing that
// source's play cursor.

class ysSdlAudioDevice;

class ysSdlAudioBuffer : public ysAudioBuffer {
    friend ysSdlAudioDevice;
    friend class ysSdlAudioSource;

public:
    ysSdlAudioBuffer();
    virtual ~ysSdlAudioBuffer();

    virtual ysError Initialize(SampleOffset samples, const ysAudioParameters &parameters) override;
    virtual ysError EditBuffer(void *data) override;
    virtual ysError EditBufferSegment(void *data, SampleOffset samples, SampleOffset offset) override;
    virtual ysError Destroy() override;

    const void *GetBuffer() const { return m_data.data(); }

protected:
    std::vector<uint8_t> m_data;
};

class ysSdlAudioSource : public ysAudioSource {
    friend ysSdlAudioDevice;

public:
    ysSdlAudioSource();
    virtual ~ysSdlAudioSource();

    ysError Initialize(const ysAudioParameters &parameters, SampleOffset samples, ysSdlAudioDevice *device);

    virtual ysError LockEntireBuffer(void **buffer, SampleOffset *samples) override;
    virtual ysError UnlockBuffer(void *buffer, SampleOffset samples) override;
    virtual ysError LockBufferSegment(
        SampleOffset offset, SampleOffset samples,
        void **segment1, SampleOffset *size1,
        void **segment2, SampleOffset *size2) override;
    virtual ysError UnlockBufferSegments(
        void *segment1, SampleOffset segment1Size,
        void *segment2, SampleOffset segment2Size) override;

    virtual ysError SetDataBuffer(ysAudioBuffer *buffer) override;

    virtual ysError SetMode(Mode mode) override;

    virtual SampleOffset GetCurrentPosition() override;
    virtual SampleOffset GetCurrentWritePosition() override;
    virtual void Seek(SampleOffset offset) override;

    virtual ysError SetVolume(float volume) override;
    virtual ysError SetPan(float pan) override;

    virtual ysError Destroy() override;

    bool IsPlaying() const { return m_playing.load(); }

    // Streaming mode: samples are cleared once they have been played, so a
    // writer that falls behind produces silence instead of replaying stale
    // data from the previous pass through the looping buffer
    void SetClearAfterPlayback(bool clear) { m_clearAfterPlayback = clear; }

    // Fills the whole buffer with silence
    void Clear();

protected:
    void ClearRange(SampleOffset from, SampleOffset to);

    // Called from the audio thread. Adds this source's contribution to the
    // stereo float mix buffer.
    void Mix(float *output, int frames, int outputSampleRate);

    float ReadSample(SampleOffset index, int channel) const;

    std::vector<uint8_t> m_data;
    ysSdlAudioDevice *m_device;

    std::atomic<double> m_playCursor;
    std::atomic<bool> m_playing;
    std::atomic<bool> m_looping;

    std::atomic<float> m_gainLeft;
    std::atomic<float> m_gainRight;

    std::atomic<bool> m_clearAfterPlayback;
};

class ysSdlAudioDevice : public ysAudioDevice {
    friend class ysSdlAudioSystem;

public:
    ysSdlAudioDevice();
    virtual ~ysSdlAudioDevice();

    virtual ysAudioBuffer *CreateBuffer(const ysAudioParameters *parameters, SampleOffset size) override;
    virtual ysAudioSource *CreateSource(const ysAudioParameters *parameters, SampleOffset size) override;
    virtual ysAudioSource *CreateSource(ysAudioBuffer *sourceBuffer) override;
    virtual void UpdateAudioSources() override;

    virtual ysError DestroyAudioSource(ysAudioSource *&source) override;

    bool Open();
    void Close();

    void Lock();
    void Unlock();

    // Number of frames SDL requests per callback; used to keep the write
    // cursor ahead of data that may already be queued for output
    int GetCallbackFrames() const { return m_callbackFrames; }
    int GetOutputSampleRate() const { return m_outputSampleRate; }

protected:
    static void AudioCallback(void *userData, uint8_t *stream, int length);
    void FillStream(float *stream, int frames);

    uint32_t m_deviceId;
    int m_callbackFrames;
    int m_outputSampleRate;
    int m_outputChannels;

    std::vector<float> m_mixBuffer;
};

class ysSdlAudioSystem : public ysAudioSystem {
public:
    ysSdlAudioSystem();
    virtual ~ysSdlAudioSystem();

    virtual void EnumerateDevices() override;
    virtual void ConnectDevice(ysAudioDevice *device, ysWindow *windowAssociation) override;
    virtual void ConnectDeviceConsole(ysAudioDevice *device) override;
    virtual void DisconnectDevice(ysAudioDevice *device) override;
};

#endif /* YDS_SDL_AUDIO_H */
