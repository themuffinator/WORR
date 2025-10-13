#include "renderer.h"

#include "vk_draw2d.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

#include "common/cmodel.h"

refcfg_t r_config = {};
unsigned r_registration_sequence = 0;

namespace refresh::vk {

namespace {
    constexpr int kDefaultCharWidth = 8;
    constexpr int kDefaultCharHeight = 8;
    constexpr float kInverseLightIntensity = 1.0f / 255.0f;

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

    std::array<float, 3> toArray(const vec3_t value) {
        return { value[0], value[1], value[2] };
    }
}

void VulkanRenderer::RenderQueues::clear() {
    beams.clear();
    flares.clear();
    bmodels.clear();
    opaque.clear();
    alphaBack.clear();
    alphaFront.clear();
}

void VulkanRenderer::FramePrimitiveBuffers::clear() {
    beams.clear();
    particles.clear();
    flares.clear();
    debugLines.clear();
}

void VulkanRenderer::FrameStats::reset() {
    drawCalls = 0;
    pipelinesBound = 0;
    beams = 0;
    particles = 0;
    flares = 0;
    debugLines = 0;
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
    resetFrameState();
}

void VulkanRenderer::resetFrameState() {
    frameQueues_.clear();
    framePrimitives_.clear();
    frameStats_.reset();
}

void VulkanRenderer::recordStage(std::string_view label) {
    commandLog_.emplace_back(label);
}

void VulkanRenderer::recordDrawCall(const PipelineDesc &pipeline, std::string_view label, size_t count) {
    std::string entry{label};
    entry.append(" [");
    entry.append(pipeline.debugName);
    entry.push_back(']');
    if (count > 0) {
        entry.push_back(' ');
        entry.push_back('x');
        entry.append(std::to_string(count));
    }
    commandLog_.push_back(std::move(entry));
    frameStats_.drawCalls += 1;
}

VulkanRenderer::PipelineDesc VulkanRenderer::makePipeline(PipelineKind kind) const {
    PipelineDesc desc{};
    desc.kind = kind;
    switch (kind) {
    case PipelineKind::InlineBsp:
        desc.debugName = "inline_bsp";
        break;
    case PipelineKind::Alias:
        desc.debugName = "alias";
        break;
    case PipelineKind::Sprite:
        desc.debugName = "sprite";
        break;
    case PipelineKind::Weapon:
        desc.debugName = "weapon";
        break;
    }
    return desc;
}

const VulkanRenderer::PipelineDesc &VulkanRenderer::ensurePipeline(PipelineKind kind) {
    if (auto it = pipelines_.find(kind); it != pipelines_.end()) {
        return it->second;
    }

    PipelineDesc desc = makePipeline(kind);
    auto [it, inserted] = pipelines_.emplace(kind, std::move(desc));
    if (inserted) {
        frameStats_.pipelinesBound += 1;
    }
    return it->second;
}

const VulkanRenderer::ModelRecord *VulkanRenderer::findModelRecord(qhandle_t handle) const {
    if (auto it = models_.find(handle); it != models_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::string_view VulkanRenderer::classifyModelName(const ModelRecord *record) const {
    if (!record || record->name.empty()) {
        return {};
    }
    const std::string &name = record->name;
    size_t dot = name.find_last_of('.');
    if (dot == std::string::npos || dot + 1 >= name.size()) {
        return {};
    }
    return std::string_view{name}.substr(dot);
}

VulkanRenderer::PipelineKind VulkanRenderer::selectPipelineForEntity(const entity_t &ent) const {
    constexpr uint32_t kInlineMask = 1u << 31;
    if ((ent.model & kInlineMask) != 0u) {
        return PipelineKind::InlineBsp;
    }
    if (ent.flags & RF_WEAPONMODEL) {
        return PipelineKind::Weapon;
    }

    const ModelRecord *record = findModelRecord(ent.model);
    std::string_view extension = classifyModelName(record);
    if (extension == ".sp2" || extension == ".spr" || extension == ".sprite") {
        return PipelineKind::Sprite;
    }

    return PipelineKind::Alias;
}

void VulkanRenderer::classifyEntities(const refdef_t &fd) {
    frameQueues_.clear();

    if (!fd.entities || fd.num_entities <= 0) {
        return;
    }

    static cvar_t *drawOrder = Cvar_FindVar("gl_draworder");
    float drawOrderThreshold = 0.5f;
    if (drawOrder) {
        drawOrderThreshold = drawOrder->value;
    }

    for (int i = 0; i < fd.num_entities; ++i) {
        const entity_t *ent = &fd.entities[i];
        if (ent->flags & RF_BEAM) {
            if (ent->frame) {
                frameQueues_.beams.push_back(ent);
            }
            continue;
        }

        if (ent->flags & RF_FLARE) {
            frameQueues_.flares.push_back(ent);
            continue;
        }

        constexpr uint32_t kInlineMask = 1u << 31;
        if ((ent->model & kInlineMask) != 0u) {
            frameQueues_.bmodels.push_back(ent);
            continue;
        }

        if (!(ent->flags & RF_TRANSLUCENT)) {
            frameQueues_.opaque.push_back(ent);
            continue;
        }

        float alpha = ent->alpha;
        if (!std::isfinite(alpha)) {
            alpha = 1.0f;
        }

        if ((ent->flags & RF_WEAPONMODEL) || alpha <= drawOrderThreshold) {
            frameQueues_.alphaFront.push_back(ent);
            continue;
        }

        frameQueues_.alphaBack.push_back(ent);
    }
}

void VulkanRenderer::buildEffectBuffers(const refdef_t &fd) {
    framePrimitives_.clear();

    for (const entity_t *beamEntity : frameQueues_.beams) {
        BeamPrimitive primitive{};
        primitive.start = toArray(beamEntity->origin);
        primitive.end = toArray(beamEntity->oldorigin);
        primitive.radius = static_cast<float>(beamEntity->frame);
        primitive.color = beamEntity->rgba;
        framePrimitives_.beams.push_back(primitive);
    }
    frameStats_.beams = framePrimitives_.beams.size();

    if (fd.particles && fd.num_particles > 0) {
        framePrimitives_.particles.reserve(static_cast<size_t>(fd.num_particles));
        for (int i = 0; i < fd.num_particles; ++i) {
            const particle_t &particle = fd.particles[i];
            ParticleBillboard billboard{};
            billboard.origin = toArray(particle.origin);
            billboard.scale = particle.scale;
            billboard.alpha = particle.alpha;
            billboard.color = particle.rgba;
            framePrimitives_.particles.push_back(billboard);
        }
    }
    frameStats_.particles = framePrimitives_.particles.size();

    for (const entity_t *flareEntity : frameQueues_.flares) {
        FlarePrimitive flare{};
        flare.origin = toArray(flareEntity->origin);
        flare.scale = std::max(0.0f, flareEntity->alpha);
        flare.color = flareEntity->rgba;
        framePrimitives_.flares.push_back(flare);
    }
    frameStats_.flares = framePrimitives_.flares.size();

    frameStats_.debugLines = framePrimitives_.debugLines.size();
}

void VulkanRenderer::submit2DDraw(const draw2d::Submission &submission) {
    if (!submission.vertices || !submission.indices) {
        return;
    }

    if (submission.vertexCount == 0 || submission.indexCount == 0) {
        return;
    }

    Com_DPrintf("vk_draw2d: flushing %zu vertices, %zu indices (texture %d)\n",
                submission.vertexCount,
                submission.indexCount,
                static_cast<int>(submission.texture));
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

    if (!draw2d::initialize()) {
        Com_Printf("Failed to initialize Vulkan 2D helper.\n");
        return false;
    }

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
        draw2d::shutdown();
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

    resetFrameState();
    commandLog_.clear();

    frameActive_ = true;

    if (!draw2d::begin([this](const draw2d::Submission &submission) {
            submit2DDraw(submission);
        })) {
        Com_Printf("vk_draw2d: failed to begin 2D batch for frame.\n");
    }
}

void VulkanRenderer::endFrame() {
    if (!initialized_ || !frameActive_) {
        return;
    }

    if (vid && vid->swap_buffers) {
        vid->swap_buffers();
    }

    draw2d::end();
    commandLog_.clear();
    frameStats_.reset();

    frameActive_ = false;
}

void VulkanRenderer::renderFrame(const refdef_t *fd) {
    if (!initialized_ || !fd) {
        return;
    }

    if (fd->height > 0) {
        autoScaleValue_ = std::max(1, fd->height / SCREEN_HEIGHT);
    }

    prepareFrameState(*fd);
    evaluateFrameSettings();
    uploadDynamicLights();
    updateSkyState();

    beginWorldPass();
    if (!(frameState_.refdef.rdflags & RDF_NOWORLDMODEL)) {
        renderWorld();
    }
    endWorldPass();
    recordStage("frame.begin");

    classifyEntities(*fd);
    buildEffectBuffers(*fd);

    if (!(fd->rdflags & RDF_NOWORLDMODEL)) {
        recordStage("world.draw");
    }

    auto processQueue = [&](const std::vector<const entity_t *> &queue, std::string_view label) {
        if (queue.empty()) {
            return;
        }

        PipelineKind currentKind = PipelineKind::Alias;
        size_t batchCount = 0;
        const PipelineDesc *pipeline = nullptr;

        for (auto it = queue.rbegin(); it != queue.rend(); ++it) {
            PipelineKind kind = selectPipelineForEntity(**it);
            if (!pipeline || kind != currentKind) {
                if (pipeline && batchCount) {
                    recordDrawCall(*pipeline, label, batchCount);
                }
                currentKind = kind;
                pipeline = &ensurePipeline(kind);
                batchCount = 0;
            }
            ++batchCount;
        }

        if (pipeline && batchCount) {
            recordDrawCall(*pipeline, label, batchCount);
        }
    };

    processQueue(frameQueues_.bmodels, "entities.inline");
    processQueue(frameQueues_.opaque, "entities.opaque");
    processQueue(frameQueues_.alphaBack, "entities.alpha_back");

    if (!framePrimitives_.beams.empty()) {
        recordDrawCall(ensurePipeline(PipelineKind::Alias), "fx.beams", framePrimitives_.beams.size());
    }

    if (!framePrimitives_.particles.empty()) {
        recordDrawCall(ensurePipeline(PipelineKind::Sprite), "fx.particles", framePrimitives_.particles.size());
    }

    if (!framePrimitives_.flares.empty()) {
        recordDrawCall(ensurePipeline(PipelineKind::Sprite), "fx.flares", framePrimitives_.flares.size());
    }

    processQueue(frameQueues_.alphaFront, "entities.alpha_front");

    if (!framePrimitives_.debugLines.empty()) {
        recordDrawCall(ensurePipeline(PipelineKind::Alias), "debug.lines", framePrimitives_.debugLines.size());
    }

    bool waterwarp = (fd->rdflags & RDF_UNDERWATER) != 0;
    if (waterwarp) {
        recordStage("post.waterwarp");
    }

    bool bloom = false;
    if (bloom) {
        recordStage("post.bloom");
    }

    if (fd->screen_blend[3] > 0.0f || fd->damage_blend[3] > 0.0f) {
        recordStage("overlay.blend");
    }

    recordStage("frame.end");
}

void VulkanRenderer::lightPoint(const vec3_t origin, vec3_t light) const {
    if (!light) {
        return;
    }

    light[0] = 1.0f;
    light[1] = 1.0f;
    light[2] = 1.0f;

    if (!frameState_.hasRefdef) {
        return;
    }

    for (const auto &dlight : frameState_.dlights) {
        float dx = origin[0] - dlight.origin[0];
        float dy = origin[1] - dlight.origin[1];
        float dz = origin[2] - dlight.origin[2];
        float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

        float contribution = dlight.radius - DLIGHT_CUTOFF - distance;
        if (contribution <= 0.0f) {
            continue;
        }

        float scale = contribution * kInverseLightIntensity * dlight.intensity;
        light[0] += scale * dlight.color[0];
        light[1] += scale * dlight.color[1];
        light[2] += scale * dlight.color[2];
    }
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
    if (!frameActive_ || !draw2d::isActive()) {
        return;
    }

    if (w <= 0 || h <= 0) {
        return;
    }

    std::array<std::array<float, 2>, 4> positions{{
        {static_cast<float>(x), static_cast<float>(y)},
        {static_cast<float>(x + w), static_cast<float>(y)},
        {static_cast<float>(x + w), static_cast<float>(y + h)},
        {static_cast<float>(x), static_cast<float>(y + h)}
    }};

    std::array<std::array<float, 2>, 4> uvs{{
        {0.0f, 0.0f},
        {1.0f, 0.0f},
        {1.0f, 1.0f},
        {0.0f, 1.0f}
    }};

    draw2d::submitQuad(positions, uvs, color.u32, pic);
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

void VulkanRenderer::resetFrameState() {
    frameState_.refdef = {};
    frameState_.entities.clear();
    frameState_.dlights.clear();
    frameState_.particles.clear();
    frameState_.lightstyles.fill(lightstyle_t{});
    frameState_.areaBits.clear();
    frameState_.hasLightstyles = false;
    frameState_.hasAreabits = false;
    frameState_.hasRefdef = false;
    frameState_.inWorldPass = false;
    frameState_.worldRendered = false;
    frameState_.dynamicLightsUploaded = false;
    frameState_.skyActive = false;
    frameState_.fogBits = FogNone;
    frameState_.fogBitsSky = FogNone;
    frameState_.perPixelLighting = false;
}

void VulkanRenderer::prepareFrameState(const refdef_t &fd) {
    frameState_.refdef = fd;

    frameState_.entities.clear();
    if (fd.entities && fd.num_entities > 0) {
        frameState_.entities.assign(fd.entities, fd.entities + fd.num_entities);
        for (auto &ent : frameState_.entities) {
            ent.next = nullptr;
        }
        frameState_.refdef.entities = frameState_.entities.data();
        frameState_.refdef.num_entities = static_cast<int>(frameState_.entities.size());
    } else {
        frameState_.refdef.entities = nullptr;
        frameState_.refdef.num_entities = 0;
    }

    frameState_.dlights.clear();
    if (fd.dlights && fd.num_dlights > 0) {
        frameState_.dlights.assign(fd.dlights, fd.dlights + fd.num_dlights);
        frameState_.refdef.dlights = frameState_.dlights.data();
        frameState_.refdef.num_dlights = static_cast<int>(frameState_.dlights.size());
    } else {
        frameState_.refdef.dlights = nullptr;
        frameState_.refdef.num_dlights = 0;
    }

    frameState_.particles.clear();
    if (fd.particles && fd.num_particles > 0) {
        frameState_.particles.assign(fd.particles, fd.particles + fd.num_particles);
        frameState_.refdef.particles = frameState_.particles.data();
        frameState_.refdef.num_particles = static_cast<int>(frameState_.particles.size());
    } else {
        frameState_.refdef.particles = nullptr;
        frameState_.refdef.num_particles = 0;
    }

    frameState_.hasLightstyles = false;
    if (fd.lightstyles) {
        std::copy(fd.lightstyles, fd.lightstyles + MAX_LIGHTSTYLES, frameState_.lightstyles.begin());
        frameState_.refdef.lightstyles = frameState_.lightstyles.data();
        frameState_.hasLightstyles = true;
    } else {
        frameState_.refdef.lightstyles = nullptr;
    }

    frameState_.areaBits.clear();
    frameState_.hasAreabits = false;
    if (fd.areabits) {
        frameState_.areaBits.assign(fd.areabits, fd.areabits + MAX_MAP_AREA_BYTES);
        frameState_.refdef.areabits = frameState_.areaBits.data();
        frameState_.hasAreabits = true;
    } else {
        frameState_.refdef.areabits = nullptr;
    }

    frameState_.hasRefdef = true;
}

void VulkanRenderer::evaluateFrameSettings() {
    frameState_.fogBits = FogNone;
    frameState_.fogBitsSky = FogNone;

    if (frameState_.refdef.fog.density > 0.0f) {
        frameState_.fogBits = static_cast<FogBits>(frameState_.fogBits | FogGlobal);
    }

    if (frameState_.refdef.heightfog.density > 0.0f && frameState_.refdef.heightfog.falloff > 0.0f) {
        frameState_.fogBits = static_cast<FogBits>(frameState_.fogBits | FogHeight);
    }

    if (frameState_.refdef.fog.sky_factor > 0.0f) {
        frameState_.fogBitsSky = static_cast<FogBits>(frameState_.fogBitsSky | FogSky);
    }

    frameState_.perPixelLighting = frameState_.refdef.num_dlights > 0;
}

void VulkanRenderer::uploadDynamicLights() {
    frameState_.dynamicLightsUploaded = !frameState_.dlights.empty();
}

void VulkanRenderer::updateSkyState() {
    frameState_.skyActive = !sky_.name.empty();
}

void VulkanRenderer::beginWorldPass() {
    frameState_.inWorldPass = true;
    frameState_.worldRendered = false;
}

void VulkanRenderer::renderWorld() {
    frameState_.worldRendered = true;
}

void VulkanRenderer::endWorldPass() {
    frameState_.inWorldPass = false;
}

} // namespace refresh::vk
