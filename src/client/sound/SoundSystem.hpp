#pragma once

#include <cstddef>
#include <deque>
#include <memory>
#include <vector>

#include "sound.hpp"

class SoundSystem {
public:
    struct Config {
        std::size_t maxSfx = 0;
        std::size_t maxPlaysounds = 0;
    };

    static constexpr std::size_t kDefaultMaxSfx = static_cast<std::size_t>(MAX_SOUNDS) * 2;
    static constexpr std::size_t kDefaultMaxPlaysounds = 128;

    SoundSystem();
    explicit SoundSystem(const Config &config);

    void Configure(const Config &config);

    unsigned registration_sequence() const;
    void reset_registration_sequence();
    void increment_registration_sequence();

    sfx_t *known_sfx_data();
    const sfx_t *known_sfx_data() const;
    int num_sfx() const;
    void set_num_sfx(int count);
    int max_sfx() const;

    channel_t *channels_data();
    const channel_t *channels_data() const;
    void set_channel_capacity(int capacity);
    void clear_channels();
    int &num_channels();
    int &max_channels();

    playsound_t *AllocatePlaysound();
    void FreePlaysound(playsound_t *ps);
    void QueuePendingPlay(playsound_t *ps);
    void RemovePendingPlay(playsound_t *ps);
    playsound_t *PeekPendingPlay();
    bool HasPendingPlays() const;
    void ResetPlaysounds();

    sfx_t *SfxForHandle(qhandle_t hSfx);
    sfxcache_t *LoadSound(sfx_t *s);
    channel_t *PickChannel(int entnum, int entchannel);
    void IssuePlaysound(playsound_t *ps);
    int BuildSoundList(int *sounds);
    void SpatializeOrigin(const vec3_t origin, float master_vol, float dist_mult,
                          float *left_vol, float *right_vol, bool stereo);

    void StartSound(const vec3_t origin, int entnum, int entchannel, qhandle_t hSfx,
                    float vol, float attenuation, float timeofs);
    void StopAllSounds();
    void Update();

    vec3_t &listener_origin();
    vec3_t &listener_forward();
    vec3_t &listener_right();
    vec3_t &listener_up();
    int &listener_entnum();

private:
    void InitializeStorage();

    Config config_{};
    unsigned registration_sequence_ = 1;
    std::vector<sfx_t> known_sfx_;
    int num_sfx_ = 0;
    std::vector<channel_t> channels_;
    int num_channels_ = 0;
    int max_channels_ = 0;
    std::vector<std::unique_ptr<playsound_t>> playsounds_;
    std::deque<playsound_t *> freeplays_;
    std::deque<playsound_t *> pendingplays_;
    vec3_t listener_origin_ = { 0.0f, 0.0f, 0.0f };
    vec3_t listener_forward_ = { 0.0f, 0.0f, 0.0f };
    vec3_t listener_right_ = { 0.0f, 0.0f, 0.0f };
    vec3_t listener_up_ = { 0.0f, 0.0f, 0.0f };
    int listener_entnum_ = -1;
};

SoundSystem &S_GetSoundSystem();

