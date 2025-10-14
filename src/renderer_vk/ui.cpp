#include "renderer.h"

#include "vk_draw2d.h"

#include "renderer/common.h"
#include "renderer/images.h"
#include "renderer/kfont.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include "client/client.h"

namespace refresh::vk {

namespace {

uint8_t colorBitsForFormat(VkFormat format) {
    switch (format) {
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_SRGB:
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SRGB:
        return 24;
    case VK_FORMAT_B8G8R8_UNORM:
    case VK_FORMAT_R8G8B8_UNORM:
        return 24;
    case VK_FORMAT_B5G6R5_UNORM_PACK16:
    case VK_FORMAT_R5G6B5_UNORM_PACK16:
        return 16;
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
    case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
        return 30;
    default:
        return 24;
    }
}

constexpr int kDefaultCharWidth = 8;
constexpr int kDefaultCharHeight = 8;
constexpr float kTileDivisor = 1.0f / 64.0f;
constexpr float kFontCellSize = 1.0f / 16.0f;
constexpr float kShadowOffset = 1.0f;
constexpr int kUIDropShadow = 1 << 4;
constexpr int kUIAltColor = 1 << 5;
constexpr int kUIXorColor = 1 << 7;

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

std::array<std::array<float, 2>, 4> makeQuad(float x, float y, float w, float h) {
    return {{{
        {x, y},
        {x + w, y},
        {x + w, y + h},
        {x, y + h},
    }}};
}

std::array<std::array<float, 2>, 4> makeUV(float u0, float v0, float u1, float v1) {
    return {{{
        {u0, v0},
        {u1, v0},
        {u1, v1},
        {u0, v1},
    }}};
}

const std::array<std::array<float, 2>, 4> kFullUVs = makeUV(0.0f, 0.0f, 1.0f, 1.0f);

std::array<std::array<float, 2>, 4> makeQuadPositions(float x0, float y0, float x1, float y1) {
    return {{{ { x0, y0 }, { x1, y0 }, { x1, y1 }, { x0, y1 } }}};
}

constexpr size_t kRawPicBytesPerPixel = 4;

inline uint8_t clampToByte(int value) {
    return static_cast<uint8_t>(std::clamp(value, 0, 255));
}

bool convertRGBAPlane(const rawPicUpload_t &upload, std::vector<uint8_t> &out) {
    if (upload.planeCount < 1 || !upload.planes[0]) {
        return false;
    }

    const size_t width = static_cast<size_t>(std::max(0, upload.width));
    const size_t height = static_cast<size_t>(std::max(0, upload.height));
    if (width == 0 || height == 0) {
        return false;
    }

    const size_t expectedStride = width * kRawPicBytesPerPixel;
    const size_t stride = upload.strides[0];
    if (stride < expectedStride) {
        return false;
    }

    out.resize(width * height * kRawPicBytesPerPixel);
    const uint8_t *source = upload.planes[0];
    uint8_t *dest = out.data();

    for (size_t y = 0; y < height; ++y) {
        std::memcpy(dest + y * expectedStride, source + y * stride, expectedStride);
    }

    return true;
}

bool convertYUV420ToRGBA(const rawPicUpload_t &upload, std::vector<uint8_t> &out) {
    if (upload.planeCount < 3 || !upload.planes[0] || !upload.planes[1] || !upload.planes[2]) {
        return false;
    }

    const int width = std::max(0, upload.width);
    const int height = std::max(0, upload.height);
    if (width <= 0 || height <= 0) {
        return false;
    }

    out.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * kRawPicBytesPerPixel);

    const uint8_t *yPlane = upload.planes[0];
    const uint8_t *uPlane = upload.planes[1];
    const uint8_t *vPlane = upload.planes[2];
    const size_t yStride = upload.strides[0];
    const size_t uStride = upload.strides[1];
    const size_t vStride = upload.strides[2];

    for (int y = 0; y < height; ++y) {
        const uint8_t *yRow = yPlane + static_cast<size_t>(y) * yStride;
        const uint8_t *uRow = uPlane + static_cast<size_t>(y / 2) * uStride;
        const uint8_t *vRow = vPlane + static_cast<size_t>(y / 2) * vStride;
        uint8_t *dst = out.data() + static_cast<size_t>(y) * static_cast<size_t>(width) * kRawPicBytesPerPixel;

        for (int x = 0; x < width; ++x) {
            const int ySample = static_cast<int>(yRow[x]);
            const int uSample = static_cast<int>(uRow[x / 2]);
            const int vSample = static_cast<int>(vRow[x / 2]);

            int c = ySample - 16;
            int d = uSample - 128;
            int e = vSample - 128;
            if (c < 0) {
                c = 0;
            }

            const int r = (298 * c + 409 * e + 128) >> 8;
            const int g = (298 * c - 100 * d - 208 * e + 128) >> 8;
            const int b = (298 * c + 516 * d + 128) >> 8;

            dst[0] = clampToByte(r);
            dst[1] = clampToByte(g);
            dst[2] = clampToByte(b);
            dst[3] = 255;
            dst += kRawPicBytesPerPixel;
        }
    }

    return true;
}

bool convertNV12ToRGBA(const rawPicUpload_t &upload, std::vector<uint8_t> &out) {
    if (upload.planeCount < 2 || !upload.planes[0] || !upload.planes[1]) {
        return false;
    }

    const int width = std::max(0, upload.width);
    const int height = std::max(0, upload.height);
    if (width <= 0 || height <= 0) {
        return false;
    }

    out.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * kRawPicBytesPerPixel);

    const uint8_t *yPlane = upload.planes[0];
    const uint8_t *uvPlane = upload.planes[1];
    const size_t yStride = upload.strides[0];
    const size_t uvStride = upload.strides[1];

    for (int y = 0; y < height; ++y) {
        const uint8_t *yRow = yPlane + static_cast<size_t>(y) * yStride;
        const uint8_t *uvRow = uvPlane + static_cast<size_t>(y / 2) * uvStride;
        uint8_t *dst = out.data() + static_cast<size_t>(y) * static_cast<size_t>(width) * kRawPicBytesPerPixel;

        for (int x = 0; x < width; ++x) {
            const int uvIndex = (x / 2) * 2;
            const int ySample = static_cast<int>(yRow[x]);
            const int uSample = static_cast<int>(uvRow[uvIndex]);
            const int vSample = static_cast<int>(uvRow[uvIndex + 1]);

            int c = ySample - 16;
            int d = uSample - 128;
            int e = vSample - 128;
            if (c < 0) {
                c = 0;
            }

            const int r = (298 * c + 409 * e + 128) >> 8;
            const int g = (298 * c - 100 * d - 208 * e + 128) >> 8;
            const int b = (298 * c + 516 * d + 128) >> 8;

            dst[0] = clampToByte(r);
            dst[1] = clampToByte(g);
            dst[2] = clampToByte(b);
            dst[3] = 255;
            dst += kRawPicBytesPerPixel;
        }
    }

    return true;
}

bool convertRawPicToRGBA(const rawPicUpload_t &upload, std::vector<uint8_t> &out) {
    switch (upload.format) {
    case RAW_PIC_FORMAT_RGBA8:
        return convertRGBAPlane(upload, out);
    case RAW_PIC_FORMAT_YUV420P:
        return convertYUV420ToRGBA(upload, out);
    case RAW_PIC_FORMAT_NV12:
        return convertNV12ToRGBA(upload, out);
    default:
        return false;
    }
}

} // namespace

extern uint32_t d_8to24table[256];

VulkanRenderer::VideoGeometry VulkanRenderer::queryVideoGeometry() const {
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

void VulkanRenderer::applyVideoGeometry(const VideoGeometry &geometry) {
    r_config.width = geometry.width;
    r_config.height = geometry.height;
    r_config.flags = geometry.flags;
}

void VulkanRenderer::resetTransientState() {
    clipRect_.reset();
    activeScissor_.reset();
    scale_ = 1.0f;
    autoScaleValue_ = 1;
    clear2DBatches();
    resetFrameState();
    lastSubmittedFrame_.reset();
}

int VulkanRenderer::computeAutoScale() const {
    int (*dpiScaleFn)() = nullptr;
    if (vid && vid->get_dpi_scale) {
        dpiScaleFn = vid->get_dpi_scale;
    }

    return Renderer_ComputeAutoScale(r_config, dpiScaleFn);
}

void VulkanRenderer::updateUIScaling() {
    autoScaleValue_ = std::max(1, computeAutoScale());

    if (!std::isfinite(scale_) || scale_ <= 0.0f) {
        scale_ = 1.0f;
    }

    // Re-apply the current scale to refresh dependent state such as clip rectangles.
    setScale(scale_);
}












const VulkanRenderer::ImageRecord *VulkanRenderer::findImageRecord(qhandle_t handle) const {
    if (auto it = images_.find(handle); it != images_.end()) {
        return &it->second;
    }
    return nullptr;
}

const VulkanRenderer::KFontRecord *VulkanRenderer::findKFontRecord(const kfont_t *font) const {
    if (!font) {
        return nullptr;
    }

    if (auto it = kfontCache_.find(font); it != kfontCache_.end()) {
        return &it->second;
    }
    return nullptr;
}






void VulkanRenderer::destroy2DBatch(Draw2DBatch &batch) {
    if (device_ != VK_NULL_HANDLE && descriptorPool_ != VK_NULL_HANDLE && batch.descriptor.set != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(device_, descriptorPool_, 1, &batch.descriptor.set);
    }
    batch.descriptor.set = VK_NULL_HANDLE;
    batch.descriptor.vertex = {};
    batch.descriptor.index = {};

    destroyBuffer(batch.vertex);
    destroyBuffer(batch.index);

    batch.vertexCount = 0;
    batch.indexCount = 0;
    batch.texture = 0;
}

void VulkanRenderer::clear2DBatches() {
    for (auto &batch : frame2DBatches_) {
        destroy2DBatch(batch);
    }
    frame2DBatches_.clear();
}


















bool VulkanRenderer::canSubmit2D() const {
    return frameActive_ && draw2d::isActive();
}

qhandle_t VulkanRenderer::ensureWhiteTexture() {
    if (whiteTextureHandle_ != 0) {
        if (auto it = images_.find(whiteTextureHandle_); it != images_.end() && it->second.image != VK_NULL_HANDLE) {
            return whiteTextureHandle_;
        }
        whiteTextureHandle_ = 0;
    }

    qhandle_t handle = registerResource(imageLookup_, "__vk_white");
    whiteTextureHandle_ = handle;

    ImageRecord record{};
    record.handle = handle;
    record.name = "__vk_white";
    record.type = IT_PIC;
    record.flags = static_cast<imageflags_t>(IF_SPECIAL | IF_PERMANENT);
    record.width = 1;
    record.height = 1;
    record.uploadWidth = 1;
    record.uploadHeight = 1;
    record.transparent = false;
    record.registrationSequence = r_registration_sequence;

    const uint8_t whitePixel[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
    if (!ensureTextureResources(record, whitePixel, sizeof(whitePixel), 1, 1, VK_FORMAT_R8G8B8A8_UNORM)) {
        imageLookup_.erase(record.name);
        whiteTextureHandle_ = 0;
        return 0;
    }

    images_[handle] = record;
    imageLookup_[record.name] = handle;
    return handle;
}

qhandle_t VulkanRenderer::ensureRawTexture() {
    if (rawTextureHandle_ != 0) {
        if (auto it = images_.find(rawTextureHandle_); it != images_.end()) {
            ImageRecord &record = it->second;
            record.width = rawPic_.width;
            record.height = rawPic_.height;
            record.uploadWidth = rawPic_.width;
            record.uploadHeight = rawPic_.height;
            record.registrationSequence = r_registration_sequence;

            if (!rawPic_.pixels.empty()) {
                if (!uploadRawTexture(record, rawPic_)) {
                    destroyImageRecord(record);
                    rawTextureHandle_ = 0;
                    return 0;
                }
            }
            return rawTextureHandle_;
        }
        rawTextureHandle_ = 0;
    }

    if (rawPic_.pixels.empty() || rawPic_.width <= 0 || rawPic_.height <= 0) {
        return 0;
    }

    qhandle_t handle = registerResource(imageLookup_, "__vk_raw");
    rawTextureHandle_ = handle;

    ImageRecord record{};
    record.handle = handle;
    record.name = "__vk_raw";
    record.type = IT_PIC;
    record.flags = IF_SPECIAL;
    record.width = rawPic_.width;
    record.height = rawPic_.height;
    record.uploadWidth = rawPic_.width;
    record.uploadHeight = rawPic_.height;
    record.transparent = true;
    record.registrationSequence = r_registration_sequence;

    if (!uploadRawTexture(record, rawPic_)) {
        imageLookup_.erase(record.name);
        rawTextureHandle_ = 0;
        return 0;
    }

    images_[handle] = record;
    imageLookup_[record.name] = handle;
    return handle;
}











































VulkanRenderer::ScissorRect VulkanRenderer::fullScissorRect() const {
    ScissorRect rect{};
    rect.x = 0;
    rect.y = 0;

    auto computeExtent = [](int dimension, double scale) -> uint32_t {
        if (dimension <= 0 || scale <= 0.0) {
            return 0u;
        }
        double scaled = static_cast<double>(dimension) * scale;
        long long rounded = std::llround(scaled);
        if (rounded <= 0) {
            return 0u;
        }
        return static_cast<uint32_t>(rounded);
    };

    double scale = static_cast<double>(std::max(scale_, 0.0f));
    rect.width = computeExtent(r_config.width, scale);
    rect.height = computeExtent(r_config.height, scale);
    return rect;
}

std::optional<VulkanRenderer::ScissorRect> VulkanRenderer::scaledClipRect(const clipRect_t &clip) const {
    ScissorRect bounds = fullScissorRect();
    if (bounds.width == 0 || bounds.height == 0) {
        return std::nullopt;
    }

    double scale = static_cast<double>(std::max(scale_, 0.0f));
    if (scale <= 0.0) {
        return std::nullopt;
    }

    double left = static_cast<double>(clip.left) * scale;
    double top = static_cast<double>(clip.top) * scale;
    double right = static_cast<double>(clip.right) * scale;
    double bottom = static_cast<double>(clip.bottom) * scale;

    int32_t x0 = static_cast<int32_t>(std::floor(left));
    int32_t y0 = static_cast<int32_t>(std::floor(top));
    int32_t x1 = static_cast<int32_t>(std::ceil(right));
    int32_t y1 = static_cast<int32_t>(std::ceil(bottom));

    int32_t maxWidth = static_cast<int32_t>(bounds.width);
    int32_t maxHeight = static_cast<int32_t>(bounds.height);

    x0 = std::clamp(x0, 0, maxWidth);
    y0 = std::clamp(y0, 0, maxHeight);
    x1 = std::clamp(x1, 0, maxWidth);
    y1 = std::clamp(y1, 0, maxHeight);

    if (x1 <= x0 || y1 <= y0) {
        return std::nullopt;
    }

    ScissorRect rect{};
    rect.x = x0;
    rect.y = y0;
    rect.width = static_cast<uint32_t>(x1 - x0);
    rect.height = static_cast<uint32_t>(y1 - y0);
    return rect;
}

void VulkanRenderer::recordScissorCommand(const ScissorRect &rect, bool clipped) {
    std::string entry = "vkCmdSetScissor offset=(";
    entry.append(std::to_string(rect.x));
    entry.push_back(',');
    entry.append(std::to_string(rect.y));
    entry.append(") extent=(");
    entry.append(std::to_string(rect.width));
    entry.push_back(',');
    entry.append(std::to_string(rect.height));
    entry.push_back(')');
    if (!clipped) {
        entry.append(" (full)");
    }
    commandLog_.push_back(std::move(entry));
}

void VulkanRenderer::setClipRect(const clipRect_t *clip) {
    bool changed = false;
    if (clip) {
        if (!clipRect_ || clipRect_->left != clip->left || clipRect_->right != clip->right ||
            clipRect_->top != clip->top || clipRect_->bottom != clip->bottom) {
            changed = true;
        }
    } else if (clipRect_) {
        changed = true;
    }

    if (changed && draw2d::isActive()) {
        draw2d::flush();
    }

    if (clip) {
        clipRect_ = *clip;
    } else {
        clipRect_.reset();
    }

    ScissorRect fullRect = fullScissorRect();
    std::optional<ScissorRect> desired = clip ? scaledClipRect(*clip) : std::nullopt;
    bool drawingActive = frameActive_ && draw2d::isActive();

    if (!desired) {
        if (!activeScissor_) {
            return;
        }
        if (drawingActive) {
            draw2d::flush();
            recordScissorCommand(fullRect, false);
        }
        activeScissor_.reset();
        return;
    }

    if (activeScissor_ && *activeScissor_ == *desired) {
        return;
    }

    if (drawingActive) {
        draw2d::flush();
        recordScissorCommand(*desired, true);
    }

    activeScissor_ = desired;
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
    float target = 1.0f;
    if (std::isfinite(scale)) {
        target = std::clamp(scale, 0.25f, 4.0f);
    }

    if (std::abs(target - scale_) <= std::numeric_limits<float>::epsilon()) {
        return;
    }

    bool drawingActive = frameActive_ && draw2d::isActive();
    if (drawingActive) {
        draw2d::flush();
    }

    scale_ = target;
    activeScissor_.reset();

    if (clipRect_) {
        setClipRect(&*clipRect_);
    } else if (drawingActive) {
        setClipRect(nullptr);
    }
}

int VulkanRenderer::autoScale() const {
    return autoScaleValue_;
}

void VulkanRenderer::drawChar(int x, int y, int flags, int ch, color_t color, qhandle_t font) {
    drawStretchChar(x, y, kDefaultCharWidth, kDefaultCharHeight, flags, ch, color, font);
}

void VulkanRenderer::drawStretchChar(int x, int y, int w, int h, int flags, int ch, color_t color, qhandle_t font) {
    if (!frameActive_ || !draw2d::isActive()) {
        return;
    }

    if (!canSubmit2D()) {
        return;
    }

    if (w <= 0 || h <= 0) {
        return;
    }

    if (!findImageRecord(font)) {
        return;
    }

    float scale = scale_;
    if (scale <= 0.0f) {
        return;
    }

    AtlasGlyphParams params{};
    params.x = x;
    params.y = y;
    params.width = w;
    params.height = h;
    params.flags = flags;
    params.glyph = static_cast<uint8_t>(ch);
    params.color = color;

    GlyphDrawData glyphData = Renderer_BuildAtlasGlyph(params);
    if (!glyphData.visible) {
        return;
    }

    auto submitGlyph = [&](const GlyphQuad &quad) {
        auto positions = makeQuad(quad.x * scale, quad.y * scale, quad.w * scale, quad.h * scale);
        auto uvs = makeUV(quad.s0, quad.t0, quad.s1, quad.t1);
        draw2d::submitQuad(positions, uvs, quad.color.u32, font);
    };

    for (int i = 0; i < glyphData.shadowCount; ++i) {
        submitGlyph(glyphData.shadows[i]);
    }

    submitGlyph(glyphData.primary);
}

int VulkanRenderer::drawStringStretch(int x, int y, int scale, int flags, size_t maxChars,
                                      const char *string, color_t color, qhandle_t font) {
    if (!string || !*string) {
        return x;
    }

    if (!canSubmit2D()) {
        return x;
    }

    int effectiveScale = std::max(1, scale);
    int charWidth = effectiveScale * kDefaultCharWidth;
    int charHeight = effectiveScale * kDefaultCharHeight;
    int cursorX = x;
    int cursorY = y;
    int lineStart = x;

    size_t remaining = maxChars ? maxChars : std::numeric_limits<size_t>::max();
    size_t processed = 0;
    while (processed < remaining && string[processed]) {
        unsigned char ch = static_cast<unsigned char>(string[processed]);
        ++processed;

        if ((flags & UI_MULTILINE) && ch == '\n') {
            cursorY += charHeight;
            cursorX = lineStart;
            continue;
        }

        drawStretchChar(cursorX, cursorY, charWidth, charHeight, flags, ch, color, font);
        cursorX += charWidth;
    }

    return cursorX;
}

int VulkanRenderer::drawKFontChar(int x, int y, int scale, int flags, uint32_t codepoint,
                                  color_t color, const kfont_t *kfont) {
    if (!kfont) {
        return x;
    }

    const kfont_char_t *metrics = lookupKFontChar(kfont, codepoint);
    if (!metrics) {
        return x;
    }

    if (!canSubmit2D()) {
        return x;
    }

    int effectiveScale = std::max(1, scale);
    int w = static_cast<int>(metrics->w) * effectiveScale;
    int h = static_cast<int>(metrics->h) * effectiveScale;
    if (w <= 0 || h <= 0) {
        return x;
    }

    const KFontRecord *record = findKFontRecord(kfont);
    qhandle_t texture = record ? record->texture : kfont->pic;
    if (!findImageRecord(texture)) {
        return x;
    }

    float scaleFactor = scale_;
    if (scaleFactor <= 0.0f) {
        return x;
    }

    float sw = record ? record->sw : kfont->sw;
    float sh = record ? record->sh : kfont->sh;

    KFontGlyphParams params{};
    params.x = x;
    params.y = y;
    params.scale = effectiveScale;
    params.flags = flags;
    params.color = color;
    params.metrics = metrics;
    params.sw = sw;
    params.sh = sh;

    GlyphDrawData glyphData = Renderer_BuildKFontGlyph(params);
    if (!glyphData.visible) {
        return x;
    }

    auto submitGlyph = [&](const GlyphQuad &quad) {
        auto positions = makeQuad(quad.x * scaleFactor, quad.y * scaleFactor,
                                  quad.w * scaleFactor, quad.h * scaleFactor);
        auto uvs = makeUV(quad.s0, quad.t0, quad.s1, quad.t1);
        draw2d::submitQuad(positions, uvs, quad.color.u32, texture);
    };

    for (int i = 0; i < glyphData.shadowCount; ++i) {
        submitGlyph(glyphData.shadows[i]);
    }

    submitGlyph(glyphData.primary);
    return x + w;
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

void VulkanRenderer::drawPic(int x, int y, color_t color, qhandle_t pic) {
    auto it = images_.find(pic);
    if (it == images_.end()) {
        return;
    }

    if (it->second.width <= 0 || it->second.height <= 0) {
        return;
    }

    drawStretchPic(x, y, it->second.width, it->second.height, color, pic);
}

void VulkanRenderer::drawStretchPic(int x, int y, int w, int h, color_t color, qhandle_t pic) {
    if (!canSubmit2D()) {
        return;
    }

    if (w <= 0 || h <= 0) {
        return;
    }

    if (!findImageRecord(pic)) {
        return;
    }

    float scale = scale_;
    if (scale <= 0.0f) {
        return;
    }

    float fx = static_cast<float>(x) * scale;
    float fy = static_cast<float>(y) * scale;
    float fw = static_cast<float>(w) * scale;
    float fh = static_cast<float>(h) * scale;

    auto positions = makeQuad(fx, fy, fw, fh);
    draw2d::submitQuad(positions, kFullUVs, color.u32, pic);
}

void VulkanRenderer::drawStretchRotatePic(int x, int y, int w, int h, color_t color, float angle, int pivot_x, int pivot_y, qhandle_t pic) {
    if (!canSubmit2D()) {
        return;
    }

    if (!frameActive_ || !draw2d::isActive()) {
        return;
    }

    if (w <= 0 || h <= 0) {
        return;
    }

    if (!findImageRecord(pic)) {
        return;
    }

    float scale = scale_;
    if (scale <= 0.0f) {
        return;
    }

    std::array<std::array<float, 2>, 4> positions = makeQuadPositions(static_cast<float>(x),
                                                                       static_cast<float>(y),
                                                                       static_cast<float>(x + w),
                                                                       static_cast<float>(y + h));

    float originX = static_cast<float>(x + pivot_x);
    float originY = static_cast<float>(y + pivot_y);
    float radians = DEG2RAD(angle);
    float sine = std::sin(radians);
    float cosine = std::cos(radians);

    for (auto &pos : positions) {
        float dx = pos[0] - originX;
        float dy = pos[1] - originY;
        float rotatedX = originX + dx * cosine - dy * sine;
        float rotatedY = originY + dx * sine + dy * cosine;
        pos[0] = rotatedX * scale;
        pos[1] = rotatedY * scale;
    }

    draw2d::submitQuad(positions, kFullUVs, color.u32, pic);
}

void VulkanRenderer::drawKeepAspectPic(int x, int y, int w, int h, color_t color, qhandle_t pic) {
    if (!frameActive_ || !draw2d::isActive()) {
        return;
    }

    if (!canSubmit2D()) {
        return;
    }

    if (w <= 0 || h <= 0) {
        return;
    }

    const ImageRecord *image = findImageRecord(pic);
    if (!image) {
        return;
    }

    if (image->flags & IF_SCRAP) {
        drawStretchPic(x, y, w, h, color, pic);
        return;
    }

    float imageWidth = static_cast<float>(std::max(1, image->width));
    float imageHeight = static_cast<float>(std::max(1, image->height));
    float aspect = imageWidth / imageHeight;

    rUvWindow_t uv;
    if (!R_ComputeKeepAspectUVWindow(w, h, aspect, &uv)) {
        drawStretchPic(x, y, w, h, color, pic);
        return;
    }

    float scaleFactor = scale_;
    if (scaleFactor <= 0.0f) {
        return;
    }

    float fx = static_cast<float>(x) * scaleFactor;
    float fy = static_cast<float>(y) * scaleFactor;
    float fw = static_cast<float>(w) * scaleFactor;
    float fh = static_cast<float>(h) * scaleFactor;

    auto positions = makeQuad(fx, fy, fw, fh);
    auto uvs = makeUV(uv.s0, uv.t0, uv.s1, uv.t1);
    draw2d::submitQuad(positions, uvs, color.u32, pic);
}

void VulkanRenderer::drawStretchRaw(int x, int y, int w, int h) {
    if (!canSubmit2D()) {
        return;
    }

    if (w <= 0 || h <= 0) {
        return;
    }

    if (rawPic_.width <= 0 || rawPic_.height <= 0 || rawPic_.pixels.empty()) {
        return;
    }

    qhandle_t texture = ensureRawTexture();
    if (!texture || !findImageRecord(texture)) {
        return;
    }

    float scale = scale_;
    if (scale <= 0.0f) {
        return;
    }

    float fx = static_cast<float>(x) * scale;
    float fy = static_cast<float>(y) * scale;
    float fw = static_cast<float>(w) * scale;
    float fh = static_cast<float>(h) * scale;

    auto positions = makeQuad(fx, fy, fw, fh);
    draw2d::submitQuad(positions, kFullUVs, COLOR_WHITE.u32, texture);
}

void VulkanRenderer::updateRawPic(const rawPicUpload_t *pic) {
    if (draw2d::isActive()) {
        draw2d::flush();
    }

    if (!pic || pic->width <= 0 || pic->height <= 0 || pic->planeCount <= 0) {
        rawPic_ = {};
        if (auto it = images_.find(rawTextureHandle_); it != images_.end()) {
            destroyImageRecord(it->second);
        }
        rawTextureHandle_ = 0;
        return;
    }

    rawPic_.width = pic->width;
    rawPic_.height = pic->height;
    rawPic_.sourceFormat = pic->format;

    std::vector<uint8_t> converted;
    if (!convertRawPicToRGBA(*pic, converted)) {
        Com_Printf("refresh-vk: unsupported raw picture format %d.\n", static_cast<int>(pic->format));
        rawPic_ = {};
        if (auto it = images_.find(rawTextureHandle_); it != images_.end()) {
            destroyImageRecord(it->second);
        }
        rawTextureHandle_ = 0;
        return;
    }

    rawPic_.pixels = std::move(converted);

    if (rawTextureHandle_ != 0) {
        if (auto it = images_.find(rawTextureHandle_); it != images_.end()) {
            ImageRecord &record = it->second;
            record.width = rawPic_.width;
            record.height = rawPic_.height;
            record.uploadWidth = rawPic_.width;
            record.uploadHeight = rawPic_.height;
            record.registrationSequence = r_registration_sequence;

            if (uploadRawTexture(record, rawPic_)) {
                return;
            }

            destroyImageRecord(record);
        }
        rawTextureHandle_ = 0;
    }

    ensureRawTexture();
}

void VulkanRenderer::tileClear(int x, int y, int w, int h, qhandle_t pic) {
    if (!frameActive_ || !draw2d::isActive()) {
        return;
    }

    if (!canSubmit2D()) {
        return;
    }

    if (w <= 0 || h <= 0) {
        return;
    }

    if (!findImageRecord(pic)) {
        return;
    }

    float scale = scale_;
    if (scale <= 0.0f) {
        return;
    }

    float fx = static_cast<float>(x) * scale;
    float fy = static_cast<float>(y) * scale;
    float fw = static_cast<float>(w) * scale;
    float fh = static_cast<float>(h) * scale;

    auto positions = makeQuad(fx, fy, fw, fh);
    const rUvWindow_t uv = R_ComputeTileUVWindow(x, y, w, h);
    auto uvs = makeUV(uv.s0, uv.t0, uv.s1, uv.t1);
    draw2d::submitQuad(positions, uvs, COLOR_WHITE.u32, pic);
}

void VulkanRenderer::drawFill8(int x, int y, int w, int h, int c) {
    if (!frameActive_ || !draw2d::isActive()) {
        return;
    }
    if (!canSubmit2D()) {
        return;
    }

    if (w <= 0 || h <= 0) {
        return;
    }

    qhandle_t texture = ensureWhiteTexture();
    if (!texture || !findImageRecord(texture)) {
        return;
    }

    float scale = scale_;
    if (scale <= 0.0f) {
        return;
    }

    float fx = static_cast<float>(x) * scale;
    float fy = static_cast<float>(y) * scale;
    float fw = static_cast<float>(w) * scale;
    float fh = static_cast<float>(h) * scale;

    color_t tint = ColorU32(d_8to24table[c & 0xFF]);
    auto positions = makeQuad(fx, fy, fw, fh);
    draw2d::submitQuad(positions, kFullUVs, tint.u32, texture);
}

void VulkanRenderer::drawFill32(int x, int y, int w, int h, color_t color) {
    if (!frameActive_ || !draw2d::isActive()) {
        return;
    }
  
    if (!canSubmit2D()) {
        return;
    }

    if (w <= 0 || h <= 0) {
        return;
    }

    qhandle_t texture = ensureWhiteTexture();
    if (!texture || !findImageRecord(texture)) {
        return;
    }

    float scale = scale_;
    if (scale <= 0.0f) {
        return;
    }

    float fx = static_cast<float>(x) * scale;
    float fy = static_cast<float>(y) * scale;
    float fw = static_cast<float>(w) * scale;
    float fh = static_cast<float>(h) * scale;

    auto positions = makeQuad(fx, fy, fw, fh);
    draw2d::submitQuad(positions, kFullUVs, color.u32, texture);
}

void VulkanRenderer::modeChanged(int width, int height, int flags) {
    VideoGeometry geometry{};
    geometry.width = std::max(1, width);
    geometry.height = std::max(1, height);
    geometry.flags = static_cast<vidFlags_t>(flags);

    swapInterval_ = readSwapIntervalSetting();
    if (swapInterval_ > 0) {
        geometry.flags = static_cast<vidFlags_t>(geometry.flags | QVF_VIDEOSYNC);
    } else {
        geometry.flags = static_cast<vidFlags_t>(geometry.flags & ~QVF_VIDEOSYNC);
    }

    applyVideoGeometry(geometry);
    updateUIScaling();

    if (!initialized_ || device_ == VK_NULL_HANDLE || swapchain_ == VK_NULL_HANDLE) {
        return;
    }

    if (geometry.width <= 0 || geometry.height <= 0) {
        return;
    }

    rebuildSwapchain();
}

bool VulkanRenderer::videoSync() const {
    if (!initialized_ || device_ == VK_NULL_HANDLE) {
        return true;
    }

    if (!lastSubmittedFrame_.has_value() || inFlightFrames_.empty()) {
        return true;
    }

    size_t index = *lastSubmittedFrame_;
    if (index >= inFlightFrames_.size()) {
        return true;
    }

    const InFlightFrame &frame = inFlightFrames_[index];
    if (frame.inFlight == VK_NULL_HANDLE) {
        return true;
    }

    VkResult status = vkGetFenceStatus(device_, frame.inFlight);
    if (status == VK_SUCCESS) {
        lastSubmittedFrame_.reset();
        return true;
    }

    if (status == VK_NOT_READY) {
        return false;
    }

    Com_Printf("refresh-vk: fence status check failed (VkResult %d).\n", static_cast<int>(status));
    return true;
}

void VulkanRenderer::expireDebugObjects() {
    R_ExpireDebugObjectsCPU();
}

bool VulkanRenderer::supportsPerPixelLighting() const {
    return resolveToggle(vk_perPixelLighting, "gl_per_pixel_lighting", true) &&
           resolveToggle(vk_dynamic, "gl_dynamic", true);
}

r_opengl_config_t VulkanRenderer::getGLConfig() const {
    r_opengl_config_t cfg{};
    cfg.colorbits = colorBitsForFormat(swapchainFormat_);
    cfg.depthbits = 0;
    cfg.stencilbits = 0;
    cfg.multisamples = 0;
    cfg.debug = supportsDebugUtils_ ? 1 : 0;
    cfg.profile = QGL_PROFILE_NONE;
    uint32_t apiVersion = physicalDeviceProperties_.apiVersion;
    if (apiVersion == 0) {
        apiVersion = VK_API_VERSION_1_0;
    }
    cfg.major_ver = static_cast<uint8_t>(VK_API_VERSION_MAJOR(apiVersion));
    cfg.minor_ver = static_cast<uint8_t>(VK_API_VERSION_MINOR(apiVersion));
    return cfg;
}

void VulkanRenderer::loadKFont(kfont_t *font, const char *filename) {
    if (!font) {
        return;
    }

    kfontCache_.erase(font);

    RendererKFontLoadContext context{};
    context.userData = this;
    context.registerImage = [](void *userData, const char *path, imagetype_t type, imageflags_t flags) -> qhandle_t {
        auto *renderer = static_cast<VulkanRenderer *>(userData);
        return renderer->registerImage(path, type, flags);
    };

    RendererKFontData data{};
    bool loaded = Renderer_LoadKFont(filename, context, &data);

    if (!loaded) {
        qhandle_t fallbackTexture = registerImage("_kfont", IT_FONT, IF_PERMANENT);
        Renderer_BuildFallbackKFont(RENDERER_DEFAULT_KFONT_WIDTH, RENDERER_DEFAULT_KFONT_HEIGHT,
                                    fallbackTexture, &data);
    }

    Renderer_AssignKFont(font, data);

    if (loaded) {
        KFontRecord record{};
        record.texture = data.texture;
        record.glyphs = data.glyphs;
        record.lineHeight = data.lineHeight;
        record.sw = data.sw;
        record.sh = data.sh;
        kfontCache_[font] = record;
    }
}

const kfont_char_t *VulkanRenderer::lookupKFontChar(const kfont_t *kfont, uint32_t codepoint) const {
    if (const KFontRecord *record = findKFontRecord(kfont)) {
        return Renderer_LookupKFontGlyph(record->glyphs.data(), record->glyphs.size(), codepoint);
    }

    return Renderer_LookupKFontGlyph(kfont, codepoint);
}









} // namespace refresh::vk

} // namespace refresh::vk
