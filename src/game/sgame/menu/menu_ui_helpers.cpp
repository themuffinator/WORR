#include "menu_ui_helpers.hpp"

#include <array>
#include <cctype>

namespace MenuUi {
namespace {

constexpr size_t kMaxStuffText = MAX_STRING_CHARS - 1;

} // namespace

std::string EscapeUiText(std::string_view text, size_t maxLen)
{
    static const char kHex[] = "0123456789ABCDEF";
    std::string out;
    if (maxLen == 0 || text.empty())
        return out;

    out.reserve(std::min(text.size(), maxLen));

    auto escape_char = [](unsigned char ch) -> int {
        switch (ch) {
        case '\a':
            return 'a';
        case '\b':
            return 'b';
        case '\t':
            return 't';
        case '\n':
            return 'n';
        case '\v':
            return 'v';
        case '\f':
            return 'f';
        case '\r':
            return 'r';
        case '\\':
            return '\\';
        case '"':
            return '"';
        default:
            return 0;
        }
    };

    for (unsigned char ch : text) {
        int escaped = escape_char(ch);
        if (escaped) {
            if (out.size() + 2 > maxLen)
                break;
            out.push_back('\\');
            out.push_back(static_cast<char>(escaped));
        } else if (std::isprint(ch)) {
            if (out.size() + 1 > maxLen)
                break;
            out.push_back(static_cast<char>(ch));
        } else {
            if (out.size() + 4 > maxLen)
                break;
            out.push_back('\\');
            out.push_back('x');
            out.push_back(kHex[ch >> 4]);
            out.push_back(kHex[ch & 15]);
        }
    }

    return out;
}

UiCommandBuilder::UiCommandBuilder(gentity_t *ent)
    : ent_(ent)
{
}

void UiCommandBuilder::FlushIfNeeded(size_t extra)
{
    if (!ent_)
        return;
    if (buffer_.empty())
        return;
    if (buffer_.size() + extra <= kMaxStuffText)
        return;

    SendUiCommand(ent_, buffer_);
    buffer_.clear();
}

void UiCommandBuilder::AppendCvar(const char *name, std::string_view value)
{
    if (!ent_ || !name || !*name)
        return;

    const std::string prefix = std::string("set ") + name + " \"";
    const std::string suffix = "\"; ";
    size_t maxValueLen = 0;
    if (kMaxStuffText > prefix.size() + suffix.size())
        maxValueLen = kMaxStuffText - prefix.size() - suffix.size();

    std::string escaped = EscapeUiText(value, maxValueLen);
    std::string chunk = prefix + escaped + suffix;

    FlushIfNeeded(chunk.size());
    buffer_ += chunk;
}

void UiCommandBuilder::AppendCommand(std::string_view command)
{
    if (!ent_ || command.empty())
        return;

    std::string chunk(command);
    if (chunk.back() != '\n' && chunk.back() != ';')
        chunk.append("; ");

    FlushIfNeeded(chunk.size());
    buffer_ += chunk;
}

void UiCommandBuilder::Flush()
{
    if (!ent_ || buffer_.empty())
        return;
    SendUiCommand(ent_, buffer_);
    buffer_.clear();
}

void SendUiCommand(gentity_t *ent, std::string_view cmd)
{
    if (!ent || cmd.empty())
        return;

    std::string text(cmd);
    gi.WriteByte(svc_stufftext);
    gi.WriteString(text.c_str());
    gi.unicast(ent, true);
}

} // namespace MenuUi
