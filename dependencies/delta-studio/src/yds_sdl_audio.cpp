#include "../include/yds_sdl_audio.h"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstring>

// ---------------------------------------------------------------------------
// ysSdlAudioBuffer
// ---------------------------------------------------------------------------

ysSdlAudioBuffer::ysSdlAudioBuffer() : ysAudioBuffer(API::Sdl) {
    /* void */
}

ysSdlAudioBuffer::~ysSdlAudioBuffer() {
    /* void */
}

ysError ysSdlAudioBuffer::Initialize(SampleOffset samples, const ysAudioParameters &parameters) {
    YDS_ERROR_DECLARE("Initialize");

    YDS_NESTED_ERROR_CALL(ysAudioBuffer::Initialize(samples, parameters));
    m_data.assign(parameters.GetSizeFromSamples(samples), 0);

    return YDS_ERROR_RETURN(ysError::None);
}

ysError ysSdlAudioBuffer::EditBuffer(void *data) {
    YDS_ERROR_DECLARE("EditBuffer");

    if (data == nullptr) return YDS_ERROR_RETURN(ysError::InvalidParameter);
    std::memcpy(m_data.data(), data, m_data.size());

    return YDS_ERROR_RETURN(ysError::None);
}

ysError ysSdlAudioBuffer::EditBufferSegment(void *data, SampleOffset samples, SampleOffset offset) {
    YDS_ERROR_DECLARE("EditBufferSegment");

    if (data == nullptr) return YDS_ERROR_RETURN(ysError::InvalidParameter);
    if (offset + samples > m_samples) return YDS_ERROR_RETURN(ysError::OutOfBounds);

    std::memcpy(
        m_data.data() + m_audioParameters.GetSizeFromSamples(offset),
        data,
        m_audioParameters.GetSizeFromSamples(samples));

    return YDS_ERROR_RETURN(ysError::None);
}

ysError ysSdlAudioBuffer::Destroy() {
    YDS_ERROR_DECLARE("Destroy");

    m_data.clear();
    m_data.shrink_to_fit();

    return YDS_ERROR_RETURN(ysError::None);
}

// ---------------------------------------------------------------------------
// ysSdlAudioSource
// ---------------------------------------------------------------------------

ysSdlAudioSource::ysSdlAudioSource() : ysAudioSource(API::Sdl) {
    m_device = nullptr;
    m_dataBuffer = nullptr;
    m_playCursor = 0.0;
    m_playing = false;
    m_looping = false;
    m_gainLeft = 1.0f;
    m_gainRight = 1.0f;
    m_clearAfterPlayback = false;
}

ysSdlAudioSource::~ysSdlAudioSource() {
    /* void */
}

ysError ysSdlAudioSource::Initialize(const ysAudioParameters &parameters, SampleOffset samples, ysSdlAudioDevice *device) {
    YDS_ERROR_DECLARE("Initialize");

    m_audioParameters = parameters;
    m_bufferSize = samples;
    m_device = device;
    m_data.assign(parameters.GetSizeFromSamples(samples), 0);

    return YDS_ERROR_RETURN(ysError::None);
}

ysError ysSdlAudioSource::LockEntireBuffer(void **buffer, SampleOffset *samples) {
    YDS_ERROR_DECLARE("LockEntireBuffer");

    YDS_NESTED_ERROR_CALL(ysAudioSource::LockEntireBuffer(buffer, samples));
    *buffer = m_data.data();

    return YDS_ERROR_RETURN(ysError::None);
}

ysError ysSdlAudioSource::UnlockBuffer(void *buffer, SampleOffset samples) {
    YDS_ERROR_DECLARE("UnlockBuffer");

    YDS_NESTED_ERROR_CALL(ysAudioSource::UnlockBuffer(buffer, samples));

    return YDS_ERROR_RETURN(ysError::None);
}

ysError ysSdlAudioSource::LockBufferSegment(
    SampleOffset offset, SampleOffset samples,
    void **segment1, SampleOffset *size1,
    void **segment2, SampleOffset *size2)
{
    YDS_ERROR_DECLARE("LockBufferSegment");

    SampleOffset s1 = 0, s2 = 0;
    YDS_NESTED_ERROR_CALL(ysAudioSource::LockBufferSegment(offset, samples, segment1, &s1, segment2, &s2));

    *segment1 = m_data.data() + m_audioParameters.GetSizeFromSamples(offset);
    *segment2 = (s2 > 0) ? m_data.data() : nullptr;

    if (size1 != nullptr) *size1 = s1;
    if (size2 != nullptr) *size2 = s2;

    return YDS_ERROR_RETURN(ysError::None);
}

ysError ysSdlAudioSource::UnlockBufferSegments(
    void *segment1, SampleOffset segment1Size,
    void *segment2, SampleOffset segment2Size)
{
    YDS_ERROR_DECLARE("UnlockBufferSegments");

    YDS_NESTED_ERROR_CALL(ysAudioSource::UnlockBufferSegments(segment1, segment1Size, segment2, segment2Size));

    return YDS_ERROR_RETURN(ysError::None);
}

ysError ysSdlAudioSource::SetDataBuffer(ysAudioBuffer *buffer) {
    YDS_ERROR_DECLARE("SetDataBuffer");

    YDS_NESTED_ERROR_CALL(ysAudioSource::SetDataBuffer(buffer));

    if (buffer->GetAPI() != API::Sdl) return YDS_ERROR_RETURN(ysError::IncompatiblePlatforms);

    ysSdlAudioBuffer *sdlBuffer = static_cast<ysSdlAudioBuffer *>(buffer);
    const size_t bytes = std::min(m_data.size(), sdlBuffer->m_data.size());

    if (m_device != nullptr) m_device->Lock();
    std::memcpy(m_data.data(), sdlBuffer->m_data.data(), bytes);
    if (m_device != nullptr) m_device->Unlock();

    return YDS_ERROR_RETURN(ysError::None);
}

ysError ysSdlAudioSource::SetMode(Mode mode) {
    YDS_ERROR_DECLARE("SetMode");

    YDS_NESTED_ERROR_CALL(ysAudioSource::SetMode(mode));

    switch (mode) {
        case Mode::Play:
        case Mode::PlayOnce:
            m_looping = false;
            m_playing = true;
            break;
        case Mode::Loop:
            m_looping = true;
            m_playing = true;
            break;
        case Mode::Stop:
            m_playing = false;
            break;
        default:
            break;
    }

    return YDS_ERROR_RETURN(ysError::None);
}

SampleOffset ysSdlAudioSource::GetCurrentPosition() {
    if (m_bufferSize == 0) return 0;
    return static_cast<SampleOffset>(m_playCursor.load()) % m_bufferSize;
}

SampleOffset ysSdlAudioSource::GetCurrentWritePosition() {
    if (m_bufferSize == 0) return 0;

    // Leave room for one callback worth of samples which the audio thread may
    // be reading at any moment (this mimics DirectSound's write cursor)
    SampleOffset lead = 0;
    if (m_device != nullptr && m_device->GetOutputSampleRate() > 0) {
        lead = static_cast<SampleOffset>(
            std::ceil(m_device->GetCallbackFrames()
                * static_cast<double>(m_audioParameters.m_sampleRate) / m_device->GetOutputSampleRate()));
    }

    return (static_cast<SampleOffset>(m_playCursor.load()) + lead) % m_bufferSize;
}

void ysSdlAudioSource::Seek(SampleOffset offset) {
    if (m_bufferSize == 0) return;
    m_playCursor = static_cast<double>(offset % m_bufferSize);
}

ysError ysSdlAudioSource::SetVolume(float volume) {
    YDS_ERROR_DECLARE("SetVolume");

    YDS_NESTED_ERROR_CALL(ysAudioSource::SetVolume(volume));

    // Match DirectSound panning: the side opposite to the pan direction is
    // attenuated while the near side stays at full volume
    m_gainLeft = m_volume * std::min(1.0f, 1.0f - m_pan);
    m_gainRight = m_volume * std::min(1.0f, 1.0f + m_pan);

    return YDS_ERROR_RETURN(ysError::None);
}

ysError ysSdlAudioSource::SetPan(float pan) {
    YDS_ERROR_DECLARE("SetPan");

    YDS_NESTED_ERROR_CALL(ysAudioSource::SetPan(pan));
    YDS_NESTED_ERROR_CALL(SetVolume(m_volume));

    return YDS_ERROR_RETURN(ysError::None);
}

ysError ysSdlAudioSource::Destroy() {
    YDS_ERROR_DECLARE("Destroy");

    YDS_NESTED_ERROR_CALL(ysAudioSource::Destroy());

    m_playing = false;
    m_data.clear();
    m_data.shrink_to_fit();
    m_bufferSize = 0;

    return YDS_ERROR_RETURN(ysError::None);
}

void ysSdlAudioSource::Clear() {
    if (m_device != nullptr) m_device->Lock();
    ClearRange(0, m_bufferSize);
    if (m_device != nullptr) m_device->Unlock();
}

void ysSdlAudioSource::ClearRange(SampleOffset from, SampleOffset to) {
    if (m_bufferSize == 0 || m_data.empty() || from >= to) return;

    // 8-bit samples are unsigned with silence at 128
    const uint8_t silence = (m_audioParameters.m_bitsPerSample == 8) ? 128 : 0;
    const size_t begin = m_audioParameters.GetSizeFromSamples(from);
    const size_t end = std::min(m_data.size(), static_cast<size_t>(m_audioParameters.GetSizeFromSamples(to)));
    if (begin < end) std::memset(m_data.data() + begin, silence, end - begin);
}

float ysSdlAudioSource::ReadSample(SampleOffset index, int channel) const {
    const int channels = m_audioParameters.m_channelCount;
    const int bytesPerSample = m_audioParameters.m_bitsPerSample / 8;
    const size_t offset = (static_cast<size_t>(index) * channels + std::min(channel, channels - 1)) * bytesPerSample;

    switch (bytesPerSample) {
        case 1:
            return (static_cast<int>(m_data[offset]) - 128) / 128.0f;
        case 2:
        {
            int16_t v;
            std::memcpy(&v, &m_data[offset], sizeof(int16_t));
            return v / 32768.0f;
        }
        case 4:
        {
            int32_t v;
            std::memcpy(&v, &m_data[offset], sizeof(int32_t));
            return v / 2147483648.0f;
        }
        default:
            return 0.0f;
    }
}

void ysSdlAudioSource::Mix(float *output, int frames, int outputSampleRate) {
    if (!m_playing || m_bufferSize == 0 || m_data.empty()) return;

    const double step = static_cast<double>(m_audioParameters.m_sampleRate) / outputSampleRate;
    const bool sameRate = std::abs(step - 1.0) < 1e-9;
    const float gainLeft = m_gainLeft;
    const float gainRight = m_gainRight;
    const bool looping = m_looping;
    const bool stereo = m_audioParameters.m_channelCount >= 2;

    double cursor = m_playCursor;
    const bool clear = m_clearAfterPlayback && looping;
    SampleOffset clearFrom = static_cast<SampleOffset>(cursor);
    SampleOffset played = 0;      // whole samples passed during this call

    for (int i = 0; i < frames; ++i) {
        if (cursor >= m_bufferSize) {
            if (looping) {
                cursor = std::fmod(cursor, static_cast<double>(m_bufferSize));
            }
            else {
                m_playing = false;
                break;
            }
        }

        const SampleOffset i0 = static_cast<SampleOffset>(cursor);
        float left, right;
        if (sameRate) {
            left = ReadSample(i0, 0);
            right = stereo ? ReadSample(i0, 1) : left;
        }
        else {
            // Linear interpolation between neighbouring samples
            const SampleOffset i1 = (i0 + 1 < m_bufferSize) ? i0 + 1 : (looping ? 0 : i0);
            const float t = static_cast<float>(cursor - i0);
            left = ReadSample(i0, 0) * (1 - t) + ReadSample(i1, 0) * t;
            right = stereo
                ? ReadSample(i0, 1) * (1 - t) + ReadSample(i1, 1) * t
                : left;
        }

        output[2 * i + 0] += left * gainLeft;
        output[2 * i + 1] += right * gainRight;

        const SampleOffset before = static_cast<SampleOffset>(cursor);
        cursor += step;
        played += static_cast<SampleOffset>(cursor) - before;
    }

    if (clear && played > 0) {
        // Clear the samples that were played, wrapping around the end
        played = std::min(played, m_bufferSize);
        const SampleOffset from = clearFrom % m_bufferSize;
        const SampleOffset firstPart = std::min(played, m_bufferSize - from);
        ClearRange(from, from + firstPart);
        ClearRange(0, played - firstPart);
    }

    m_playCursor = looping ? std::fmod(cursor, static_cast<double>(m_bufferSize)) : cursor;
}

// ---------------------------------------------------------------------------
// ysSdlAudioDevice
// ---------------------------------------------------------------------------

ysSdlAudioDevice::ysSdlAudioDevice() : ysAudioDevice(API::Sdl) {
    m_deviceId = 0;
    m_callbackFrames = 512;
    m_outputSampleRate = 44100;
    m_outputChannels = 2;
}

ysSdlAudioDevice::~ysSdlAudioDevice() {
    Close();
}

bool ysSdlAudioDevice::Open() {
    if (m_deviceId != 0) return true;

    SDL_AudioSpec desired;
    SDL_zero(desired);
    desired.freq = m_outputSampleRate;
    desired.format = AUDIO_F32SYS;
    desired.channels = static_cast<Uint8>(m_outputChannels);
    desired.samples = static_cast<Uint16>(m_callbackFrames);
    desired.callback = &ysSdlAudioDevice::AudioCallback;
    desired.userdata = this;

    SDL_AudioSpec obtained;
    SDL_zero(obtained);

    // Allow SDL to pick a different buffer size but have it convert the
    // sample format, rate and channel count for us
    m_deviceId = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, SDL_AUDIO_ALLOW_SAMPLES_CHANGE);
    if (m_deviceId == 0) return false;

    m_callbackFrames = obtained.samples;
    m_mixBuffer.assign(static_cast<size_t>(m_callbackFrames) * m_outputChannels, 0.0f);

    SDL_PauseAudioDevice(m_deviceId, 0);

    return true;
}

void ysSdlAudioDevice::Close() {
    if (m_deviceId != 0) {
        SDL_CloseAudioDevice(m_deviceId);
        m_deviceId = 0;
    }
}

void ysSdlAudioDevice::Lock() {
    if (m_deviceId != 0) SDL_LockAudioDevice(m_deviceId);
}

void ysSdlAudioDevice::Unlock() {
    if (m_deviceId != 0) SDL_UnlockAudioDevice(m_deviceId);
}

void ysSdlAudioDevice::AudioCallback(void *userData, uint8_t *stream, int length) {
    ysSdlAudioDevice *device = static_cast<ysSdlAudioDevice *>(userData);
    const int frames = length / static_cast<int>(sizeof(float) * device->m_outputChannels);
    device->FillStream(reinterpret_cast<float *>(stream), frames);
}

void ysSdlAudioDevice::FillStream(float *stream, int frames) {
    std::fill(stream, stream + static_cast<size_t>(frames) * m_outputChannels, 0.0f);

    const int sourceCount = m_audioSources.GetNumObjects();
    for (int i = 0; i < sourceCount; ++i) {
        ysSdlAudioSource *source = static_cast<ysSdlAudioSource *>(m_audioSources.Get(i));
        source->Mix(stream, frames, m_outputSampleRate);
    }

    for (int i = 0; i < frames * m_outputChannels; ++i) {
        stream[i] = std::max(-1.0f, std::min(1.0f, stream[i]));
    }
}

ysAudioBuffer *ysSdlAudioDevice::CreateBuffer(const ysAudioParameters *parameters, SampleOffset size) {
    ysSdlAudioBuffer *newBuffer = m_audioBuffers.NewGeneric<ysSdlAudioBuffer>();
    newBuffer->Initialize(size, *parameters);

    return newBuffer;
}

ysAudioSource *ysSdlAudioDevice::CreateSource(const ysAudioParameters *parameters, SampleOffset size) {
    Lock();
    ysSdlAudioSource *newSource = m_audioSources.NewGeneric<ysSdlAudioSource>();
    newSource->Initialize(*parameters, size, this);
    Unlock();

    return newSource;
}

ysAudioSource *ysSdlAudioDevice::CreateSource(ysAudioBuffer *sourceBuffer) {
    ysAudioSource *newSource = CreateSource(sourceBuffer->GetAudioParameters(), sourceBuffer->GetSampleCount());
    newSource->SetDataBuffer(sourceBuffer);

    return newSource;
}

void ysSdlAudioDevice::UpdateAudioSources() {
    Lock();
    const int sourceCount = m_audioSources.GetNumObjects();
    for (int i = sourceCount - 1; i >= 0; --i) {
        ysSdlAudioSource *source = static_cast<ysSdlAudioSource *>(m_audioSources.Get(i));
        if (source->GetBufferMode() == ysAudioSource::Mode::PlayOnce && !source->IsPlaying()) {
            source->Destroy();
            m_audioSources.Delete(source->GetIndex());
        }
    }
    Unlock();
}

ysError ysSdlAudioDevice::DestroyAudioSource(ysAudioSource *&source) {
    YDS_ERROR_DECLARE("DestroyAudioSource");

    if (source == nullptr) return YDS_ERROR_RETURN(ysError::InvalidParameter);

    Lock();
    source->Destroy();
    m_audioSources.Delete(source->GetIndex());
    Unlock();

    source = nullptr;

    return YDS_ERROR_RETURN(ysError::None);
}

// ---------------------------------------------------------------------------
// ysSdlAudioSystem
// ---------------------------------------------------------------------------

ysSdlAudioSystem::ysSdlAudioSystem() : ysAudioSystem(API::Sdl) {
    // Reference counted; paired with SDL_QuitSubSystem() in the destructor
    SDL_InitSubSystem(SDL_INIT_AUDIO);
}

ysSdlAudioSystem::~ysSdlAudioSystem() {
    for (int i = 0; i < m_devices.GetNumObjects(); ++i) {
        static_cast<ysSdlAudioDevice *>(m_devices.Get(i))->Close();
    }

    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void ysSdlAudioSystem::EnumerateDevices() {
    ysAudioSystem::EnumerateDevices();

    // Index 0 is always the system default output device
    ysSdlAudioDevice *device = m_devices.NewGeneric<ysSdlAudioDevice>();
    device->SetDeviceName("Default");
}

void ysSdlAudioSystem::ConnectDevice(ysAudioDevice *device, ysWindow *windowAssociation) {
    ysAudioSystem::ConnectDevice(device, windowAssociation);
    static_cast<ysSdlAudioDevice *>(device)->Open();
}

void ysSdlAudioSystem::ConnectDeviceConsole(ysAudioDevice *device) {
    ysAudioSystem::ConnectDeviceConsole(device);
    static_cast<ysSdlAudioDevice *>(device)->Open();
}

void ysSdlAudioSystem::DisconnectDevice(ysAudioDevice *device) {
    ysAudioSystem::DisconnectDevice(device);
    static_cast<ysSdlAudioDevice *>(device)->Close();
}
