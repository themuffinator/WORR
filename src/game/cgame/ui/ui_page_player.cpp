#include "ui/ui_internal.h"

namespace ui {

static const char *const handedness[] = { "right", "left", "center", nullptr };

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

private:
    void ReloadMedia();
    void RunFrame();
    void UpdatePreviewLayout();
    void RefreshSkinList();

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
    int lastModel_ = -1;
    int lastSkin_ = -1;
};

PlayerConfigPage::PlayerConfigPage()
    : menu_("players")
{
    auto nameField = std::make_unique<FieldWidget>("name", Cvar_FindVar("name"),
                                                   MAX_CLIENT_NAME - 1, false, false, false);
    name_ = nameField.get();

    auto modelSpin = std::make_unique<SpinWidget>("model", nullptr, SpinType::Index);
    model_ = modelSpin.get();

    auto skinSpin = std::make_unique<SpinWidget>("skin", nullptr, SpinType::Index);
    skin_ = skinSpin.get();

    auto handSpin = std::make_unique<SpinWidget>("handedness", Cvar_FindVar("hand"), SpinType::Index);
    hand_ = handSpin.get();
    for (int i = 0; handedness[i]; i++)
        hand_->AddOption(handedness[i]);

    cvar_t *dogtag = Cvar_FindVar("dogtag");
    if (!dogtag)
        dogtag = Cvar_Get("dogtag", "default", CVAR_USERINFO | CVAR_ARCHIVE);
    auto dogtagSpin = std::make_unique<ImageSpinWidget>("dogtag", dogtag, "tags", "*", 192, 32);
    dogtag_ = dogtagSpin.get();

    menu_.AddWidget(std::move(nameField));
    menu_.AddWidget(std::move(modelSpin));
    menu_.AddWidget(std::move(skinSpin));
    menu_.AddWidget(std::move(handSpin));
    menu_.AddWidget(std::move(dogtagSpin));

    const vec3_t origin = { 40.0f, 0.0f, 0.0f };
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

    if (!uis.weaponModel[0])
        return;

    Q_concat(scratch, sizeof(scratch), "players/", model, "/", uis.weaponModel);
    entities_[1].model = R_RegisterModel(scratch);
    if (!entities_[1].model)
        return;
    refdef_.num_entities++;
}

void PlayerConfigPage::RunFrame()
{
    if (time_ < uis.realtime) {
        oldTime_ = time_;
        time_ += 120;
        if (time_ < uis.realtime)
            time_ = uis.realtime;
        int frame = (time_ / 120) % 40;
        for (int i = 0; i < refdef_.num_entities; i++) {
            entities_[i].oldframe = entities_[i].frame;
            entities_[i].frame = frame;
        }
    }
}

void PlayerConfigPage::UpdatePreviewLayout()
{
    int w = static_cast<int>(uis.width / uis.scale);
    int h = static_cast<int>(uis.height / uis.scale);

    refdef_.x = w / 2;
    refdef_.y = h / 10;
    refdef_.width = w / 2;
    refdef_.height = h - h / 5;
    refdef_.fov_x = 90;
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
            Com_Printf("No player models found.\n");
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

    qhandle_t banner = R_RegisterPic("m_banner_plauer_setup");
    if (banner) {
        vrect_t rc{};
        R_GetPicSize(&rc.width, &rc.height, banner);
        menu_.SetBanner(banner, rc);
        menu_.SetTitle("");
    } else {
        menu_.SetTitle("Player Setup");
    }

    menu_.OnOpen();

    ReloadMedia();
    time_ = uis.realtime - 120;
    oldTime_ = time_;
    RunFrame();
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
        } else if (skin_->Current() != lastSkin_) {
            lastSkin_ = skin_->Current();
            ReloadMedia();
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

void PlayerConfigPage::Draw()
{
    float backlerp = 0.0f;
    refdef_.time = uis.realtime * 0.001f;

    RunFrame();
    if (time_ != oldTime_) {
        backlerp = 1.0f - static_cast<float>(uis.realtime - oldTime_) /
                             static_cast<float>(time_ - oldTime_);
    }

    for (int i = 0; i < refdef_.num_entities; i++)
        entities_[i].backlerp = backlerp;

    menu_.Draw();
    R_RenderFrame(&refdef_);
    R_SetScale(uis.scale);
}

std::unique_ptr<MenuPage> CreatePlayerConfigPage()
{
    return std::make_unique<PlayerConfigPage>();
}

} // namespace ui
