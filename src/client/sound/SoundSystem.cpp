#include "SoundSystem.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

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

sfx_t *SoundSystem::AllocSfx()
{
    for (int i = 0; i < num_sfx_; i++) {
        sfx_t *sfx = &known_sfx_[i];
        if (!sfx->name[0]) {
            return sfx;
        }
    }

    if (num_sfx_ == max_sfx()) {
        return nullptr;
    }

    sfx_t *sfx = &known_sfx_[num_sfx_];
    set_num_sfx(num_sfx_ + 1);
    return sfx;
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

bool SoundSystem::is_registering() const
{
    return registering_;
}

sfx_t *SoundSystem::FindOrAllocateSfx(const char *name, size_t namelen)
{
    for (int i = 0; i < num_sfx_; i++) {
        sfx_t *sfx = &known_sfx_[i];
        if (!FS_pathcmp(sfx->name, name)) {
            sfx->registration_sequence = registration_sequence_;
            return sfx;
        }
    }

    sfx_t *sfx = AllocSfx();
    if (sfx) {
        std::memcpy(sfx->name, name, namelen + 1);
        sfx->registration_sequence = registration_sequence_;
    }
    return sfx;
}

void SoundSystem::FreeSound(sfx_t *sfx)
{
    if (s_started != SoundBackend::Not && s_api->delete_sfx) {
        s_api->delete_sfx(sfx);
    }
    Z_Free(sfx->cache);
    Z_Free(sfx->truename);
    std::memset(sfx, 0, sizeof(*sfx));
}

void SoundSystem::BeginRegistration()
{
    increment_registration_sequence();
    registering_ = true;
}

qhandle_t SoundSystem::RegisterSound(const char *name)
{
    char buffer[MAX_QPATH];
    size_t len;

    if (s_started == SoundBackend::Not) {
        return 0;
    }

    Q_assert(name);

    if (!*name) {
        return 0;
    }

    if (*name == '*') {
        len = Q_strlcpy(buffer, name, MAX_QPATH);
    } else if (*name == '#') {
        len = FS_NormalizePathBuffer(buffer, name + 1, MAX_QPATH);
    } else {
        len = Q_concat(buffer, MAX_QPATH, "sound/", name);
        if (len < MAX_QPATH) {
            len = FS_NormalizePath(buffer);
        }
    }

    if (len >= MAX_QPATH) {
        Com_DPrintf("%s: oversize name\n", __func__);
        return 0;
    }

    if (len == 0) {
        Com_DPrintf("%s: empty name\n", __func__);
        return 0;
    }

    sfx_t *sfx = FindOrAllocateSfx(buffer, len);
    if (!sfx) {
        Com_DPrintf("%s: out of slots\n", __func__);
        return 0;
    }

    if (!registering_) {
        LoadSound(sfx);
    }

    return static_cast<qhandle_t>((sfx - known_sfx_.data()) + 1);
}

sfx_t *SoundSystem::RegisterSexedSound(int entnum, const char *base)
{
    const char *model;
    char buffer[MAX_QPATH];

    if (entnum > 0 && entnum <= MAX_CLIENTS) {
        model = cl.clientinfo[entnum - 1].model_name;
    } else {
        model = cl.baseclientinfo.model_name;
    }

    if (!*model) {
        model = "male";
    }

    if (Q_concat(buffer, MAX_QPATH, "players/", model, "/", base + 1) >= MAX_QPATH &&
        Q_concat(buffer, MAX_QPATH, "players/", "male", "/", base + 1) >= MAX_QPATH) {
        return nullptr;
    }

    sfx_t *sfx = FindOrAllocateSfx(buffer, FS_NormalizePath(buffer));

    if (sfx && !sfx->truename && !is_registering() && !LoadSound(sfx)) {
        if (Q_concat(buffer, MAX_QPATH, "sound/player/male/", base + 1) < MAX_QPATH) {
            FS_NormalizePath(buffer);
            sfx->error = Q_ERR_SUCCESS;
            sfx->truename = S_CopyString(buffer);
        }
    }

    return sfx;
}

void SoundSystem::RegisterSexedSounds()
{
    std::vector<int> sounds;
    sounds.reserve(num_sfx_);

    for (int i = 0; i < num_sfx_; i++) {
        sfx_t *sfx = &known_sfx_[i];
        if (sfx->name[0] != '*') {
            continue;
        }
        if (sfx->registration_sequence != registration_sequence_) {
            continue;
        }
        sounds.push_back(i);
    }

    for (int i = 0; i <= MAX_CLIENTS; i++) {
        if (i > 0 && !cl.clientinfo[i - 1].model_name[0]) {
            continue;
        }
        for (int index : sounds) {
            sfx_t *sfx = &known_sfx_[index];
            RegisterSexedSound(i, sfx->name);
        }
    }
}

void SoundSystem::EndRegistration()
{
    RegisterSexedSounds();

    StopAllSounds();

    for (int i = 0; i < num_sfx_; i++) {
        sfx_t *sfx = &known_sfx_[i];
        if (!sfx->name[0]) {
            continue;
        }
        if (sfx->registration_sequence != registration_sequence_) {
            FreeSound(sfx);
            continue;
        }
        if (s_started != SoundBackend::Not && s_api->page_in_sfx) {
            s_api->page_in_sfx(sfx);
        }
    }

    for (int i = 0; i < num_sfx_; i++) {
        sfx_t *sfx = &known_sfx_[i];
        if (!sfx->name[0]) {
            continue;
        }
        LoadSound(sfx);
    }

    if (s_started != SoundBackend::Not && s_api->end_registration) {
        s_api->end_registration();
    }

    registering_ = false;
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
