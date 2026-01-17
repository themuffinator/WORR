#pragma once

#include "../sgame/g_local.hpp"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

inline char ToLowerASCII(char c) {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (uc >= 'A' && uc <= 'Z') {
                return static_cast<char>(uc - 'A' + 'a');
        }
        return static_cast<char>(uc);
}

inline char ToUpperASCII(char c) {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (uc >= 'a' && uc <= 'z') {
                return static_cast<char>(uc - 'a' + 'A');
        }
        return static_cast<char>(uc);
}

inline std::string NormalizeWeaponAbbreviation(std::string_view abbr) {
        std::string normalized;
        normalized.reserve(abbr.size());
        for (char ch : abbr) {
                normalized.push_back(ToUpperASCII(ch));
        }
        return normalized;
}

inline std::optional<Weapon> ParseNormalizedWeaponAbbreviation(std::string_view normalized) {
        for (std::size_t i = 0; i < weaponAbbreviations.size(); ++i) {
                if (normalized == weaponAbbreviations[i]) {
                        return static_cast<Weapon>(i);
                }
        }
        return std::nullopt;
}

inline std::optional<Weapon> ParseWeaponAbbreviation(std::string_view abbr) {
        const std::string normalized = NormalizeWeaponAbbreviation(abbr);
        auto weapon = ParseNormalizedWeaponAbbreviation(normalized);
        if (!weapon || *weapon == Weapon::None) {
                return std::nullopt;
        }
        return weapon;
}

enum class WeaponPrefAppendResult {
        Added,
        Duplicate,
        Invalid,
        CapacityExceeded,
};

constexpr size_t WeaponPreferenceCapacity = static_cast<size_t>(Weapon::Total) - 1;

inline WeaponPrefAppendResult TryAppendWeaponPreference(
        std::string_view token,
        std::vector<Weapon>& outPrefs,
        std::array<bool, static_cast<size_t>(Weapon::Total)>& seen,
        std::string* normalizedOut = nullptr) {
        const std::string normalized = NormalizeWeaponAbbreviation(token);
        if (normalizedOut) {
                *normalizedOut = normalized;
        }

        auto weapon = ParseNormalizedWeaponAbbreviation(normalized);
        if (!weapon || *weapon == Weapon::None) {
                return WeaponPrefAppendResult::Invalid;
        }

        const auto index = static_cast<size_t>(*weapon);
        if (index >= seen.size()) {
                return WeaponPrefAppendResult::Invalid;
        }

        if (outPrefs.size() >= WeaponPreferenceCapacity) {
                return WeaponPrefAppendResult::CapacityExceeded;
        }

        if (seen[index]) {
                return WeaponPrefAppendResult::Duplicate;
        }

        seen[index] = true;
        outPrefs.push_back(*weapon);
        return WeaponPrefAppendResult::Added;
}

inline std::string_view WeaponToAbbreviation(Weapon weapon) {
        const std::size_t index = static_cast<std::size_t>(weapon);
        if (index >= weaponAbbreviations.size()) {
                return {};
        }
        return weaponAbbreviations[index];
}
