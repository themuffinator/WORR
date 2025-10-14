#include "renderer/kfont.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>

#include "common/common.h"
#include "common/files.h"
#include "renderer/images.h"

namespace {
    std::string normalizeTexturePath(const char *path) {
        if (!path || !*path) {
            return {};
        }

        std::string normalized{path};
        if (!normalized.empty() && normalized.front() != '/') {
            normalized.insert(normalized.begin(), '/');
        }
        return normalized;
    }
}

bool Renderer_LoadKFont(const char *filename, const RendererKFontLoadContext &context, RendererKFontData *out) {
    if (!out) {
        return false;
    }

    *out = RendererKFontData{};

    if (!filename || !*filename || !context.registerImage) {
        return false;
    }

    char *buffer = nullptr;
    if (FS_LoadFile(filename, reinterpret_cast<void **>(&buffer)) < 0 || !buffer) {
        return false;
    }

    std::string texturePath;

    const char *data = buffer;
    while (true) {
        const char *token = COM_Parse(&data);
        if (!token || !*token) {
            break;
        }

        if (!std::strcmp(token, "texture")) {
            const char *textureToken = COM_Parse(&data);
            if (textureToken && *textureToken) {
                texturePath = textureToken;
            }
        } else if (!std::strcmp(token, "unicode")) {
            do {
                token = COM_Parse(&data);
            } while (token && *token && std::strcmp(token, "}"));
        } else if (!std::strcmp(token, "mapchar")) {
            token = COM_Parse(&data);
            while (true) {
                token = COM_Parse(&data);
                if (!token || !*token) {
                    break;
                }
                if (!std::strcmp(token, "}")) {
                    break;
                }

                const char *xToken = COM_Parse(&data);
                const char *yToken = COM_Parse(&data);
                const char *wToken = COM_Parse(&data);
                const char *hToken = COM_Parse(&data);
                const char *sheetToken = COM_Parse(&data);
                (void)sheetToken;

                if (!xToken || !*xToken || !yToken || !*yToken ||
                    !wToken || !*wToken || !hToken || !*hToken) {
                    continue;
                }

                uint32_t codepoint = static_cast<uint32_t>(std::strtoul(token, nullptr, 10));
                if (codepoint < KFONT_ASCII_MIN || codepoint > KFONT_ASCII_MAX) {
                    continue;
                }

                const uint32_t xValue = static_cast<uint32_t>(std::strtoul(xToken, nullptr, 10));
                const uint32_t yValue = static_cast<uint32_t>(std::strtoul(yToken, nullptr, 10));
                const uint32_t wValue = static_cast<uint32_t>(std::strtoul(wToken, nullptr, 10));
                const uint32_t hValue = static_cast<uint32_t>(std::strtoul(hToken, nullptr, 10));

                const size_t index = static_cast<size_t>(codepoint - KFONT_ASCII_MIN);
                auto &glyph = out->glyphs[index];
                glyph.x = static_cast<uint16_t>(xValue);
                glyph.y = static_cast<uint16_t>(yValue);
                glyph.w = static_cast<uint16_t>(wValue);
                glyph.h = static_cast<uint16_t>(hValue);
                out->lineHeight = std::max<uint16_t>(out->lineHeight, glyph.h);
            }
        }
    }

    FS_FreeFile(buffer);

    if (texturePath.empty()) {
        return false;
    }

    std::string normalized = normalizeTexturePath(texturePath.c_str());
    if (normalized.empty()) {
        return false;
    }

    qhandle_t texture = context.registerImage(context.userData, normalized.c_str(), IT_FONT, IF_PERMANENT);
    if (!texture) {
        return false;
    }

    const image_t *image = IMG_ForHandle(texture);
    if (!image || image->width <= 0 || image->height <= 0) {
        return false;
    }

    out->texture = texture;
    out->sw = 1.0f / static_cast<float>(image->width);
    out->sh = 1.0f / static_cast<float>(image->height);
    if (out->lineHeight == 0) {
        out->lineHeight = RENDERER_DEFAULT_KFONT_HEIGHT;
    }

    return true;
}

void Renderer_AssignKFont(kfont_t *font, const RendererKFontData &data) {
    if (!font) {
        return;
    }

    std::memset(font, 0, sizeof(*font));
    font->pic = data.texture;
    font->line_height = data.lineHeight;
    font->sw = data.sw;
    font->sh = data.sh;
    std::copy(data.glyphs.begin(), data.glyphs.end(), std::begin(font->chars));
}

void Renderer_BuildFallbackKFont(uint16_t glyphWidth, uint16_t glyphHeight, qhandle_t texture, RendererKFontData *out) {
    if (!out) {
        return;
    }

    RendererKFontData fallback{};
    fallback.texture = texture;
    fallback.lineHeight = glyphHeight;
    fallback.sw = 1.0f;
    fallback.sh = 1.0f;

    uint16_t cursorX = 0;
    for (auto &glyph : fallback.glyphs) {
        glyph.x = cursorX;
        glyph.y = 0;
        glyph.w = glyphWidth;
        glyph.h = glyphHeight;
        cursorX = static_cast<uint16_t>(cursorX + glyphWidth);
    }

    *out = fallback;
}

const kfont_char_t *Renderer_LookupKFontGlyph(const RendererKFontData &data, uint32_t codepoint) {
    return Renderer_LookupKFontGlyph(data.glyphs.data(), data.glyphs.size(), codepoint);
}

const kfont_char_t *Renderer_LookupKFontGlyph(const kfont_t *font, uint32_t codepoint) {
    if (!font) {
        return nullptr;
    }
    return Renderer_LookupKFontGlyph(font->chars, RENDERER_KFONT_GLYPH_COUNT, codepoint);
}

const kfont_char_t *Renderer_LookupKFontGlyph(const kfont_char_t *glyphs, size_t count, uint32_t codepoint) {
    if (!glyphs) {
        return nullptr;
    }

    if (codepoint < KFONT_ASCII_MIN || codepoint > KFONT_ASCII_MAX) {
        return nullptr;
    }

    size_t index = static_cast<size_t>(codepoint - KFONT_ASCII_MIN);
    if (index >= count) {
        return nullptr;
    }

    const kfont_char_t &glyph = glyphs[index];
    if (glyph.w == 0) {
        return nullptr;
    }

    return &glyph;
}


