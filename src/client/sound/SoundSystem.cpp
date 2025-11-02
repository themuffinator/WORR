#include "SoundSystem.hpp"

#include <algorithm>
#include <cstring>

SoundSystem::SoundSystem()
{
    Configure({ kDefaultMaxSfx, kDefaultMaxPlaysounds });
}

SoundSystem::SoundSystem(const Config &config)
{
    Configure(config);
}

void SoundSystem::Configure(const Config &config)
{
    config_ = config;
    InitializeStorage();
}

void SoundSystem::InitializeStorage()
{
    known_sfx_.assign(config_.maxSfx, sfx_t{});
    playsounds_.clear();
    playsounds_.reserve(config_.maxPlaysounds);
    freeplays_.clear();
    pendingplays_.clear();
    for (std::size_t i = 0; i < config_.maxPlaysounds; ++i) {
        auto playsound = std::make_unique<playsound_t>();
        std::memset(playsound.get(), 0, sizeof(*playsound));
        List_Init(&playsound->entry);
        freeplays_.push_back(playsound.get());
        playsounds_.push_back(std::move(playsound));
    }

    num_sfx_ = 0;
    registration_sequence_ = 1;
    clear_channels();
}

unsigned SoundSystem::registration_sequence() const
{
    return registration_sequence_;
}

void SoundSystem::reset_registration_sequence()
{
    registration_sequence_ = 1;
}

void SoundSystem::increment_registration_sequence()
{
    ++registration_sequence_;
}

SoundBackend SoundSystem::backend() const
{
    return backend_;
}

void SoundSystem::set_backend(SoundBackend backend)
{
    backend_ = backend;
}

const sndapi_t *SoundSystem::api() const
{
    return api_;
}

void SoundSystem::set_api(const sndapi_t *api)
{
    api_ = api;
}

bool SoundSystem::supports_float() const
{
    return supports_float_;
}

void SoundSystem::set_supports_float(bool supports)
{
    supports_float_ = supports;
}

bool SoundSystem::is_active() const
{
    return active_;
}

void SoundSystem::set_active(bool active)
{
    active_ = active;
}

bool SoundSystem::is_registering() const
{
    return registering_;
}

void SoundSystem::set_registering(bool registering)
{
    registering_ = registering;
}

int SoundSystem::painted_time() const
{
    return painted_time_;
}

int &SoundSystem::painted_time()
{
    return painted_time_;
}

void SoundSystem::set_painted_time(int time)
{
    painted_time_ = time;
}

sfx_t *SoundSystem::known_sfx_data()
{
    return known_sfx_.data();
}

const sfx_t *SoundSystem::known_sfx_data() const
{
    return known_sfx_.data();
}

int SoundSystem::num_sfx() const
{
    return num_sfx_;
}

void SoundSystem::set_num_sfx(int count)
{
    num_sfx_ = std::clamp(count, 0, static_cast<int>(known_sfx_.size()));
}

int SoundSystem::max_sfx() const
{
    return static_cast<int>(known_sfx_.size());
}

channel_t *SoundSystem::channels_data()
{
    return channels_.empty() ? nullptr : channels_.data();
}

const channel_t *SoundSystem::channels_data() const
{
    return channels_.empty() ? nullptr : channels_.data();
}

void SoundSystem::set_channel_capacity(int capacity)
{
    max_channels_ = capacity;
    channels_.assign(static_cast<std::size_t>((std::max)(capacity, 0)), channel_t{});
    clear_channels();
}

void SoundSystem::clear_channels()
{
    for (auto &channel : channels_) {
        std::memset(&channel, 0, sizeof(channel));
    }
}

int &SoundSystem::num_channels()
{
    return num_channels_;
}

int &SoundSystem::max_channels()
{
    return max_channels_;
}

playsound_t *SoundSystem::AllocatePlaysound()
{
    if (freeplays_.empty()) {
        return nullptr;
    }

    playsound_t *ps = freeplays_.front();
    freeplays_.pop_front();
    std::memset(ps, 0, sizeof(*ps));
    List_Init(&ps->entry);
    return ps;
}

void SoundSystem::FreePlaysound(playsound_t *ps)
{
    if (!ps) {
        return;
    }

    RemovePendingPlay(ps);
    std::memset(ps, 0, sizeof(*ps));
    List_Init(&ps->entry);
    freeplays_.push_back(ps);
}

void SoundSystem::QueuePendingPlay(playsound_t *ps)
{
    auto it = std::lower_bound(pendingplays_.begin(), pendingplays_.end(), ps,
                               [](const playsound_t *lhs, const playsound_t *rhs) {
                                   return lhs->begin < rhs->begin;
                               });
    pendingplays_.insert(it, ps);
}

void SoundSystem::RemovePendingPlay(playsound_t *ps)
{
    auto it = std::find(pendingplays_.begin(), pendingplays_.end(), ps);
    if (it != pendingplays_.end()) {
        pendingplays_.erase(it);
    }
}

playsound_t *SoundSystem::PeekPendingPlay()
{
    return pendingplays_.empty() ? nullptr : pendingplays_.front();
}

bool SoundSystem::HasPendingPlays() const
{
    return !pendingplays_.empty();
}

void SoundSystem::ResetPlaysounds()
{
    pendingplays_.clear();
    freeplays_.clear();
    for (auto &playsound : playsounds_) {
        std::memset(playsound.get(), 0, sizeof(*playsound));
        List_Init(&playsound->entry);
        freeplays_.push_back(playsound.get());
    }
}

vec3_t &SoundSystem::listener_origin()
{
    return listener_origin_;
}

vec3_t &SoundSystem::listener_forward()
{
    return listener_forward_;
}

vec3_t &SoundSystem::listener_right()
{
    return listener_right_;
}

vec3_t &SoundSystem::listener_up()
{
    return listener_up_;
}

int &SoundSystem::listener_entnum()
{
    return listener_entnum_;
}

SoundSystem &S_GetSoundSystem()
{
    static SoundSystem system;
    return system;
}

vec3_t &listener_origin = S_GetSoundSystem().listener_origin();
vec3_t &listener_forward = S_GetSoundSystem().listener_forward();
vec3_t &listener_right = S_GetSoundSystem().listener_right();
vec3_t &listener_up = S_GetSoundSystem().listener_up();
int &listener_entnum = S_GetSoundSystem().listener_entnum();

