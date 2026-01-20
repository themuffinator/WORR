#pragma once

#include "../g_local.hpp"

#include <string>
#include <string_view>

namespace MenuUi {

std::string EscapeUiText(std::string_view text, size_t maxLen = MAX_STRING_CHARS - 1);

class UiCommandBuilder {
public:
    explicit UiCommandBuilder(gentity_t *ent);

    void AppendCvar(const char *name, std::string_view value);
    void AppendCommand(std::string_view command);
    void Flush();

private:
    void FlushIfNeeded(size_t extra);

    gentity_t *ent_ = nullptr;
    std::string buffer_{};
};

void SendUiCommand(gentity_t *ent, std::string_view cmd);

} // namespace MenuUi
