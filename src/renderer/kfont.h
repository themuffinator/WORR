#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "renderer/common.h"

constexpr uint16_t RENDERER_DEFAULT_KFONT_WIDTH = 16;
constexpr uint16_t RENDERER_DEFAULT_KFONT_HEIGHT = 16;
constexpr size_t RENDERER_KFONT_GLYPH_COUNT = static_cast<size_t>(KFONT_ASCII_MAX - KFONT_ASCII_MIN + 1);

struct RendererKFontLoadContext {
    qhandle_t (*registerImage)(void *userData, const char *path, imagetype_t type, imageflags_t flags) = nullptr;
    void *userData = nullptr;
};

struct RendererKFontData {
    qhandle_t texture = 0;
    float sw = 1.0f;
    float sh = 1.0f;
    uint16_t lineHeight = 0;
    std::array<kfont_char_t, RENDERER_KFONT_GLYPH_COUNT> glyphs{};
};

bool Renderer_LoadKFont(const char *filename, const RendererKFontLoadContext &context, RendererKFontData *out);
void Renderer_AssignKFont(kfont_t *font, const RendererKFontData &data);
void Renderer_BuildFallbackKFont(uint16_t glyphWidth, uint16_t glyphHeight, qhandle_t texture, RendererKFontData *out);
const kfont_char_t *Renderer_LookupKFontGlyph(const RendererKFontData &data, uint32_t codepoint);
const kfont_char_t *Renderer_LookupKFontGlyph(const kfont_t *font, uint32_t codepoint);
const kfont_char_t *Renderer_LookupKFontGlyph(const kfont_char_t *glyphs, size_t count, uint32_t codepoint);

