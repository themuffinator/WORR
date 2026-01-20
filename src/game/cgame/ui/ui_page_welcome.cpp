#include "ui/ui_internal.h"

#include "../../bgame/version.hpp"

#include <sstream>

namespace ui {
namespace {

static std::string DecodeEscapes(const char *text)
{
    std::string out;
    if (!text || !*text)
        return out;

    for (size_t i = 0; text[i]; i++) {
        char ch = text[i];
        if (ch == '\\' && text[i + 1]) {
            char next = text[i + 1];
            if (next == 'n' || next == 'r' || next == 't' || next == 'v' || next == 'f') {
                out.push_back(' ');
                i++;
                continue;
            }
            if (next == '\\' || next == '"') {
                out.push_back(next);
                i++;
                continue;
            }
            if (next == 'x' && text[i + 2] && text[i + 3]) {
                out.push_back(' ');
                i += 3;
                continue;
            }
        }

        if (ch < 32)
            out.push_back(' ');
        else
            out.push_back(ch);
    }

    return out;
}

static std::vector<std::string> WrapText(const std::string &text, size_t maxWidth, size_t maxLines)
{
    std::vector<std::string> lines;
    if (text.empty() || maxWidth == 0 || maxLines == 0)
        return lines;

    std::istringstream stream(text);
    std::string word;
    std::string currentLine;

    while (stream >> word) {
        if (lines.size() >= maxLines)
            break;

        if (currentLine.empty()) {
            currentLine = word;
        } else if (currentLine.size() + 1 + word.size() <= maxWidth) {
            currentLine += " " + word;
        } else {
            lines.push_back(currentLine);
            currentLine = word;
            if (lines.size() >= maxLines)
                break;
        }
    }

    if (!currentLine.empty() && lines.size() < maxLines)
        lines.push_back(currentLine);

    return lines;
}

static size_t MaxLineChars()
{
    int columns = max(1, uis.width / CONCHAR_WIDTH);
    int maxChars = columns - 8;
    if (maxChars < 20)
        maxChars = columns;
    maxChars = Q_clip(maxChars, 20, 40);
    return static_cast<size_t>(maxChars);
}

} // namespace

class WelcomePage : public MenuPage {
public:
    WelcomePage();
    const char *Name() const override { return "dm_welcome"; }
    void OnOpen() override;
    void OnClose() override;
    void Draw() override;
    Sound KeyEvent(int key) override;
    Sound CharEvent(int ch) override;
    void MouseEvent(int x, int y, bool down) override;
    bool IsTransparent() const override { return menu_.IsTransparent(); }

private:
    void BuildMenu();
    void TriggerContinue();

    Menu menu_;
};

WelcomePage::WelcomePage()
    : menu_("dm_welcome")
{
}

void WelcomePage::BuildMenu()
{
    menu_ = Menu("dm_welcome");
    menu_.SetCompact(true);
    menu_.SetTransparent(true);
    menu_.SetBackground(COLOR_RGBA(0, 0, 0, 200));
    menu_.SetTitle("");

    auto addSpacer = [&]() {
        menu_.AddWidget(std::make_unique<SeparatorWidget>());
    };

    auto addText = [&](const std::string &text, bool center) {
        auto line = std::make_unique<ActionWidget>(std::string{});
        line->SetLabel(text);
        line->SetAlignLeft(!center);
        line->SetSelectable(false);
        menu_.AddWidget(std::move(line));
    };

    addText("Welcome to", true);

    std::string title(worr::version::kGameTitle);
    std::string version(worr::version::kGameVersion);
    addText(title + " v" + version, true);

    addSpacer();

    const char *hostname_var = "";
    if (cvar_t *host = Cvar_FindVar("ui_welcome_hostname"))
        hostname_var = host->string ? host->string : "";
    std::string hostname = DecodeEscapes(hostname_var);
    if (!hostname.empty()) {
        addText(hostname, true);
        addSpacer();
    }

    const char *motd_var = "";
    if (cvar_t *motd = Cvar_FindVar("ui_welcome_motd"))
        motd_var = motd->string ? motd->string : "";
    std::string motd = DecodeEscapes(motd_var);
    if (!motd.empty()) {
        auto lines = WrapText(motd, MaxLineChars(), 5);
        for (const auto &line : lines)
            addText(line, false);
        if (!lines.empty())
            addSpacer();
    }

    auto cont = std::make_unique<ActionWidget>("forcemenuoff; worr_welcome_continue");
    cont->SetLabel("Continue");
    cont->SetAlignLeft(true);
    menu_.AddWidget(std::move(cont));
}

void WelcomePage::TriggerContinue()
{
    Cbuf_AddText(&cmd_buffer, "forcemenuoff; worr_welcome_continue\n");
}

void WelcomePage::OnOpen()
{
    BuildMenu();
    menu_.OnOpen();
    menu_.ClearHints();
    menu_.AddHintLeft(K_ESCAPE, "Continue", "Esc");
    menu_.AddHintRight(K_ENTER, "Continue", "Enter");
}

void WelcomePage::OnClose()
{
    menu_.OnClose();
}

void WelcomePage::Draw()
{
    menu_.Draw();
}

Sound WelcomePage::KeyEvent(int key)
{
    if (key == K_ESCAPE || key == K_MOUSE2) {
        TriggerContinue();
        return Sound::Out;
    }

    return menu_.KeyEvent(key);
}

Sound WelcomePage::CharEvent(int ch)
{
    return menu_.CharEvent(ch);
}

void WelcomePage::MouseEvent(int x, int y, bool down)
{
    menu_.MouseEvent(x, y, down);
}

std::unique_ptr<MenuPage> CreateWelcomePage()
{
    return std::make_unique<WelcomePage>();
}

} // namespace ui
