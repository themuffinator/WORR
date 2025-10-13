#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "client/video.h"
#include "common/cvar.h"
#include "common/math.h"
#include "common/common.h"
#include "refresh/refresh.h"

namespace refresh::vk {

namespace draw2d {
    struct Submission;
}

class VulkanRenderer {
public:
    VulkanRenderer();
    ~VulkanRenderer();

    bool init(bool total);
    void shutdown(bool total);

    void beginRegistration(const char *map);
    qhandle_t registerModel(const char *name);
    qhandle_t registerImage(const char *name, imagetype_t type, imageflags_t flags);
    void setSky(const char *name, float rotate, bool autorotate, const vec3_t axis);
    void endRegistration();

    void beginFrame();
    void endFrame();
    void renderFrame(const refdef_t *fd);
    void lightPoint(const vec3_t origin, vec3_t light) const;

    void setClipRect(const clipRect_t *clip);
    float clampScale(cvar_t *var) const;
    void setScale(float scale);
    int autoScale() const;

    void drawChar(int x, int y, int flags, int ch, color_t color, qhandle_t font);
    void drawStretchChar(int x, int y, int w, int h, int flags, int ch, color_t color, qhandle_t font);
    int drawStringStretch(int x, int y, int scale, int flags, size_t maxChars,
                          const char *string, color_t color, qhandle_t font);
    int drawKFontChar(int x, int y, int scale, int flags, uint32_t codepoint,
                      color_t color, const kfont_t *kfont);

    bool getPicSize(int *w, int *h, qhandle_t pic) const;
    void drawPic(int x, int y, color_t color, qhandle_t pic);
    void drawStretchPic(int x, int y, int w, int h, color_t color, qhandle_t pic);
    void drawStretchRotatePic(int x, int y, int w, int h, color_t color, float angle,
                              int pivot_x, int pivot_y, qhandle_t pic);
    void drawKeepAspectPic(int x, int y, int w, int h, color_t color, qhandle_t pic);
    void drawStretchRaw(int x, int y, int w, int h);
    void updateRawPic(int pic_w, int pic_h, const uint32_t *pic);
    void tileClear(int x, int y, int w, int h, qhandle_t pic);
    void drawFill8(int x, int y, int w, int h, int c);
    void drawFill32(int x, int y, int w, int h, color_t color);

    void modeChanged(int width, int height, int flags);
    bool videoSync() const;
    void expireDebugObjects();
    bool supportsPerPixelLighting() const;
    r_opengl_config_t getGLConfig() const;

    void loadKFont(kfont_t *font, const char *filename);
    const kfont_char_t *lookupKFontChar(const kfont_t *kfont, uint32_t codepoint) const;

private:
    static constexpr size_t kKFontGlyphCount = static_cast<size_t>(KFONT_ASCII_MAX - KFONT_ASCII_MIN + 1);

    enum FogBits : uint32_t {
        FogNone = 0,
        FogGlobal = 1u << 0,
        FogHeight = 1u << 1,
        FogSky = 1u << 2,
    };
  
    enum class PipelineKind {
        InlineBsp,
        Alias,
        Sprite,
        Weapon,
    };

    struct PipelineDesc {
        PipelineKind kind = PipelineKind::Alias;
        std::string debugName;
    };

    struct RenderQueues {
        std::vector<const entity_t *> beams;
        std::vector<const entity_t *> flares;
        std::vector<const entity_t *> bmodels;
        std::vector<const entity_t *> opaque;
        std::vector<const entity_t *> alphaBack;
        std::vector<const entity_t *> alphaFront;

        void clear();
    };

    struct BeamPrimitive {
        std::array<float, 3> start{};
        std::array<float, 3> end{};
        float radius = 0.0f;
        color_t color = COLOR_WHITE;
    };

    struct ParticleBillboard {
        std::array<float, 3> origin{};
        float scale = 1.0f;
        float alpha = 1.0f;
        color_t color = COLOR_WHITE;
    };

    struct FlarePrimitive {
        std::array<float, 3> origin{};
        float scale = 1.0f;
        color_t color = COLOR_WHITE;
    };

    struct DebugLinePrimitive {
        std::array<float, 3> start{};
        std::array<float, 3> end{};
        color_t color = COLOR_WHITE;
        bool depthTest = true;
    };

    struct FramePrimitiveBuffers {
        std::vector<BeamPrimitive> beams;
        std::vector<ParticleBillboard> particles;
        std::vector<FlarePrimitive> flares;
        std::vector<DebugLinePrimitive> debugLines;

        void clear();
    };

    struct FrameStats {
        size_t drawCalls = 0;
        size_t pipelinesBound = 0;
        size_t beams = 0;
        size_t particles = 0;
        size_t flares = 0;
        size_t debugLines = 0;

        void reset();
    };

    struct EnumHash {
        template <typename T>
        size_t operator()(T value) const noexcept {
            return static_cast<size_t>(value);
        }
    };

    struct SkyDefinition {
        std::string name;
        float rotate = 0.0f;
        bool autorotate = false;
        std::array<float, 3> axis{ 0.0f, 0.0f, 1.0f };
    };

    struct ModelRecord {
        struct AliasFrameMetadata {
            std::array<float, 3> boundsMin{0.0f, 0.0f, 0.0f};
            std::array<float, 3> boundsMax{0.0f, 0.0f, 0.0f};
            std::array<float, 3> scale{0.0f, 0.0f, 0.0f};
            std::array<float, 3> translate{0.0f, 0.0f, 0.0f};
            float radius = 0.0f;
        };

        struct SpriteFrameMetadata {
            int width = 0;
            int height = 0;
            int originX = 0;
            int originY = 0;
        };

        qhandle_t handle = 0;
        std::string name;
        unsigned registrationSequence = 0;
        int type = 0;
        int numFrames = 0;
        int numMeshes = 0;
        bool inlineModel = false;
        std::vector<AliasFrameMetadata> aliasFrames;
        std::vector<SpriteFrameMetadata> spriteFrames;
    };

    struct ImageRecord {
        qhandle_t handle = 0;
        std::string name;
        imagetype_t type = IT_PIC;
        imageflags_t flags = IF_NONE;
        int width = 0;
        int height = 0;
        bool transparent = false;
        unsigned registrationSequence = 0;
    };

    struct KFontRecord {
        qhandle_t texture = 0;
        std::array<kfont_char_t, kKFontGlyphCount> glyphs{};
        uint16_t lineHeight = 0;
        float sw = 0.0f;
        float sh = 0.0f;
    };

    struct RawPicState {
        int width = 0;
        int height = 0;
        std::vector<uint32_t> pixels;
    };

    using ModelMap = std::unordered_map<qhandle_t, ModelRecord>;
    using ImageMap = std::unordered_map<qhandle_t, ImageRecord>;
    using NameLookup = std::unordered_map<std::string, qhandle_t>;

    struct FrameState {
        refdef_t refdef{};
        std::vector<entity_t> entities;
        std::vector<dlight_t> dlights;
        std::vector<particle_t> particles;
        std::array<lightstyle_t, MAX_LIGHTSTYLES> lightstyles{};
        std::vector<uint8_t> areaBits;
        bool hasLightstyles = false;
        bool hasAreabits = false;
        bool hasRefdef = false;
        bool inWorldPass = false;
        bool worldRendered = false;
        bool dynamicLightsUploaded = false;
        bool skyActive = false;
        FogBits fogBits = FogNone;
        FogBits fogBitsSky = FogNone;
        bool perPixelLighting = false;
    };

    qhandle_t nextHandle();
    qhandle_t registerResource(NameLookup &lookup, std::string_view name);

    void resetTransientState();
    void resetFrameState();
    void prepareFrameState(const refdef_t &fd);
    void evaluateFrameSettings();
    void uploadDynamicLights();
    void updateSkyState();
    void beginWorldPass();
    void renderWorld();
    void endWorldPass();
    void classifyEntities(const refdef_t &fd);
    void buildEffectBuffers(const refdef_t &fd);
    void recordDrawCall(const PipelineDesc &pipeline, std::string_view label, size_t count = 0);
    void recordStage(std::string_view label);
    PipelineDesc makePipeline(PipelineKind kind) const;
    const PipelineDesc &ensurePipeline(PipelineKind kind);
    PipelineKind selectPipelineForEntity(const entity_t &ent) const;
    const ModelRecord *findModelRecord(qhandle_t handle) const;
    const ImageRecord *findImageRecord(qhandle_t handle) const;
    const KFontRecord *findKFontRecord(const kfont_t *font) const;
    std::string_view classifyModelName(const ModelRecord *record) const;

    void submit2DDraw(const draw2d::Submission &submission);
    bool canSubmit2D() const;
    qhandle_t ensureWhiteTexture();
    qhandle_t ensureRawTexture();

    std::atomic<qhandle_t> handleCounter_;
    bool initialized_ = false;
    bool frameActive_ = false;

    RenderQueues frameQueues_{};
    FramePrimitiveBuffers framePrimitives_{};
    FrameStats frameStats_{};
    std::vector<std::string> commandLog_{};

    SkyDefinition sky_{};
    std::string currentMap_;

    struct ScissorRect {
        int32_t x = 0;
        int32_t y = 0;
        uint32_t width = 0;
        uint32_t height = 0;

        bool operator==(const ScissorRect &other) const {
            return x == other.x && y == other.y && width == other.width && height == other.height;
        }

        bool operator!=(const ScissorRect &other) const {
            return !(*this == other);
        }
    };

    void recordScissorCommand(const ScissorRect &rect, bool clipped);
    ScissorRect fullScissorRect() const;
    std::optional<ScissorRect> scaledClipRect(const clipRect_t &clip) const;

    std::optional<clipRect_t> clipRect_;
    std::optional<ScissorRect> activeScissor_;
    float scale_ = 1.0f;
    int autoScaleValue_ = 1;

    ModelMap models_;
    ImageMap images_;
    NameLookup modelLookup_;
    NameLookup imageLookup_;
    std::unordered_map<const kfont_t *, KFontRecord> kfontCache_;
    RawPicState rawPic_;
    FrameState frameState_{};
    qhandle_t whiteTextureHandle_ = 0;
    qhandle_t rawTextureHandle_ = 0;

    std::unordered_map<PipelineKind, PipelineDesc, EnumHash> pipelines_;
};

} // namespace refresh::vk


