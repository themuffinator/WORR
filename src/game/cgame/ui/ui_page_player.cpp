#include "ui/ui_internal.h"

#include <algorithm>
#include <cmath>

#include "../../sgame/monsters/m_player.hpp"

namespace ui {

static const char *const handedness[] = { "right", "left", "center", nullptr };

struct PreviewStage {
    int start = 0;
    int end = 0;
    int frameMsec = 120;
    int loops = 1;
    int holdMsec = 0;
    bool fire = false;
    bool switchWeapon = false;
};

static const PreviewStage kPreviewStages[] = {
    { FRAME_stand01, FRAME_stand40, 120, 2, 0, false, false },
    { FRAME_run1, FRAME_run6, 90, 4, 0, false, false },
    { FRAME_pain301, FRAME_pain304, 90, 1, 0, false, true },
    { FRAME_attack1, FRAME_attack8, 80, 2, 0, true, false },
    { FRAME_crstnd01, FRAME_crstnd19, 120, 1, 0, false, false },
    { FRAME_crattak1, FRAME_crattak9, 80, 1, 0, true, false },
    { FRAME_death101, FRAME_death106, 120, 1, 1200, false, false }
};
static constexpr int kPreviewStageCount = q_countof(kPreviewStages);

static bool IsPlayerWeaponModel(const char *name)
{
    if (!name || !*name)
        return false;
    if (!Q_stricmp(name, "weapon.md2"))
        return true;
    return !Q_strncasecmp(name, "w_", 2);
}

class PlayerConfigPage : public MenuPage {
public:
    PlayerConfigPage();
    const char *Name() const override { return "players"; }
    void OnOpen() override;
    void OnClose() override;
    void Draw() override;
    Sound KeyEvent(int key) override;
    Sound CharEvent(int ch) override;
    void MouseEvent(int x, int y, bool down) override;
    bool WantsTextCursor(int x, int y) const override;

private:
    void ReloadMedia();
    void RunFrame();
    void UpdatePreviewLayout();
    void RefreshSkinList();
    void BuildWeaponList(const char *model);
    void AdvanceWeapon();
    void SyncWeaponEntity();
    void SetStage(int index);
    void AdvanceStage();
    void UpdateMuzzleFlash();

    Menu menu_;
    FieldWidget *name_ = nullptr;
    SpinWidget *model_ = nullptr;
    SpinWidget *skin_ = nullptr;
    SpinWidget *hand_ = nullptr;
    ImageSpinWidget *dogtag_ = nullptr;

    refdef_t refdef_{};
    entity_t entities_[2]{};
    unsigned time_ = 0;
    unsigned oldTime_ = 0;
    int stageIndex_ = 0;
    int stageLoopsLeft_ = 0;
    int animStart_ = 0;
    int animEnd_ = 0;
    int frameMsec_ = 120;
    unsigned holdUntil_ = 0;
    unsigned muzzleFlashUntil_ = 0;
    std::vector<qhandle_t> weaponModels_;
    int weaponIndex_ = -1;
    float baseYaw_ = 260.0f;
    float rotationSpeed_ = 0.02f;
    dlight_t muzzleLight_{};
    int lastModel_ = -1;
    int lastSkin_ = -1;
};

PlayerConfigPage::PlayerConfigPage()
    : menu_("players")
{
    auto modelSpin = std::make_unique<SpinWidget>("model", nullptr, SpinType::Index);
    model_ = modelSpin.get();

    auto skinSpin = std::make_unique<SpinWidget>("skin", nullptr, SpinType::Index);
    skin_ = skinSpin.get();

    auto handSpin = std::make_unique<SpinWidget>("hand", Cvar_FindVar("hand"), SpinType::Index);
    hand_ = handSpin.get();
    for (int i = 0; handedness[i]; i++)
        hand_->AddOption(handedness[i]);

    cvar_t *dogtag = Cvar_FindVar("dogtag");
    if (!dogtag)
        dogtag = Cvar_Get("dogtag", "default", CVAR_USERINFO | CVAR_ARCHIVE);
    auto dogtagSpin = std::make_unique<ImageSpinWidget>("tag", dogtag, "tags", "*", 192, 32);
    dogtag_ = dogtagSpin.get();

    auto nameField = std::make_unique<FieldWidget>("name", Cvar_FindVar("name"),
                                                   MAX_CLIENT_NAME - 1, false, false, false);
    name_ = nameField.get();

    menu_.AddWidget(std::move(modelSpin));
    menu_.AddWidget(std::move(skinSpin));
    menu_.AddWidget(std::move(dogtagSpin));
    menu_.AddWidget(std::move(nameField));
    menu_.AddWidget(std::move(handSpin));

    const vec3_t origin = { 28.0f, 0.0f, 0.0f };
    const vec3_t angles = { 0.0f, 260.0f, 0.0f };

    entities_[0].flags = RF_FULLBRIGHT;
    VectorCopy(angles, entities_[0].angles);
    VectorCopy(origin, entities_[0].origin);
    VectorCopy(origin, entities_[0].oldorigin);

    entities_[1].flags = RF_FULLBRIGHT;
    VectorCopy(angles, entities_[1].angles);
    VectorCopy(origin, entities_[1].origin);
    VectorCopy(origin, entities_[1].oldorigin);

    refdef_.num_entities = 0;
    refdef_.entities = entities_;
    refdef_.rdflags = RDF_NOWORLDMODEL;
    refdef_.dof_strength = 0.0f;
    refdef_.num_dlights = 0;
    refdef_.dlights = &muzzleLight_;
}

void PlayerConfigPage::ReloadMedia()
{
    char scratch[MAX_QPATH];
    if (model_->Current() < 0 || model_->Current() >= uis.numPlayerModels)
        return;
    if (skin_->Current() < 0 || skin_->Current() >= uis.pmi[model_->Current()].nskins)
        return;

    const char *model = uis.pmi[model_->Current()].directory;
    const char *skin = uis.pmi[model_->Current()].skindisplaynames[skin_->Current()];

    refdef_.num_entities = 0;

    Q_concat(scratch, sizeof(scratch), "players/", model, "/tris.md2");
    entities_[0].model = R_RegisterModel(scratch);
    if (!entities_[0].model)
        return;
    refdef_.num_entities++;

    Q_concat(scratch, sizeof(scratch), "players/", model, "/", skin, ".pcx");
    entities_[0].skin = R_RegisterSkin(scratch);

    BuildWeaponList(model);
}

void PlayerConfigPage::BuildWeaponList(const char *model)
{
    char scratch[MAX_QPATH];

    weaponModels_.clear();
    weaponIndex_ = -1;
    entities_[1].model = 0;
    if (refdef_.num_entities > 1)
        refdef_.num_entities = 1;

    Q_concat(scratch, sizeof(scratch), "players/", model);
    int file_count = 0;
    char **files = (char **)FS_ListFiles(scratch, ".md2", 0, &file_count);
    std::vector<std::string> names;
    if (files) {
        names.reserve(file_count);
        for (int i = 0; i < file_count; i++) {
            if (IsPlayerWeaponModel(files[i]))
                names.emplace_back(files[i]);
        }
        FS_FreeList((void **)files);
    }

    std::sort(names.begin(), names.end(),
              [](const std::string &a, const std::string &b) {
                  return Q_stricmp(a.c_str(), b.c_str()) < 0;
              });

    for (const auto &name : names) {
        Q_concat(scratch, sizeof(scratch), "players/", model, "/", name.c_str());
        qhandle_t handle = R_RegisterModel(scratch);
        if (handle) {
            weaponModels_.push_back(handle);
        }
    }

    if (weaponModels_.empty() && uis.weaponModel[0]) {
        Q_concat(scratch, sizeof(scratch), "players/", model, "/", uis.weaponModel);
        qhandle_t handle = R_RegisterModel(scratch);
        if (handle) {
            weaponModels_.push_back(handle);
        }
    }

    if (!weaponModels_.empty()) {
        weaponIndex_ = static_cast<int>(Q_rand_uniform(static_cast<uint32_t>(weaponModels_.size())));
        entities_[1].model = weaponModels_[weaponIndex_];
        refdef_.num_entities = max(refdef_.num_entities, 2);
        SyncWeaponEntity();
    }
}

void PlayerConfigPage::AdvanceWeapon()
{
    int count = static_cast<int>(weaponModels_.size());
    if (count <= 0)
        return;

    int next = weaponIndex_;
    if (count > 1) {
        int attempts = 0;
        do {
            next = static_cast<int>(Q_rand_uniform(count));
        } while (next == weaponIndex_ && ++attempts < 4);
        if (next == weaponIndex_)
            next = (weaponIndex_ + 1) % count;
    } else {
        next = 0;
    }

    weaponIndex_ = next;
    entities_[1].model = weaponModels_[weaponIndex_];
    refdef_.num_entities = max(refdef_.num_entities, 2);
    SyncWeaponEntity();
}

void PlayerConfigPage::SyncWeaponEntity()
{
    if (weaponIndex_ < 0 || weaponIndex_ >= static_cast<int>(weaponModels_.size()))
        return;

    VectorCopy(entities_[0].origin, entities_[1].origin);
    VectorCopy(entities_[0].oldorigin, entities_[1].oldorigin);
    VectorCopy(entities_[0].angles, entities_[1].angles);
    entities_[1].frame = entities_[0].frame;
    entities_[1].oldframe = entities_[0].oldframe;
    entities_[1].backlerp = entities_[0].backlerp;
    entities_[1].flags = entities_[0].flags;
    entities_[1].alpha = entities_[0].alpha;
    entities_[1].rgba = entities_[0].rgba;
    VectorCopy(entities_[0].scale, entities_[1].scale);
    entities_[1].bottom_z = entities_[0].bottom_z;
    entities_[1].skinnum = 0;
    entities_[1].skin = 0;
}

void PlayerConfigPage::SetStage(int index)
{
    if (index < 0 || index >= kPreviewStageCount)
        index = 0;

    stageIndex_ = index;
    const PreviewStage &stage = kPreviewStages[stageIndex_];

    stageLoopsLeft_ = max(1, stage.loops);
    animStart_ = stage.start;
    animEnd_ = stage.end;
    frameMsec_ = max(1, stage.frameMsec);
    holdUntil_ = 0;
    muzzleFlashUntil_ = 0;

    entities_[0].frame = animStart_;
    entities_[0].oldframe = animStart_;
    if (refdef_.num_entities > 1) {
        entities_[1].frame = entities_[0].frame;
        entities_[1].oldframe = entities_[0].oldframe;
    }

    time_ = uis.realtime - frameMsec_;
    oldTime_ = time_;

    if (stage.switchWeapon)
        AdvanceWeapon();
    else
        SyncWeaponEntity();
}

void PlayerConfigPage::AdvanceStage()
{
    int next = stageIndex_ + 1;
    if (next >= kPreviewStageCount)
        next = 0;
    SetStage(next);
}

void PlayerConfigPage::UpdateMuzzleFlash()
{
    refdef_.num_dlights = 0;
    if (uis.realtime >= muzzleFlashUntil_ || refdef_.num_entities < 2)
        return;

    vec3_t forward, right, up;
    AngleVectors(entities_[0].angles, forward, right, up);

    VectorCopy(entities_[0].origin, muzzleLight_.origin);
    VectorMA(muzzleLight_.origin, 18.0f, forward, muzzleLight_.origin);
    VectorMA(muzzleLight_.origin, 8.0f, right, muzzleLight_.origin);
    VectorMA(muzzleLight_.origin, 8.0f, up, muzzleLight_.origin);

    muzzleLight_.radius = 120.0f;
    muzzleLight_.intensity = 1.0f;
    muzzleLight_.color[0] = 1.0f;
    muzzleLight_.color[1] = 0.8f;
    muzzleLight_.color[2] = 0.4f;

    refdef_.num_dlights = 1;
}

void PlayerConfigPage::RunFrame()
{
    if (holdUntil_) {
        oldTime_ = time_ = uis.realtime;
        if (uis.realtime >= holdUntil_) {
            holdUntil_ = 0;
            AdvanceStage();
        }
        return;
    }

    if (time_ < uis.realtime) {
        oldTime_ = time_;
        time_ += frameMsec_;
        if (time_ < uis.realtime)
            time_ = uis.realtime;

        int nextFrame = entities_[0].frame + 1;
        const PreviewStage &stage = kPreviewStages[stageIndex_];
        if (nextFrame > animEnd_) {
            stageLoopsLeft_--;
            if (stageLoopsLeft_ > 0) {
                nextFrame = animStart_;
            } else {
                nextFrame = animEnd_;
                if (stage.holdMsec > 0) {
                    holdUntil_ = uis.realtime + stage.holdMsec;
                    oldTime_ = time_ = uis.realtime;
                } else {
                    AdvanceStage();
                    return;
                }
            }
        }

        entities_[0].oldframe = entities_[0].frame;
        entities_[0].frame = nextFrame;
        if (refdef_.num_entities > 1)
            SyncWeaponEntity();

        if (stage.fire && ((nextFrame - animStart_) & 1)) {
            muzzleFlashUntil_ = uis.realtime + 60;
        }
    }
}

void PlayerConfigPage::UpdatePreviewLayout()
{
    float inv_scale = (uis.scale > 0.0f) ? (1.0f / uis.scale) : 1.0f;
    int scaled_w = uis.width;
    int scaled_h = uis.height;
    int h = Q_rint(scaled_h * inv_scale);

    int pad = CONCHAR_WIDTH * 2;
    int right_column = max(CONCHAR_WIDTH * 12, scaled_w / 3);
    int menu_left = pad;
    int menu_width = scaled_w - right_column - pad * 2;
    bool split_columns = menu_width >= CONCHAR_WIDTH * 18;
    if (split_columns) {
        menu_.SetLayoutBounds(menu_left, menu_width);
    } else {
        menu_.ClearLayoutBounds();
    }
    menu_.RefreshLayout();

    int preview_left = scaled_w / 2;
    int preview_width = scaled_w - preview_left;
    if (split_columns) {
        preview_left = menu_left + menu_width + pad;
        preview_width = scaled_w - preview_left - pad;
    }
    if (preview_width < CONCHAR_WIDTH * 10) {
        preview_left = scaled_w / 2;
        preview_width = scaled_w - preview_left;
    }
    if (preview_width < 1)
        preview_width = max(1, scaled_w - preview_left);

    refdef_.x = Q_rint(preview_left * inv_scale);
    refdef_.width = max(1, Q_rint(preview_width * inv_scale));

    int menuTop = menu_.ContentTop();
    int menuBottom = menu_.ContentBottom();
    int centerY = (menuBottom > menuTop) ? (menuTop + menuBottom) / 2 : (scaled_h / 2);
    int targetHeight = min(scaled_h - pad * 2, Q_rint(scaled_h * 0.9f));
    refdef_.height = max(1, Q_rint(targetHeight * inv_scale));

    int centerYScaled = Q_rint(centerY * inv_scale);
    int maxY = max(0, h - refdef_.height);
    refdef_.y = Q_clip(centerYScaled - refdef_.height / 2, 0, maxY);

    refdef_.fov_x = 65;
    refdef_.fov_y = V_CalcFov(refdef_.fov_x, refdef_.width, refdef_.height);
}

void PlayerConfigPage::RefreshSkinList()
{
    skin_->ClearOptions();
    if (model_->Current() < 0 || model_->Current() >= uis.numPlayerModels)
        return;

    const playerModelInfo_t &info = uis.pmi[model_->Current()];
    for (int i = 0; i < info.nskins; i++)
        skin_->AddOption(info.skindisplaynames[i]);

    skin_->SetCurrent(0);
}

void PlayerConfigPage::OnOpen()
{
    if (!uis.numPlayerModels) {
        PlayerModel_Load();
        if (!uis.numPlayerModels) {
            Com_Printf("$cg_auto_f0e9a2b48b82");
            return;
        }
    }

    char currentdirectory[MAX_QPATH];
    char currentskin[MAX_QPATH];
    Q_strlcpy(currentdirectory, "male", sizeof(currentdirectory));
    Q_strlcpy(currentskin, "grunt", sizeof(currentskin));

    Cvar_VariableStringBuffer("skin", currentdirectory, sizeof(currentdirectory));
    char *p = strchr(currentdirectory, '/');
    if (!p)
        p = strchr(currentdirectory, '\\');
    if (p) {
        *p++ = 0;
        Q_strlcpy(currentskin, p, sizeof(currentskin));
    }

    int currentdirectoryindex = 0;
    int currentskinindex = 0;
    for (int i = 0; i < uis.numPlayerModels; i++) {
        if (Q_stricmp(uis.pmi[i].directory, currentdirectory) == 0) {
            currentdirectoryindex = i;
            for (int j = 0; j < uis.pmi[i].nskins; j++) {
                if (Q_stricmp(uis.pmi[i].skindisplaynames[j], currentskin) == 0) {
                    currentskinindex = j;
                    break;
                }
            }
        }
    }

    model_->ClearOptions();
    for (int i = 0; i < uis.numPlayerModels; i++)
        model_->AddOption(uis.pmi[i].directory);
    model_->SetCurrent(currentdirectoryindex);

    RefreshSkinList();
    skin_->SetCurrent(currentskinindex);

    int hand = Cvar_VariableInteger("hand");
    if (hand < 0 || hand > 2)
        hand = 0;
    hand_->SetCurrent(hand);

    if (uis.backgroundHandle) {
        menu_.SetBackgroundImage(uis.backgroundHandle, uis.transparent);
    } else {
        menu_.SetBackground(uis.color.background);
    }

    menu_.SetTitle("Player Configuration");

    menu_.OnOpen();

    ReloadMedia();
    SetStage(0);
    UpdatePreviewLayout();

    lastModel_ = model_->Current();
    lastSkin_ = skin_->Current();
}

void PlayerConfigPage::OnClose()
{
    menu_.OnClose();

    if (model_ && skin_) {
        char scratch[MAX_QPATH];
        Q_concat(scratch, sizeof(scratch),
                 uis.pmi[model_->Current()].directory, "/",
                 uis.pmi[model_->Current()].skindisplaynames[skin_->Current()]);
        Cvar_SetEx("skin", scratch, FROM_CONSOLE);
    }
}

Sound PlayerConfigPage::KeyEvent(int key)
{
    Sound sound = menu_.KeyEvent(key);

    if (model_ && skin_) {
        if (model_->Current() != lastModel_) {
            RefreshSkinList();
            lastModel_ = model_->Current();
            lastSkin_ = skin_->Current();
            ReloadMedia();
            SetStage(stageIndex_);
        } else if (skin_->Current() != lastSkin_) {
            lastSkin_ = skin_->Current();
            ReloadMedia();
            SetStage(stageIndex_);
        }
    }

    return sound;
}

Sound PlayerConfigPage::CharEvent(int ch)
{
    return menu_.CharEvent(ch);
}

void PlayerConfigPage::MouseEvent(int x, int y, bool down)
{
    menu_.MouseEvent(x, y, down);
}

bool PlayerConfigPage::WantsTextCursor(int x, int y) const
{
    (void)x;
    (void)y;
    return menu_.HoverTextInput();
}

void PlayerConfigPage::Draw()
{
    float backlerp = 0.0f;
    refdef_.time = uis.realtime * 0.001f;

    RunFrame();
    if (time_ != oldTime_) {
        backlerp = 1.0f - static_cast<float>(uis.realtime - oldTime_) /
                             static_cast<float>(time_ - oldTime_);
    }

    float yaw = std::fmod(baseYaw_ + uis.realtime * rotationSpeed_, 360.0f);
    for (int i = 0; i < refdef_.num_entities; i++) {
        entities_[i].backlerp = backlerp;
        entities_[i].angles[1] = yaw;
    }

    UpdateMuzzleFlash();

    UpdatePreviewLayout();
    menu_.Draw();

    R_RenderFrame(&refdef_);
    R_SetScale(uis.scale);
}

std::unique_ptr<MenuPage> CreatePlayerConfigPage()
{
    return std::make_unique<PlayerConfigPage>();
}

} // namespace ui
