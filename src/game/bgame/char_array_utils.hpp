#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <string_view>

// Returns true when the first character in the null-terminated buffer is the
// string terminator, indicating the array does not contain any text.
template <std::size_t N>
[[nodiscard]] constexpr bool CharArrayIsBlank(const std::array<char, N> &buffer) noexcept
{
        static_assert(N > 0, "character arrays used for text must have capacity");
        return buffer[0] == '\0';
}

// Convenience wrapper for readability when a caller needs to test for text
// content without manually negating CharArrayIsBlank.
template <std::size_t N>
[[nodiscard]] constexpr bool CharArrayHasText(const std::array<char, N> &buffer) noexcept
{
        return !CharArrayIsBlank(buffer);
}

// Returns a non-owning view of the null-terminated character array's contents.
// The view length is clamped to the first null character so we never read past
// the end when the buffer is not fully populated.
template <std::size_t N>
[[nodiscard]] inline std::string_view CharArrayToStringView(const std::array<char, N>& buffer) noexcept
{
        const auto terminator = std::find(buffer.begin(), buffer.end(), '\0');
        const auto length = static_cast<std::size_t>(terminator - buffer.begin());
        return std::string_view(buffer.data(), length);
}

// Convenience helper that returns an owning std::string copy of the character
// array contents while still respecting the null terminator.
template <std::size_t N>
[[nodiscard]] inline std::string CharArrayToString(const std::array<char, N>& buffer)
{
        return std::string(CharArrayToStringView(buffer));
}
