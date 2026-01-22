// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_hud_blob.hpp"

#include <algorithm>

namespace {

uint32_t hud_blob_flags = 0;
std::string scoreboard_section;
std::string eou_section;
std::string last_blob;

static void G_HudBlob_EnsureTrailingNewline(std::string &section) {
  if (!section.empty() && section.back() != '\n')
    section.push_back('\n');
}

static std::string G_HudBlob_Build() {
  std::string blob;
  blob.reserve(512);

  blob += "HUD_BLOB v1\n";
  fmt::format_to(std::back_inserter(blob), FMT_STRING("hud_flags 0x{:x}\n"),
                 hud_blob_flags);

  if (!scoreboard_section.empty()) {
    blob += scoreboard_section;
    G_HudBlob_EnsureTrailingNewline(blob);
  }

  if (!eou_section.empty()) {
    blob += eou_section;
    G_HudBlob_EnsureTrailingNewline(blob);
  }

  return blob;
}

static std::string G_HudBlob_BuildClamped() {
  std::string blob = G_HudBlob_Build();
  if (blob.size() <= HUD_BLOB_MAX_SIZE)
    return blob;

  const size_t max_len = HUD_BLOB_MAX_SIZE;
  size_t cut = blob.rfind('\n', max_len - 1);
  if (cut == std::string::npos) {
    blob.resize(max_len);
    return blob;
  }

  blob.resize(cut + 1);
  return blob;
}

static void G_HudBlob_Commit() {
  std::string blob = G_HudBlob_BuildClamped();
  if (blob == last_blob)
    return;

  last_blob = blob;

  size_t offset = 0;
  for (int i = 0; i < HUD_BLOB_SEGMENTS; ++i) {
    std::string segment;
    if (offset < blob.size()) {
      size_t remaining = blob.size() - offset;
      size_t len = std::min(remaining, HUD_BLOB_SEGMENT_SIZE);
      segment.assign(blob, offset, len);
      offset += len;
    }

    gi.configString(CONFIG_HUD_BLOB + i, segment.c_str());
  }
}

} // namespace

void G_HudBlob_SetFlags(uint32_t flags) {
  if (hud_blob_flags == flags)
    return;

  hud_blob_flags = flags;
  G_HudBlob_Commit();
}

void G_HudBlob_SetScoreboardSection(const std::string &section) {
  if (scoreboard_section == section)
    return;

  scoreboard_section = section;
  G_HudBlob_Commit();
}

void G_HudBlob_ClearScoreboardSection() {
  if (scoreboard_section.empty())
    return;

  scoreboard_section.clear();
  G_HudBlob_Commit();
}

void G_HudBlob_SetEOUSection(const std::string &section) {
  if (eou_section == section)
    return;

  eou_section = section;
  G_HudBlob_Commit();
}

void G_HudBlob_ClearEOUSection() {
  if (eou_section.empty())
    return;

  eou_section.clear();
  G_HudBlob_Commit();
}
