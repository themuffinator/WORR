#include "renderer.h"

#include <algorithm>
#include <cmath>

refcfg_t r_config = {};
unsigned r_registration_sequence = 0;

namespace refresh::vk {

namespace {
    constexpr int kDefaultCharWidth = 8;
    constexpr int kDefaultCharHeight = 8;

    struct VideoGeometry {
        int width = SCREEN_WIDTH;
        int height = SCREEN_HEIGHT;
        vidFlags_t flags = {};
    };

    int countPrintable(std::string_view value, size_t maxChars) {
        size_t count = 0;
        for (char ch : value) {
            if (ch == '\0') {
                break;
            }
            if (maxChars && count >= maxChars) {
                break;
            }
            ++count;
        }
        return static_cast<int>(count);
    }

    constexpr uint16_t defaultKFontWidth() {
        return 16;
    }

    constexpr uint16_t defaultKFontHeight() {
        return 16;
    }

    VideoGeometry queryVideoGeometry() {
        VideoGeometry geometry{};

        vrect_t rect{};
        if (VID_GetGeometry(&rect)) {
            geometry.width = std::max(1, rect.width);
            geometry.height = std::max(1, rect.height);
        }

        vrect_t fullscreen{};
        int freq = 0;
        int depth = 0;
        if (VID_GetFullscreen(&fullscreen, &freq, &depth)) {
            geometry.flags = static_cast<vidFlags_t>(geometry.flags | QVF_FULLSCREEN);
        }

        return geometry;
    }

    void applyVideoGeometry(const VideoGeometry &geometry) {
        r_config.width = geometry.width;
        r_config.height = geometry.height;
        r_config.flags = geometry.flags;
    }
}

VulkanRenderer::VulkanRenderer()
    : handleCounter_{1} {
}

VulkanRenderer::~VulkanRenderer() = default;

qhandle_t VulkanRenderer::nextHandle() {
    return handleCounter_.fetch_add(1, std::memory_order_relaxed);
}

qhandle_t VulkanRenderer::registerResource(NameLookup &lookup, std::string_view name) {
    if (name.empty()) {
        return 0;
    }

    std::string key{name};

    if (auto it = lookup.find(key); it != lookup.end()) {
        return it->second;
    }

    qhandle_t handle = nextHandle();
    lookup.emplace(std::move(key), handle);
    return handle;
}

void VulkanRenderer::resetTransientState() {
    clipRect_.reset();
    scale_ = 1.0f;
    autoScaleValue_ = 1;
}

bool VulkanRenderer::init(bool total) {
    if (!total) {
        frameActive_ = false;
        return true;
    }

    if (initialized_) {
        return true;
    }

    Com_Printf("------- refresh-vk init -------\n");

    VideoGeometry geometry = queryVideoGeometry();
    applyVideoGeometry(geometry);
    Com_Printf("Using video geometry %dx%d%s\n", r_config.width, r_config.height,
               (r_config.flags & QVF_FULLSCREEN) ? " (fullscreen)" : "");

    resetTransientState();

    models_.clear();
    images_.clear();
    modelLookup_.clear();
    imageLookup_.clear();
    rawPic_ = {};

    initialized_ = true;
    r_registration_sequence = 1;

    Com_Printf("refresh-vk initialized (placeholder implementation).\n");
    Com_Printf("------------------------------\n");

    return true;
}

void VulkanRenderer::shutdown(bool total) {
    if (!initialized_) {
        return;
    }

    if (frameActive_) {
        endFrame();
    }

    if (total) {
        models_.clear();
        images_.clear();
        modelLookup_.clear();
        imageLookup_.clear();
        rawPic_.pixels.clear();
        currentMap_.clear();
        resetTransientState();
        initialized_ = false;
    }
}

void VulkanRenderer::beginRegistration(const char *map) {
    if (map) {
        currentMap_ = map;
    } else {
        currentMap_.clear();
    }

    ++r_registration_sequence;
}

qhandle_t VulkanRenderer::registerModel(const char *name) {
    if (!name || !*name) {
        return 0;
    }

    qhandle_t handle = registerResource(modelLookup_, name);
    auto &record = models_[handle];
    record.handle = handle;
    record.name = name;
    record.registrationSequence = r_registration_sequence;

    return handle;
}

qhandle_t VulkanRenderer::registerImage(const char *name, imagetype_t type, imageflags_t flags) {
    if (!name || !*name) {
        return 0;
    }

    qhandle_t handle = registerResource(imageLookup_, name);
    auto &record = images_[handle];
    record.handle = handle;
    record.name = name;
    record.type = type;
    record.flags = flags;
    record.registrationSequence = r_registration_sequence;
    record.transparent = (flags & IF_TRANSPARENT) != 0;

    if (flags & IF_SPECIAL) {
        record.width = 1;
        record.height = 1;
    }

    return handle;
}

void VulkanRenderer::setSky(const char *name, float rotate, bool autorotate, const vec3_t axis) {
    sky_.name = name ? name : "";
    sky_.rotate = rotate;
    sky_.autorotate = autorotate;
    if (axis) {
        std::copy_n(axis, sky_.axis.size(), sky_.axis.begin());
    }
}

void VulkanRenderer::endRegistration() {
    auto predicate = [](auto &pair) {
        return pair.second.registrationSequence != r_registration_sequence;
    };

    std::erase_if(models_, predicate);
    std::erase_if(images_, predicate);
}

void VulkanRenderer::beginFrame() {
    if (!initialized_) {
        return;
    }

    if (vid && vid->pump_events) {
        vid->pump_events();
    }

    frameActive_ = true;
}

void VulkanRenderer::endFrame() {
    if (!initialized_ || !frameActive_) {
        return;
    }

    if (vid && vid->swap_buffers) {
        vid->swap_buffers();
    }

    frameActive_ = false;
}

void VulkanRenderer::renderFrame(const refdef_t *fd) {
    if (!initialized_ || !fd) {
        return;
    }

    // Placeholder implementation: update auto-scale using viewport height.
    if (fd->height > 0) {
        autoScaleValue_ = std::max(1, fd->height / SCREEN_HEIGHT);
    }
}

void VulkanRenderer::lightPoint(const vec3_t origin, vec3_t light) const {
    if (!light) {
        return;
    }

    light[0] = 0.0f;
    light[1] = 0.0f;
    light[2] = 0.0f;
}

void VulkanRenderer::setClipRect(const clipRect_t *clip) {
    if (clip) {
        clipRect_ = *clip;
    } else {
        clipRect_.reset();
    }
}

float VulkanRenderer::clampScale(cvar_t *var) const {
    if (!var) {
        return 1.0f;
    }

    float value = var->value;
    if (!std::isfinite(value)) {
        value = 1.0f;
    }

    constexpr float kMinScale = 0.25f;
    constexpr float kMaxScale = 4.0f;
    return std::clamp(value, kMinScale, kMaxScale);
}

void VulkanRenderer::setScale(float scale) {
    if (!std::isfinite(scale)) {
        scale_ = 1.0f;
    } else {
        scale_ = std::clamp(scale, 0.25f, 4.0f);
    }
}

int VulkanRenderer::autoScale() const {
    return autoScaleValue_;
}

void VulkanRenderer::drawChar(int x, int y, int flags, int ch, color_t color, qhandle_t font) {
    drawStretchChar(x, y, kDefaultCharWidth, kDefaultCharHeight, flags, ch, color, font);
}

void VulkanRenderer::drawStretchChar(int, int, int, int, int, int, color_t, qhandle_t) {
    // Intentionally left blank – drawing is handled by Vulkan backend in the future.
}

int VulkanRenderer::drawStringStretch(int x, int, int scale, int, size_t maxChars,
                                      const char *string, color_t, qhandle_t) {
    if (!string || !*string) {
        return x;
    }

    std::string_view view{string};
    int printable = countPrintable(view, maxChars);
    int charWidth = std::max(1, scale) * kDefaultCharWidth;
    return x + printable * charWidth;
}

int VulkanRenderer::drawKFontChar(int x, int, int scale, int, uint32_t codepoint,
                                  color_t, const kfont_t *kfont) {
    if (!kfont) {
        return x;
    }

    const kfont_char_t *metrics = lookupKFontChar(kfont, codepoint);
    if (!metrics) {
        return x;
    }

    int advance = std::max(1, scale) * static_cast<int>(metrics->w);
    return x + advance;
}

bool VulkanRenderer::getPicSize(int *w, int *h, qhandle_t pic) const {
    auto it = images_.find(pic);
    if (it == images_.end()) {
        return false;
    }

    if (w) {
        *w = it->second.width;
    }
    if (h) {
        *h = it->second.height;
    }
    return it->second.transparent;
}

void VulkanRenderer::drawPic(int, int, color_t, qhandle_t) {
}

void VulkanRenderer::drawStretchPic(int, int, int, int, color_t, qhandle_t) {
}

void VulkanRenderer::drawStretchRotatePic(int, int, int, int, color_t, float, int, int, qhandle_t) {
}

void VulkanRenderer::drawKeepAspectPic(int, int, int, int, color_t, qhandle_t) {
}

void VulkanRenderer::drawStretchRaw(int, int, int, int) {
}

void VulkanRenderer::updateRawPic(int pic_w, int pic_h, const uint32_t *pic) {
    if (pic_w <= 0 || pic_h <= 0 || !pic) {
        rawPic_ = {};
        return;
    }

    rawPic_.width = pic_w;
    rawPic_.height = pic_h;
    rawPic_.pixels.assign(pic, pic + (static_cast<size_t>(pic_w) * static_cast<size_t>(pic_h)));
}

void VulkanRenderer::tileClear(int, int, int, int, qhandle_t) {
}

void VulkanRenderer::drawFill8(int, int, int, int, int) {
}

void VulkanRenderer::drawFill32(int, int, int, int, color_t) {
}

void VulkanRenderer::modeChanged(int width, int height, int flags) {
    VideoGeometry geometry{};
    geometry.width = std::max(1, width);
    geometry.height = std::max(1, height);
    geometry.flags = static_cast<vidFlags_t>(flags);
    applyVideoGeometry(geometry);
}

bool VulkanRenderer::videoSync() const {
    return false;
}

void VulkanRenderer::expireDebugObjects() {
}

bool VulkanRenderer::supportsPerPixelLighting() const {
    return false;
}

r_opengl_config_t VulkanRenderer::getGLConfig() const {
    return {};
}

void VulkanRenderer::loadKFont(kfont_t *font, const char *filename) {
    if (!font) {
        return;
    }

    font->pic = registerImage(filename && *filename ? filename : "_kfont", IT_FONT, IF_PERMANENT);

    uint16_t x = 0;
    uint16_t y = 0;
    for (auto &glyph : font->chars) {
        glyph = { x, y, defaultKFontWidth(), defaultKFontHeight() };
        x = static_cast<uint16_t>(x + glyph.w);
    }

    font->line_height = defaultKFontHeight();
    font->sw = 1.0f;
    font->sh = 1.0f;
}

const kfont_char_t *VulkanRenderer::lookupKFontChar(const kfont_t *kfont, uint32_t codepoint) const {
    if (!kfont) {
        return nullptr;
    }

    if (codepoint < KFONT_ASCII_MIN || codepoint > KFONT_ASCII_MAX) {
        return nullptr;
    }

    size_t index = static_cast<size_t>(codepoint - KFONT_ASCII_MIN);
    return &kfont->chars[index];
}

} // namespace refresh::vk
