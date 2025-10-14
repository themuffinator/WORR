#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <vulkan/vulkan_core.h>

#include "client/video.h"
#include "common/common.h"
#include "common/cvar.h"
#include "common/math.h"
#include "common/bsp.h"
#include "refresh/refresh.h"

namespace refresh::vk {

namespace draw2d {
    struct Submission;
}

class PipelineLibrary;

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

    const std::vector<std::string> &platformInstanceExtensions() const;
    bool createPlatformSurface(VkInstance instance, const VkAllocationCallbacks *allocator);
    void destroyPlatformSurface(VkInstance instance, const VkAllocationCallbacks *allocator);
    VkSurfaceKHR platformSurface() const;

    VkDevice device() const { return device_; }
    VkRenderPass renderPass() const { return renderPass_; }
    VkPipelineLayout pipelineLayoutFor(PipelineKind kind) const;

private:
    friend class PipelineLibrary;
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
        Draw2D,
        BeamSimple,
        BeamCylindrical,
        ParticleAlpha,
        ParticleAdditive,
        Flare,
        DebugLineDepth,
        DebugLineNoDepth,
    };

    struct PipelineKey {
        PipelineKind kind = PipelineKind::Alias;
        FogBits fogBits = FogNone;
        FogBits fogSkyBits = FogNone;
        bool perPixelLighting = false;
        bool dynamicLights = false;

        bool operator==(const PipelineKey &other) const noexcept {
            return kind == other.kind && fogBits == other.fogBits && fogSkyBits == other.fogSkyBits &&
                   perPixelLighting == other.perPixelLighting && dynamicLights == other.dynamicLights;
        }
    };

    struct PipelineDesc {
        enum class BlendMode {
            None,
            Alpha,
            Additive,
        };

        PipelineKey key{};
        std::string debugName;
        VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        BlendMode blend = BlendMode::None;
        bool depthTest = true;
        bool depthWrite = true;
        bool textured = false;
        bool usesFog = false;
        bool usesSkyFog = false;
        bool usesDynamicLights = false;
    };

    struct alignas(16) EntityPushConstants {
        std::array<float, 16> modelMatrix{};
        std::array<float, 4> color{};
        std::array<float, 4> lighting{};
        std::array<float, 4> misc{};
        std::array<uint32_t, 4> indices{};
    };

    struct PipelineKeyHash {
        size_t operator()(const PipelineKey &key) const noexcept {
            size_t hash = static_cast<size_t>(key.kind);
            hash ^= static_cast<size_t>(key.fogBits) << 4;
            hash ^= static_cast<size_t>(key.fogSkyBits) << 8;
            hash ^= static_cast<size_t>(key.perPixelLighting) << 12;
            hash ^= static_cast<size_t>(key.dynamicLights) << 16;
            return hash;
        }
    };

    // Frame data structures
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

    struct EffectVertexStreams {
        struct BeamVertex {
            std::array<float, 3> position{};
            std::array<float, 2> uv{};
            std::array<float, 4> color{};
        };

        struct BillboardVertex {
            std::array<float, 3> position{};
            std::array<float, 2> uv{};
            std::array<float, 4> color{};
        };

        struct DebugLineVertex {
            std::array<float, 3> position{};
            std::array<float, 4> color{};
        };

        std::vector<BeamVertex> beamVertices;
        std::vector<uint16_t> beamIndices;
        std::vector<BillboardVertex> particleVertices;
        std::vector<BillboardVertex> flareVertices;
        std::vector<uint16_t> flareIndices;
        std::vector<DebugLineVertex> debugLinesDepth;
        std::vector<DebugLineVertex> debugLinesNoDepth;

        void clear();
    };

    struct WorldSurfaceDraw {
        const mface_t *face = nullptr;
        uint32_t firstVertex = 0;
        uint32_t vertexCount = 0;
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
        struct BufferAllocationInfo {
            VkBuffer buffer = VK_NULL_HANDLE;
            VkDeviceMemory memory = VK_NULL_HANDLE;
            VkDeviceSize offset = 0;
            VkDeviceSize size = 0;
        };

        struct DescriptorReference {
            VkDescriptorSet set = VK_NULL_HANDLE;
            uint32_t binding = 0;
            VkDescriptorBufferInfo vertex{};
            VkDescriptorBufferInfo index{};
        };

        struct MeshGeometry {
            BufferAllocationInfo vertex;
            BufferAllocationInfo index;
            DescriptorReference descriptor;
            VkIndexType indexType = VK_INDEX_TYPE_UINT16;
            size_t vertexCount = 0;
            size_t indexCount = 0;
            std::vector<uint8_t> vertexStaging;
            std::vector<uint8_t> indexStaging;
            std::vector<qhandle_t> skinHandles;
            bool uploaded = false;
        };

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
        std::vector<MeshGeometry> meshGeometry;
    };

    struct ImageRecord {
        qhandle_t handle = 0;
        std::string name;
        imagetype_t type = IT_PIC;
        imageflags_t flags = IF_NONE;
        int width = 0;
        int height = 0;
        int uploadWidth = 0;
        int uploadHeight = 0;
        bool transparent = false;
        unsigned registrationSequence = 0;
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        VkFormat format = VK_FORMAT_UNDEFINED;
        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkExtent3D extent{ 0u, 0u, 1u };
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

    struct OffscreenTarget {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        VkExtent2D extent{ 0u, 0u };
    };

    struct WorldDrawCommand {
        ModelRecord::DescriptorReference geometry{};
        VkDescriptorSet diffuse = VK_NULL_HANDLE;
        VkDescriptorSet lightmap = VK_NULL_HANDLE;
        VkDescriptorSet dynamicLights = VK_NULL_HANDLE;
        uint32_t firstIndex = 0;
        uint32_t indexCount = 0;
        int32_t vertexOffset = 0;
        uint32_t vertexCount = 0;
    };

    struct Draw2DBatch {
        ModelRecord::BufferAllocationInfo vertex;
        ModelRecord::BufferAllocationInfo index;
        ModelRecord::DescriptorReference descriptor;
        VkIndexType indexType = VK_INDEX_TYPE_UINT16;
        size_t vertexCount = 0;
        size_t indexCount = 0;
        qhandle_t texture = 0;
    };

    using ModelMap = std::unordered_map<qhandle_t, ModelRecord>;
    using ImageMap = std::unordered_map<qhandle_t, ImageRecord>;
    using NameLookup = std::unordered_map<std::string, qhandle_t>;

    struct FrameState {
        refdef_t refdef{};
        std::vector<entity_t> entities;
        std::vector<dlight_t> dlights;
        struct DynamicLightGPU {
            std::array<float, 4> positionRadius{};
            std::array<float, 4> colorIntensity{};
            std::array<float, 4> cone{};
        };
        std::vector<DynamicLightGPU> dynamicLights;
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
        bool fogEnabled = false;
        bool fogSkyEnabled = false;
        bool perPixelLighting = false;
        bool dynamicLightsEnabled = false;
        int dynamicLightCount = 0;
        bool waterwarpActive = false;
        bool bloomActive = false;
        bool overlayBlendActive = false;
    };

    // Resource management
    qhandle_t nextHandle();
    qhandle_t registerResource(NameLookup &lookup, std::string_view name);

    // Frame rendering
    void clearFrameTransientQueues();
    void resetFrameStatistics();
    void resetFrameState();
    void prepareFrameState(const refdef_t &fd);
    void allocateModelGeometry(ModelRecord &record, const model_t &model);
    void bindModelGeometryBuffers(ModelRecord &record);
    EntityPushConstants buildEntityPushConstants(const entity_t &entity, const ModelRecord &record) const;
    VkDescriptorSet selectTextureDescriptor(const entity_t &entity, const ModelRecord::MeshGeometry &geometry);
    bool uploadMeshGeometry(ModelRecord::MeshGeometry &geometry);
    void destroyMeshGeometry(ModelRecord::MeshGeometry &geometry);
    void destroyModelRecord(ModelRecord &record);
    void destroyAllModelGeometry();
    bool createModelDescriptorResources();
    void destroyModelDescriptorResources();
    bool createBuffer(ModelRecord::BufferAllocationInfo &buffer,
                      VkDeviceSize size,
                      VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags properties);
    void destroyBuffer(ModelRecord::BufferAllocationInfo &buffer);
    bool copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size);
    void evaluateFrameSettings();
    void uploadDynamicLights();
    void updateSkyState();
    void beginWorldPass();
    void renderWorld();
    void endWorldPass();
    void markVisibleNodes(const bsp_t *bsp);
    void buildWorldFrustum(const ViewParameters &view);
    void gatherNodeSurfaces(const mnode_t *node, int clipFlags);
    void gatherLeafSurfaces(mleaf_t *leaf);
    void enqueueWorldFace(mface_t *face);
    bool clipNode(const mnode_t *node, int &clipFlags) const;
    void classifyEntities(const refdef_t &fd);
    void sortTransparentQueues(const refdef_t &fd);
    void buildEffectBuffers(const refdef_t &fd);
    void recordDrawCall(const PipelineDesc &pipeline, std::string_view label, size_t count = 0);
    void recordStage(std::string_view label);
    PipelineDesc makePipeline(const PipelineKey &key) const;
    PipelineKey buildPipelineKey(PipelineKind kind) const;
    const PipelineDesc &ensurePipeline(const PipelineKey &key);
    struct ViewParameters {
        std::array<std::array<float, 3>, 3> axis{};
        std::array<float, 3> origin{};
    };
    ViewParameters computeViewParameters(const refdef_t &fd) const;
    void streamBeamPrimitives(const ViewParameters &view, bool cylindricalStyle);
    void streamParticlePrimitives(const ViewParameters &view, bool additiveBlend);
    void streamFlarePrimitives(const ViewParameters &view);
    void streamDebugLinePrimitives();
    PipelineKind selectPipelineForEntity(const entity_t &ent) const;
    const ModelRecord *findModelRecord(qhandle_t handle) const;
    ModelRecord *findModelRecord(qhandle_t handle);
    const ImageRecord *findImageRecord(qhandle_t handle) const;
    const KFontRecord *findKFontRecord(const kfont_t *font) const;
    std::string_view classifyModelName(const ModelRecord *record) const;

    // UI drawing
    void submit2DDraw(const draw2d::Submission &submission);
    bool canSubmit2D() const;
    qhandle_t ensureWhiteTexture();
    qhandle_t ensureRawTexture();
    void destroy2DBatch(Draw2DBatch &batch);
    void clear2DBatches();

    // Device initialization
    void initializePlatformHooks();
    void collectPlatformInstanceExtensions();
    bool createTextureDescriptorSetLayout();
    void destroyTextureDescriptorSetLayout();
    void destroyImageRecord(ImageRecord &record);
    void destroyAllImageResources();
    bool allocateTextureDescriptor(ImageRecord &record);
    uint32_t findMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties) const;
    bool createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                      VkBuffer &buffer, VkDeviceMemory &memory);
    void destroyBuffer(VkBuffer buffer, VkDeviceMemory memory);
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
    void transitionImageLayout(VkCommandBuffer commandBuffer, VkImage image,
                               VkImageLayout oldLayout, VkImageLayout newLayout);
    void copyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer buffer, VkImage image,
                           uint32_t width, uint32_t height);
    bool uploadImagePixels(ImageRecord &record, const uint8_t *pixels, size_t size,
                            uint32_t width, uint32_t height, VkFormat format);
    bool ensureTextureResources(ImageRecord &record, const uint8_t *pixels, size_t size,
                                uint32_t width, uint32_t height, VkFormat format);
    bool createPostProcessResources();
    void destroyPostProcessResources();
    bool createOffscreenTarget(OffscreenTarget &target, VkExtent2D extent, VkFormat format, VkRenderPass renderPass,
                               VkImageUsageFlags usage);
    void destroyOffscreenTarget(OffscreenTarget &target);

    std::atomic<qhandle_t> handleCounter_;
    bool initialized_ = false;
    bool frameActive_ = false;

    RenderQueues frameQueues_{};
    FramePrimitiveBuffers framePrimitives_{};
    EffectVertexStreams effectStreams_{};
    FrameStats frameStats_{};
    std::vector<std::string> commandLog_{};
    std::vector<WorldSurfaceDraw> worldOpaqueSurfaces_{};
    std::vector<WorldSurfaceDraw> worldAlphaSurfaces_{};
    std::vector<WorldSurfaceDraw> worldSkySurfaces_{};
    std::array<cplane_t, 4> worldFrustumPlanes_{};
    unsigned int worldVisFrame_ = 0;
    unsigned int worldDrawFrame_ = 0;
    int worldViewCluster1_ = -1;
    int worldViewCluster2_ = -1;

    SkyDefinition sky_{};
    std::string currentMap_;

    ModelRecord::BufferAllocationInfo worldVertexBuffer_{};
    ModelRecord::BufferAllocationInfo worldIndexBuffer_{};
    VkIndexType worldIndexType_ = VK_INDEX_TYPE_UINT16;
    std::array<std::vector<WorldDrawCommand>, 3> worldSurfaceBuckets_{};

    struct VideoGeometry {
        int width = SCREEN_WIDTH;
        int height = SCREEN_HEIGHT;
        vidFlags_t flags = {};
    };

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

    // UI management
    VideoGeometry queryVideoGeometry() const;
    void applyVideoGeometry(const VideoGeometry &geometry);
    void resetTransientState();
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
    std::vector<Draw2DBatch> frame2DBatches_{};

    std::unordered_map<PipelineKey, PipelineDesc, PipelineKeyHash> pipelines_;
    std::unique_ptr<PipelineLibrary> pipelineLibrary_;

    struct PlatformHooks {
        vid_vk_get_instance_extensions_fn getInstanceExtensions = nullptr;
        vid_vk_create_surface_fn createSurface = nullptr;
        vid_vk_destroy_surface_fn destroySurface = nullptr;
    };

    PlatformHooks platformHooks_{};
    std::vector<std::string> platformInstanceExtensions_;
    VkInstance platformInstance_ = VK_NULL_HANDLE;
    VkSurfaceKHR platformSurface_ = VK_NULL_HANDLE;

    struct QueueFamilyIndices {
        std::optional<uint32_t> graphics;
        std::optional<uint32_t> present;

        bool complete() const {
            return graphics.has_value() && present.has_value();
        }
    };

    struct SwapchainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities{};
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    struct InFlightFrame {
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkSemaphore imageAvailable = VK_NULL_HANDLE;
        VkSemaphore renderFinished = VK_NULL_HANDLE;
        VkFence inFlight = VK_NULL_HANDLE;
        uint32_t imageIndex = 0;
        bool hasImage = false;
    };

    bool createInstance();
    void destroyInstance();
    bool createDeviceResources();
    void destroyDeviceResources();
    bool createDescriptorPool();
    void destroyDescriptorPool();
    bool createCommandPool();
    void destroyCommandPool();
    bool createSwapchainResources(VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE);
    void destroySwapchainResources();
    void destroyDepthResources();
    void destroySyncObjects();
    bool recreateSwapchain();
    bool rebuildSwapchain();
    QueueFamilyIndices queryQueueFamilies(VkPhysicalDevice device) const;
    SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice device) const;
    VkSurfaceFormatKHR chooseSwapchainFormat(const std::vector<VkSurfaceFormatKHR> &formats) const;
    VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR> &presentModes) const;
    VkExtent2D chooseSwapchainExtent(const VkSurfaceCapabilitiesKHR &capabilities) const;
    bool pickPhysicalDevice();
    bool createLogicalDevice();
    bool createSyncObjects();
    void destroyVulkan();
    void finishFrameRecording();
    void updateUIScaling();
    int computeAutoScale() const;
    int readSwapIntervalSetting() const;
    bool refreshSwapInterval(bool allowRecreate = true);

    VkInstance instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties physicalDeviceProperties_{};
    VkPhysicalDeviceFeatures supportedFeatures_{};
    VkPhysicalDeviceFeatures enabledFeatures_{};
    VkPhysicalDeviceMemoryProperties memoryProperties_{};
    uint32_t graphicsQueueFamily_ = VK_QUEUE_FAMILY_IGNORED;
    uint32_t presentQueueFamily_ = VK_QUEUE_FAMILY_IGNORED;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkQueue presentQueue_ = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapchainFormat_ = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent_{ 0u, 0u };
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;
    std::vector<VkFramebuffer> swapchainFramebuffers_;
    VkImage depthImage_ = VK_NULL_HANDLE;
    VkDeviceMemory depthImageMemory_ = VK_NULL_HANDLE;
    VkImageView depthImageView_ = VK_NULL_HANDLE;
    VkFormat depthFormat_ = VK_FORMAT_D24_UNORM_S8_UINT;
    std::vector<VkFence> imagesInFlight_;
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkRenderPass offscreenRenderPass_ = VK_NULL_HANDLE;
    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    std::vector<InFlightFrame> inFlightFrames_;
    size_t currentFrameIndex_ = 0;
    bool frameAcquired_ = false;
    bool vsyncEnabled_ = true;
    int swapInterval_ = 1;
    bool supportsDebugUtils_ = false;
    mutable std::optional<size_t> lastSubmittedFrame_;
    static constexpr size_t kMaxFramesInFlight = 2;
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout textureDescriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout modelDescriptorSetLayout_ = VK_NULL_HANDLE;
    VkPipelineLayout modelPipelineLayout_ = VK_NULL_HANDLE;
    std::vector<std::string> deviceExtensions_;
    OffscreenTarget sceneTarget_{};
    std::array<OffscreenTarget, 2> bloomTargets_{};
    bool postProcessAvailable_ = false;
    bool frameRenderPassActive_ = false;
    bool draw2DBegun_ = false;
    bool sceneTargetReady_ = false;
    std::array<bool, 2> bloomTargetsReady_{};
};

} // namespace refresh::vk


