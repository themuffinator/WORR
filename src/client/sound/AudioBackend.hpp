#pragma once

#include <cstddef>

struct sfx_t;
struct sfxcache_t;
struct channel_t;

class AudioBackend {
public:
    virtual ~AudioBackend() = default;

    virtual bool Init() = 0;
    virtual void Shutdown() = 0;
    virtual void Update() = 0;
    virtual void Activate() = 0;
    virtual void SoundInfo() = 0;
    virtual sfxcache_t *UploadSfx(sfx_t *sfx) = 0;
    virtual void DeleteSfx(sfx_t *sfx);
    virtual void PageInSfx(sfx_t *sfx);
    virtual bool RawSamples(int samples, int rate, int width, int channels, const void *data, float volume) = 0;
    virtual int NeedRawSamples() const;
    virtual int HaveRawSamples() const;
    virtual void DropRawSamples();
    virtual void PauseRawSamples(bool paused);
    virtual int GetBeginOffset(float timeofs) = 0;
    virtual void PlayChannel(channel_t *ch) = 0;
    virtual void StopChannel(channel_t *ch);
    virtual void StopAllSounds();
    virtual int GetSampleRate() const = 0;
    virtual void EndRegistration();
};

inline void AudioBackend::DeleteSfx(sfx_t *)
{
}

inline void AudioBackend::PageInSfx(sfx_t *)
{
}

inline int AudioBackend::NeedRawSamples() const
{
    return 0;
}

inline int AudioBackend::HaveRawSamples() const
{
    return 0;
}

inline void AudioBackend::DropRawSamples()
{
}

inline void AudioBackend::PauseRawSamples(bool)
{
}

inline void AudioBackend::StopChannel(channel_t *)
{
}

inline void AudioBackend::StopAllSounds()
{
}

inline void AudioBackend::EndRegistration()
{
}
