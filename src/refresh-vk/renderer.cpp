#include "renderer.h"

#include "vk_draw2d.h"

#include "refresh/images.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <unordered_set>

#include "common/cmodel.h"
#include "common/files.h"
#include "refresh/gl.h"
#include "refresh/images.h"
#include "client/client.h"

extern uint32_t d_8to24table[256];

refcfg_t r_config = {};
unsigned r_registration_sequence = 0;

namespace refresh::vk {

namespace {
    constexpr int kDefaultCharWidth = 8;
    constexpr int kDefaultCharHeight = 8;
    constexpr float kInverseLightIntensity = 1.0f / 255.0f;
    constexpr float kFontCellSize = 1.0f / 16.0f;
    constexpr float kTileDivisor = 1.0f / 64.0f;
    constexpr float kShadowOffset = 1.0f;
    constexpr int kUIDropShadow = 1 << 4;
    constexpr int kUIAltColor = 1 << 5;
    constexpr int kUIXorColor = 1 << 7;

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

    constexpr float kParticleSize = 1.0f + M_SQRT1_2f;
    constexpr float kParticleScale = 1.0f / (2.0f * kParticleSize);
    constexpr float kParticleDistanceBias = 20.0f;
    constexpr float kParticleDistanceScale = 0.004f;
    constexpr int kBeamCylinderSides = 12;

    std::array<float, 4> toColorArray(color_t color, float alphaScale = 1.0f) {
        constexpr float kInv255 = 1.0f / 255.0f;
        float alpha = static_cast<float>(color.a) * kInv255 * alphaScale;
        alpha = std::clamp(alpha, 0.0f, 1.0f);
        return {
            static_cast<float>(color.r) * kInv255,
            static_cast<float>(color.g) * kInv255,
            static_cast<float>(color.b) * kInv255,
            alpha,
        };
    }

    void arrayToVec3(const std::array<float, 3> &src, vec3_t dst) {
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
    }

    VulkanRenderer::ModelRecord::AliasFrameMetadata makeAliasFrameMetadata(const maliasframe_t &frame) {
        VulkanRenderer::ModelRecord::AliasFrameMetadata metadata{};
        metadata.boundsMin = toArray(frame.bounds[0]);
        metadata.boundsMax = toArray(frame.bounds[1]);
        metadata.scale = toArray(frame.scale);
        metadata.translate = toArray(frame.translate);
        metadata.radius = frame.radius;
        return metadata;
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

    std::array<std::array<float, 2>, 4> makeQuadUVs(float s0, float t0, float s1, float t1) {
        return {{{ { s0, t0 }, { s1, t0 }, { s1, t1 }, { s0, t1 } }}};
    }

    void submitTexturedQuad(float x0, float y0, float x1, float y1,
                            float s0, float t0, float s1, float t1,
                            color_t color, qhandle_t texture) {
        draw2d::submitQuad(makeQuadPositions(x0, y0, x1, y1),
                           makeQuadUVs(s0, t0, s1, t1),
                           color.u32,
                           texture);
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

model_t *MOD_Find(const char *name);

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

void VulkanRenderer::EffectVertexStreams::clear() {
    beamVertices.clear();
    beamIndices.clear();
    particleVertices.clear();
    flareVertices.clear();
    flareIndices.clear();
    debugLinesDepth.clear();
    debugLinesNoDepth.clear();
}

void VulkanRenderer::FrameStats::reset() {
    drawCalls = 0;
    pipelinesBound = 0;
    beams = 0;
    particles = 0;
    flares = 0;
    debugLines = 0;
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
    activeScissor_.reset();
    scale_ = 1.0f;
    autoScaleValue_ = 1;
    resetFrameState();
}

void VulkanRenderer::resetFrameState() {
    frameQueues_.clear();
    framePrimitives_.clear();
    effectStreams_.clear();
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
    case PipelineKind::BeamSimple:
        desc.debugName = "beam.simple";
        desc.blend = PipelineDesc::BlendMode::Alpha;
        desc.depthWrite = false;
        desc.textured = true;
        break;
    case PipelineKind::BeamCylindrical:
        desc.debugName = "beam.cylinder";
        desc.blend = PipelineDesc::BlendMode::Alpha;
        desc.depthWrite = false;
        desc.textured = false;
        break;
    case PipelineKind::ParticleAlpha:
        desc.debugName = "particle.alpha";
        desc.blend = PipelineDesc::BlendMode::Alpha;
        desc.depthWrite = false;
        desc.textured = true;
        break;
    case PipelineKind::ParticleAdditive:
        desc.debugName = "particle.add";
        desc.blend = PipelineDesc::BlendMode::Additive;
        desc.depthWrite = false;
        desc.textured = true;
        break;
    case PipelineKind::Flare:
        desc.debugName = "flare";
        desc.blend = PipelineDesc::BlendMode::Additive;
        desc.depthTest = false;
        desc.depthWrite = false;
        desc.textured = true;
        break;
    case PipelineKind::DebugLineDepth:
        desc.debugName = "debug.line.depth";
        desc.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        desc.blend = PipelineDesc::BlendMode::Alpha;
        desc.depthWrite = false;
        desc.textured = false;
        break;
    case PipelineKind::DebugLineNoDepth:
        desc.debugName = "debug.line.nodepth";
        desc.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        desc.blend = PipelineDesc::BlendMode::Alpha;
        desc.depthTest = false;
        desc.depthWrite = false;
        desc.textured = false;
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

VulkanRenderer::ModelRecord *VulkanRenderer::findModelRecord(qhandle_t handle) {
    if (auto it = models_.find(handle); it != models_.end()) {
        return &it->second;
    }
    return nullptr;
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

uint32_t VulkanRenderer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
    if (physicalDevice_ == VK_NULL_HANDLE) {
        return UINT32_MAX;
    }

    VkPhysicalDeviceMemoryProperties memoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memoryProperties);

    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) == 0) {
            continue;
        }
        if ((memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    return UINT32_MAX;
}

bool VulkanRenderer::createBuffer(ModelRecord::BufferAllocationInfo &buffer,
                                  VkDeviceSize size,
                                  VkBufferUsageFlags usage,
                                  VkMemoryPropertyFlags properties) {
    if (device_ == VK_NULL_HANDLE || size == 0) {
        return false;
    }

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult bufferResult = vkCreateBuffer(device_, &bufferInfo, nullptr, &buffer.buffer);
    if (bufferResult != VK_SUCCESS || buffer.buffer == VK_NULL_HANDLE) {
        Com_Printf("refresh-vk: failed to create buffer (VkResult %d).\n", static_cast<int>(bufferResult));
        buffer.buffer = VK_NULL_HANDLE;
        return false;
    }

    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device_, buffer.buffer, &requirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = requirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(requirements.memoryTypeBits, properties);

    if (allocInfo.memoryTypeIndex == UINT32_MAX) {
        Com_Printf("refresh-vk: unable to find compatible memory type for buffer.\n");
        vkDestroyBuffer(device_, buffer.buffer, nullptr);
        buffer.buffer = VK_NULL_HANDLE;
        return false;
    }

    VkResult memoryResult = vkAllocateMemory(device_, &allocInfo, nullptr, &buffer.memory);
    if (memoryResult != VK_SUCCESS || buffer.memory == VK_NULL_HANDLE) {
        Com_Printf("refresh-vk: failed to allocate buffer memory (VkResult %d).\n", static_cast<int>(memoryResult));
        vkDestroyBuffer(device_, buffer.buffer, nullptr);
        buffer.buffer = VK_NULL_HANDLE;
        buffer.memory = VK_NULL_HANDLE;
        return false;
    }

    VkResult bindResult = vkBindBufferMemory(device_, buffer.buffer, buffer.memory, 0);
    if (bindResult != VK_SUCCESS) {
        Com_Printf("refresh-vk: failed to bind buffer memory (VkResult %d).\n", static_cast<int>(bindResult));
        vkDestroyBuffer(device_, buffer.buffer, nullptr);
        vkFreeMemory(device_, buffer.memory, nullptr);
        buffer.buffer = VK_NULL_HANDLE;
        buffer.memory = VK_NULL_HANDLE;
        return false;
    }

    buffer.offset = 0;
    buffer.size = size;
    return true;
}

void VulkanRenderer::destroyBuffer(ModelRecord::BufferAllocationInfo &buffer) {
    if (device_ == VK_NULL_HANDLE) {
        buffer = {};
        return;
    }

    if (buffer.buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, buffer.buffer, nullptr);
        buffer.buffer = VK_NULL_HANDLE;
    }

    if (buffer.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device_, buffer.memory, nullptr);
        buffer.memory = VK_NULL_HANDLE;
    }

    buffer.offset = 0;
    buffer.size = 0;
}

bool VulkanRenderer::copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) {
    if (device_ == VK_NULL_HANDLE || commandPool_ == VK_NULL_HANDLE || graphicsQueue_ == VK_NULL_HANDLE) {
        return false;
    }

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool_;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VkResult allocResult = vkAllocateCommandBuffers(device_, &allocInfo, &commandBuffer);
    if (allocResult != VK_SUCCESS || commandBuffer == VK_NULL_HANDLE) {
        Com_Printf("refresh-vk: failed to allocate command buffer for copy (VkResult %d).\n", static_cast<int>(allocResult));
        return false;
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VkResult beginResult = vkBeginCommandBuffer(commandBuffer, &beginInfo);
    if (beginResult != VK_SUCCESS) {
        Com_Printf("refresh-vk: failed to begin copy command buffer (VkResult %d).\n", static_cast<int>(beginResult));
        vkFreeCommandBuffers(device_, commandPool_, 1, &commandBuffer);
        return false;
    }

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, src, dst, 1, &copyRegion);

    VkResult endResult = vkEndCommandBuffer(commandBuffer);
    if (endResult != VK_SUCCESS) {
        Com_Printf("refresh-vk: failed to end copy command buffer (VkResult %d).\n", static_cast<int>(endResult));
        vkFreeCommandBuffers(device_, commandPool_, 1, &commandBuffer);
        return false;
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkResult submitResult = vkQueueSubmit(graphicsQueue_, 1, &submitInfo, VK_NULL_HANDLE);
    if (submitResult != VK_SUCCESS) {
        Com_Printf("refresh-vk: failed to submit buffer copy (VkResult %d).\n", static_cast<int>(submitResult));
        vkFreeCommandBuffers(device_, commandPool_, 1, &commandBuffer);
        return false;
    }

    VkResult waitResult = vkQueueWaitIdle(graphicsQueue_);
    if (waitResult != VK_SUCCESS) {
        Com_Printf("refresh-vk: queue wait failed after buffer copy (VkResult %d).\n", static_cast<int>(waitResult));
        vkFreeCommandBuffers(device_, commandPool_, 1, &commandBuffer);
        return false;
    }

    vkFreeCommandBuffers(device_, commandPool_, 1, &commandBuffer);
    return true;
}

bool VulkanRenderer::createModelDescriptorResources() {
    if (device_ == VK_NULL_HANDLE) {
        return false;
    }

    if (modelDescriptorSetLayout_ == VK_NULL_HANDLE) {
        VkDescriptorSetLayoutBinding vertexBinding{};
        vertexBinding.binding = 0;
        vertexBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        vertexBinding.descriptorCount = 1;
        vertexBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutBinding indexBinding = vertexBinding;
        indexBinding.binding = 1;

        std::array<VkDescriptorSetLayoutBinding, 2> bindings{vertexBinding, indexBinding};

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        VkResult layoutResult = vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr, &modelDescriptorSetLayout_);
        if (layoutResult != VK_SUCCESS || modelDescriptorSetLayout_ == VK_NULL_HANDLE) {
            Com_Printf("refresh-vk: failed to create model descriptor set layout (VkResult %d).\n", static_cast<int>(layoutResult));
            modelDescriptorSetLayout_ = VK_NULL_HANDLE;
            return false;
        }
    }

    if (modelPipelineLayout_ == VK_NULL_HANDLE) {
        VkPipelineLayoutCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineInfo.setLayoutCount = 1;
        pipelineInfo.pSetLayouts = &modelDescriptorSetLayout_;

        VkResult pipelineResult = vkCreatePipelineLayout(device_, &pipelineInfo, nullptr, &modelPipelineLayout_);
        if (pipelineResult != VK_SUCCESS || modelPipelineLayout_ == VK_NULL_HANDLE) {
            Com_Printf("refresh-vk: failed to create model pipeline layout (VkResult %d).\n", static_cast<int>(pipelineResult));
            if (modelPipelineLayout_ != VK_NULL_HANDLE) {
                vkDestroyPipelineLayout(device_, modelPipelineLayout_, nullptr);
                modelPipelineLayout_ = VK_NULL_HANDLE;
            }
            if (modelDescriptorSetLayout_ != VK_NULL_HANDLE) {
                vkDestroyDescriptorSetLayout(device_, modelDescriptorSetLayout_, nullptr);
                modelDescriptorSetLayout_ = VK_NULL_HANDLE;
            }
            return false;
        }
    }

    return true;
}

void VulkanRenderer::destroyModelDescriptorResources() {
    if (device_ == VK_NULL_HANDLE) {
        modelPipelineLayout_ = VK_NULL_HANDLE;
        modelDescriptorSetLayout_ = VK_NULL_HANDLE;
        return;
    }

    if (modelPipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, modelPipelineLayout_, nullptr);
        modelPipelineLayout_ = VK_NULL_HANDLE;
    }

    if (modelDescriptorSetLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, modelDescriptorSetLayout_, nullptr);
        modelDescriptorSetLayout_ = VK_NULL_HANDLE;
    }
}

bool VulkanRenderer::uploadMeshGeometry(ModelRecord::MeshGeometry &geometry) {
    if (device_ == VK_NULL_HANDLE) {
        return false;
    }

    const bool hasVertexData = !geometry.vertexStaging.empty();
    const bool hasIndexData = !geometry.indexStaging.empty();

    if (!hasVertexData && !hasIndexData) {
        geometry.uploaded = true;
        return true;
    }

    ModelRecord::BufferAllocationInfo newVertex{};
    ModelRecord::BufferAllocationInfo newIndex{};

    if (hasVertexData) {
        ModelRecord::BufferAllocationInfo staging{};
        VkDeviceSize vertexSize = static_cast<VkDeviceSize>(geometry.vertexStaging.size());
        if (!createBuffer(staging,
                          vertexSize,
                          VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            return false;
        }

        void *mapped = nullptr;
        VkResult mapResult = vkMapMemory(device_, staging.memory, staging.offset, staging.size, 0, &mapped);
        if (mapResult != VK_SUCCESS || mapped == nullptr) {
            Com_Printf("refresh-vk: failed to map vertex staging memory (VkResult %d).\n", static_cast<int>(mapResult));
            destroyBuffer(staging);
            return false;
        }

        std::memcpy(mapped, geometry.vertexStaging.data(), geometry.vertexStaging.size());
        vkUnmapMemory(device_, staging.memory);

        if (!createBuffer(newVertex,
                          vertexSize,
                          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            destroyBuffer(staging);
            return false;
        }

        if (!copyBuffer(staging.buffer, newVertex.buffer, newVertex.size)) {
            destroyBuffer(staging);
            destroyBuffer(newVertex);
            return false;
        }

        destroyBuffer(staging);
    }

    if (hasIndexData) {
        ModelRecord::BufferAllocationInfo staging{};
        VkDeviceSize indexSize = static_cast<VkDeviceSize>(geometry.indexStaging.size());
        if (!createBuffer(staging,
                          indexSize,
                          VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            if (hasVertexData) {
                destroyBuffer(newVertex);
            }
            return false;
        }

        void *mapped = nullptr;
        VkResult mapResult = vkMapMemory(device_, staging.memory, staging.offset, staging.size, 0, &mapped);
        if (mapResult != VK_SUCCESS || mapped == nullptr) {
            Com_Printf("refresh-vk: failed to map index staging memory (VkResult %d).\n", static_cast<int>(mapResult));
            destroyBuffer(staging);
            if (hasVertexData) {
                destroyBuffer(newVertex);
            }
            return false;
        }

        std::memcpy(mapped, geometry.indexStaging.data(), geometry.indexStaging.size());
        vkUnmapMemory(device_, staging.memory);

        if (!createBuffer(newIndex,
                          indexSize,
                          VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            destroyBuffer(staging);
            if (hasVertexData) {
                destroyBuffer(newVertex);
            }
            return false;
        }

        if (!copyBuffer(staging.buffer, newIndex.buffer, newIndex.size)) {
            destroyBuffer(staging);
            destroyBuffer(newIndex);
            if (hasVertexData) {
                destroyBuffer(newVertex);
            }
            return false;
        }

        destroyBuffer(staging);
    }

    if (hasVertexData) {
        destroyBuffer(geometry.vertex);
        geometry.vertex = newVertex;
        geometry.descriptor.vertex.buffer = geometry.vertex.buffer;
        geometry.descriptor.vertex.offset = geometry.vertex.offset;
        geometry.descriptor.vertex.range = geometry.vertex.size;
    }

    if (hasIndexData) {
        destroyBuffer(geometry.index);
        geometry.index = newIndex;
        geometry.descriptor.index.buffer = geometry.index.buffer;
        geometry.descriptor.index.offset = geometry.index.offset;
        geometry.descriptor.index.range = geometry.index.size;
    }

    if ((geometry.vertex.buffer != VK_NULL_HANDLE || geometry.index.buffer != VK_NULL_HANDLE) &&
        descriptorPool_ != VK_NULL_HANDLE) {
        if (geometry.descriptor.set == VK_NULL_HANDLE) {
            if (createModelDescriptorResources()) {
                VkDescriptorSetAllocateInfo allocInfo{};
                allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                allocInfo.descriptorPool = descriptorPool_;
                allocInfo.descriptorSetCount = 1;
                allocInfo.pSetLayouts = &modelDescriptorSetLayout_;

                VkResult allocResult = vkAllocateDescriptorSets(device_, &allocInfo, &geometry.descriptor.set);
                if (allocResult != VK_SUCCESS) {
                    Com_Printf("refresh-vk: failed to allocate model descriptor set (VkResult %d).\n", static_cast<int>(allocResult));
                    geometry.descriptor.set = VK_NULL_HANDLE;
                }
            }
        }

        if (geometry.descriptor.set != VK_NULL_HANDLE) {
            std::array<VkWriteDescriptorSet, 2> writes{};
            uint32_t writeCount = 0;

            if (geometry.vertex.buffer != VK_NULL_HANDLE && geometry.vertex.size > 0) {
                geometry.descriptor.vertex.buffer = geometry.vertex.buffer;
                geometry.descriptor.vertex.offset = geometry.vertex.offset;
                geometry.descriptor.vertex.range = geometry.vertex.size;

                VkWriteDescriptorSet &write = writes[writeCount++];
                write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                write.dstSet = geometry.descriptor.set;
                write.dstBinding = 0;
                write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                write.descriptorCount = 1;
                write.pBufferInfo = &geometry.descriptor.vertex;
            } else {
                geometry.descriptor.vertex = {};
            }

            if (geometry.index.buffer != VK_NULL_HANDLE && geometry.index.size > 0) {
                geometry.descriptor.index.buffer = geometry.index.buffer;
                geometry.descriptor.index.offset = geometry.index.offset;
                geometry.descriptor.index.range = geometry.index.size;

                VkWriteDescriptorSet &write = writes[writeCount++];
                write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                write.dstSet = geometry.descriptor.set;
                write.dstBinding = 1;
                write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                write.descriptorCount = 1;
                write.pBufferInfo = &geometry.descriptor.index;
            } else {
                geometry.descriptor.index = {};
            }

            if (writeCount > 0) {
                vkUpdateDescriptorSets(device_, writeCount, writes.data(), 0, nullptr);
            }
        }
    }

    if (hasVertexData) {
        geometry.vertexStaging.clear();
    }
    if (hasIndexData) {
        geometry.indexStaging.clear();
    }

    geometry.uploaded = true;
    return true;
}

void VulkanRenderer::destroyMeshGeometry(ModelRecord::MeshGeometry &geometry) {
    if (geometry.descriptor.set != VK_NULL_HANDLE && descriptorPool_ != VK_NULL_HANDLE && device_ != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(device_, descriptorPool_, 1, &geometry.descriptor.set);
    }
    geometry.descriptor.set = VK_NULL_HANDLE;
    geometry.descriptor.vertex = {};
    geometry.descriptor.index = {};

    destroyBuffer(geometry.vertex);
    destroyBuffer(geometry.index);

    geometry.vertexStaging.clear();
    geometry.indexStaging.clear();
    geometry.vertexCount = 0;
    geometry.indexCount = 0;
    geometry.indexType = VK_INDEX_TYPE_UINT16;
    geometry.uploaded = false;
}

void VulkanRenderer::destroyModelRecord(ModelRecord &record) {
    for (auto &geometry : record.meshGeometry) {
        destroyMeshGeometry(geometry);
    }
    record.meshGeometry.clear();
}

void VulkanRenderer::destroyAllModelGeometry() {
    for (auto &entry : models_) {
        destroyModelRecord(entry.second);
    }
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
            if (particle.color != -1) {
                billboard.color.u32 = d_8to24table[particle.color & 0xff];
            } else {
                billboard.color = particle.rgba;
            }
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

VulkanRenderer::ViewParameters VulkanRenderer::computeViewParameters(const refdef_t &fd) const {
    ViewParameters params{};
    vec3_t axis[3]{};
    AnglesToAxis(fd.viewangles, axis);
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            params.axis[i][j] = axis[i][j];
        }
    }
    params.origin = toArray(fd.vieworg);
    return params;
}

void VulkanRenderer::streamBeamPrimitives(const ViewParameters &view, bool cylindricalStyle) {
    effectStreams_.beamVertices.clear();
    effectStreams_.beamIndices.clear();

    if (framePrimitives_.beams.empty()) {
        return;
    }

    vec3_t viewAxis[3]{};
    for (int i = 0; i < 3; ++i) {
        arrayToVec3(view.axis[i], viewAxis[i]);
    }
    vec3_t viewOrigin{};
    arrayToVec3(view.origin, viewOrigin);

    for (const BeamPrimitive &beam : framePrimitives_.beams) {
        vec3_t start{};
        vec3_t end{};
        arrayToVec3(beam.start, start);
        arrayToVec3(beam.end, end);

        float radius = std::abs(beam.radius);
        if (!std::isfinite(radius) || radius <= 0.0f) {
            continue;
        }

        float widthScale = cylindricalStyle ? 0.5f : 1.2f;
        float width = radius * widthScale;
        if (!std::isfinite(width) || width <= 0.0f) {
            continue;
        }

        const std::array<float, 4> color = toColorArray(beam.color);

        vec3_t direction{};
        VectorSubtract(end, start, direction);
        if (VectorNormalize(direction) <= 0.0f) {
            continue;
        }

        if (cylindricalStyle) {
            size_t requiredVertices = static_cast<size_t>(kBeamCylinderSides) * 2;
            if (effectStreams_.beamVertices.size() + requiredVertices > std::numeric_limits<uint16_t>::max()) {
                continue;
            }

            uint16_t baseIndex = static_cast<uint16_t>(effectStreams_.beamVertices.size());

            vec3_t right{};
            vec3_t up{};
            MakeNormalVectors(direction, right, up);
            VectorScale(right, width, right);

            for (int i = 0; i < kBeamCylinderSides; ++i) {
                float angle = (360.0f / static_cast<float>(kBeamCylinderSides)) * static_cast<float>(i);
                vec3_t offset{};
                RotatePointAroundVector(offset, direction, right, angle);

                vec3_t startVertex{};
                VectorAdd(start, offset, startVertex);
                EffectVertexStreams::BeamVertex startBeam{};
                startBeam.position = toArray(startVertex);
                startBeam.uv = { 0.0f, 0.0f };
                startBeam.color = color;
                effectStreams_.beamVertices.emplace_back(startBeam);

                vec3_t endVertex{};
                VectorAdd(end, offset, endVertex);
                EffectVertexStreams::BeamVertex endBeam{};
                endBeam.position = toArray(endVertex);
                endBeam.uv = { 0.0f, 1.0f };
                endBeam.color = color;
                effectStreams_.beamVertices.emplace_back(endBeam);
            }

            for (int i = 0; i < kBeamCylinderSides; ++i) {
                uint16_t current = baseIndex + static_cast<uint16_t>(i * 2);
                uint16_t next = baseIndex + static_cast<uint16_t>(((i + 1) % kBeamCylinderSides) * 2);
                uint16_t currentTop = current + 1;
                uint16_t nextTop = next + 1;

                effectStreams_.beamIndices.push_back(current);
                effectStreams_.beamIndices.push_back(currentTop);
                effectStreams_.beamIndices.push_back(nextTop);

                effectStreams_.beamIndices.push_back(current);
                effectStreams_.beamIndices.push_back(nextTop);
                effectStreams_.beamIndices.push_back(next);
            }
        } else {
            if (effectStreams_.beamVertices.size() + 4 > std::numeric_limits<uint16_t>::max()) {
                continue;
            }

            vec3_t viewerToStart{};
            VectorSubtract(viewOrigin, start, viewerToStart);

            vec3_t right{};
            CrossProduct(direction, viewerToStart, right);
            if (VectorNormalize(right) <= 0.0f) {
                vec3_t tmp{};
                MakeNormalVectors(direction, right, tmp);
            }
            VectorScale(right, width, right);

            vec3_t v0{};
            vec3_t v1{};
            vec3_t v2{};
            vec3_t v3{};
            VectorAdd(start, right, v0);
            VectorSubtract(start, right, v1);
            VectorSubtract(end, right, v2);
            VectorAdd(end, right, v3);

            uint16_t baseIndex = static_cast<uint16_t>(effectStreams_.beamVertices.size());

            EffectVertexStreams::BeamVertex vertices[4]{};
            vertices[0].position = toArray(v0);
            vertices[0].uv = { 0.0f, 0.0f };
            vertices[0].color = color;

            vertices[1].position = toArray(v1);
            vertices[1].uv = { 1.0f, 0.0f };
            vertices[1].color = color;

            vertices[2].position = toArray(v2);
            vertices[2].uv = { 1.0f, 1.0f };
            vertices[2].color = color;

            vertices[3].position = toArray(v3);
            vertices[3].uv = { 0.0f, 1.0f };
            vertices[3].color = color;

            effectStreams_.beamVertices.insert(effectStreams_.beamVertices.end(), std::begin(vertices), std::end(vertices));

            effectStreams_.beamIndices.push_back(baseIndex + 0);
            effectStreams_.beamIndices.push_back(baseIndex + 2);
            effectStreams_.beamIndices.push_back(baseIndex + 3);
            effectStreams_.beamIndices.push_back(baseIndex + 0);
            effectStreams_.beamIndices.push_back(baseIndex + 1);
            effectStreams_.beamIndices.push_back(baseIndex + 2);
        }
    }
}

void VulkanRenderer::streamParticlePrimitives(const ViewParameters &view, bool additiveBlend) {
    (void)additiveBlend;

    effectStreams_.particleVertices.clear();

    if (framePrimitives_.particles.empty()) {
        return;
    }

    vec3_t viewAxis[3]{};
    for (int i = 0; i < 3; ++i) {
        arrayToVec3(view.axis[i], viewAxis[i]);
    }
    vec3_t viewOrigin{};
    arrayToVec3(view.origin, viewOrigin);

    float partScale = (gl_partscale) ? gl_partscale->value : 1.0f;

    for (const ParticleBillboard &billboard : framePrimitives_.particles) {
        vec3_t origin{};
        arrayToVec3(billboard.origin, origin);

        vec3_t toParticle{};
        VectorSubtract(origin, viewOrigin, toParticle);
        float dist = DotProduct(toParticle, viewAxis[0]);

        float scale = 1.0f;
        if (dist > kParticleDistanceBias) {
            scale += dist * kParticleDistanceScale;
        }
        scale *= partScale * billboard.scale;
        if (!std::isfinite(scale) || scale <= 0.0f) {
            continue;
        }

        float scale2 = scale * kParticleScale;

        vec3_t vertex0{};
        VectorMA(origin, scale2, viewAxis[1], vertex0);
        VectorMA(vertex0, -scale2, viewAxis[2], vertex0);

        vec3_t vertex1{};
        VectorMA(vertex0, scale, viewAxis[2], vertex1);

        vec3_t vertex2{};
        VectorMA(vertex0, -scale, viewAxis[1], vertex2);

        const std::array<float, 4> color = toColorArray(billboard.color, billboard.alpha);

        EffectVertexStreams::BillboardVertex v0{};
        v0.position = toArray(vertex0);
        v0.uv = { 0.0f, 0.0f };
        v0.color = color;

        EffectVertexStreams::BillboardVertex v1{};
        v1.position = toArray(vertex1);
        v1.uv = { 0.0f, kParticleSize };
        v1.color = color;

        EffectVertexStreams::BillboardVertex v2{};
        v2.position = toArray(vertex2);
        v2.uv = { kParticleSize, 0.0f };
        v2.color = color;

        effectStreams_.particleVertices.push_back(v0);
        effectStreams_.particleVertices.push_back(v1);
        effectStreams_.particleVertices.push_back(v2);
    }
}

void VulkanRenderer::streamFlarePrimitives(const ViewParameters &view) {
    effectStreams_.flareVertices.clear();
    effectStreams_.flareIndices.clear();

    if (framePrimitives_.flares.empty()) {
        return;
    }

    vec3_t viewAxis[3]{};
    for (int i = 0; i < 3; ++i) {
        arrayToVec3(view.axis[i], viewAxis[i]);
    }
    vec3_t viewOrigin{};
    arrayToVec3(view.origin, viewOrigin);

    for (const FlarePrimitive &flare : framePrimitives_.flares) {
        if (flare.scale <= 0.0f) {
            continue;
        }

        vec3_t origin{};
        arrayToVec3(flare.origin, origin);

        vec3_t dir{};
        VectorSubtract(origin, viewOrigin, dir);
        float dist = DotProduct(dir, viewAxis[0]);

        float scale = 2.5f;
        if (dist > kParticleDistanceBias) {
            scale += dist * kParticleDistanceScale;
        }
        scale *= flare.scale;
        if (!std::isfinite(scale) || scale <= 0.0f) {
            continue;
        }

        if (effectStreams_.flareVertices.size() + 4 > std::numeric_limits<uint16_t>::max()) {
            continue;
        }

        vec3_t left{};
        vec3_t right{};
        vec3_t up{};
        vec3_t down{};
        VectorScale(viewAxis[1], scale, left);
        VectorScale(viewAxis[1], -scale, right);
        VectorScale(viewAxis[2], scale, up);
        VectorScale(viewAxis[2], -scale, down);

        vec3_t v0{};
        vec3_t v1{};
        vec3_t v2{};
        vec3_t v3{};

        vec3_t temp{};
        VectorAdd(origin, down, temp);
        VectorAdd(temp, left, v0);

        VectorAdd(origin, up, temp);
        VectorAdd(temp, left, v1);

        VectorAdd(origin, down, temp);
        VectorAdd(temp, right, v2);

        VectorAdd(origin, up, temp);
        VectorAdd(temp, right, v3);

        uint16_t baseIndex = static_cast<uint16_t>(effectStreams_.flareVertices.size());

        const std::array<float, 4> color = toColorArray(flare.color);

        EffectVertexStreams::BillboardVertex vertices[4]{};
        vertices[0].position = toArray(v0);
        vertices[0].uv = { 0.0f, 1.0f };
        vertices[0].color = color;

        vertices[1].position = toArray(v1);
        vertices[1].uv = { 0.0f, 0.0f };
        vertices[1].color = color;

        vertices[2].position = toArray(v2);
        vertices[2].uv = { 1.0f, 1.0f };
        vertices[2].color = color;

        vertices[3].position = toArray(v3);
        vertices[3].uv = { 1.0f, 0.0f };
        vertices[3].color = color;

        effectStreams_.flareVertices.insert(effectStreams_.flareVertices.end(), std::begin(vertices), std::end(vertices));

        effectStreams_.flareIndices.push_back(baseIndex + 0);
        effectStreams_.flareIndices.push_back(baseIndex + 1);
        effectStreams_.flareIndices.push_back(baseIndex + 2);
        effectStreams_.flareIndices.push_back(baseIndex + 2);
        effectStreams_.flareIndices.push_back(baseIndex + 1);
        effectStreams_.flareIndices.push_back(baseIndex + 3);
    }
}

void VulkanRenderer::streamDebugLinePrimitives() {
    effectStreams_.debugLinesDepth.clear();
    effectStreams_.debugLinesNoDepth.clear();

    if (framePrimitives_.debugLines.empty()) {
        return;
    }

    for (const DebugLinePrimitive &line : framePrimitives_.debugLines) {
        const std::array<float, 4> color = toColorArray(line.color);

        EffectVertexStreams::DebugLineVertex start{};
        start.position = line.start;
        start.color = color;

        EffectVertexStreams::DebugLineVertex end{};
        end.position = line.end;
        end.color = color;

        if (line.depthTest) {
            effectStreams_.debugLinesDepth.push_back(start);
            effectStreams_.debugLinesDepth.push_back(end);
        } else {
            effectStreams_.debugLinesNoDepth.push_back(start);
            effectStreams_.debugLinesNoDepth.push_back(end);
        }
    }
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

bool VulkanRenderer::canSubmit2D() const {
    return frameActive_ && draw2d::isActive();
}

qhandle_t VulkanRenderer::ensureWhiteTexture() {
    if (whiteTextureHandle_ != 0) {
        if (images_.find(whiteTextureHandle_) != images_.end()) {
            return whiteTextureHandle_;
        }
        whiteTextureHandle_ = 0;
    }

    qhandle_t handle = nextHandle();
    whiteTextureHandle_ = handle;

    ImageRecord record{};
    record.handle = handle;
    record.name = "__vk_white";
    record.type = IT_PIC;
    record.flags = static_cast<imageflags_t>(IF_SPECIAL | IF_PERMANENT);
    record.width = 1;
    record.height = 1;
    record.transparent = false;
    record.registrationSequence = r_registration_sequence;

    images_[handle] = record;
    imageLookup_[record.name] = handle;
    return handle;
}

qhandle_t VulkanRenderer::ensureRawTexture() {
    if (rawTextureHandle_ != 0) {
        if (auto it = images_.find(rawTextureHandle_); it != images_.end()) {
            it->second.width = rawPic_.width;
            it->second.height = rawPic_.height;
            return rawTextureHandle_;
        }
        rawTextureHandle_ = 0;
    }

    qhandle_t handle = nextHandle();
    rawTextureHandle_ = handle;

    ImageRecord record{};
    record.handle = handle;
    record.name = "__vk_raw";
    record.type = IT_PIC;
    record.flags = IF_SPECIAL;
    record.width = rawPic_.width;
    record.height = rawPic_.height;
    record.transparent = true;
    record.registrationSequence = r_registration_sequence;

    images_[handle] = record;
    imageLookup_[record.name] = handle;
    return handle;
}

const std::vector<std::string> &VulkanRenderer::platformInstanceExtensions() const {
    return platformInstanceExtensions_;
}

VkSurfaceKHR VulkanRenderer::platformSurface() const {
    return platformSurface_;
}

bool VulkanRenderer::createPlatformSurface(VkInstance instance, const VkAllocationCallbacks *allocator) {
    if (!platformHooks_.createSurface) {
        Com_Printf("refresh-vk: platform does not support Vulkan surface creation.\n");
        return false;
    }

    destroyPlatformSurface(VK_NULL_HANDLE, allocator);

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkResult result = platformHooks_.createSurface(instance, allocator, &surface);
    if (result != VK_SUCCESS || surface == VK_NULL_HANDLE) {
        Com_Printf("refresh-vk: failed to create Vulkan surface (VkResult %d).\n", static_cast<int>(result));
        return false;
    }

    platformSurface_ = surface;
    platformInstance_ = instance;
    return true;
}

void VulkanRenderer::destroyPlatformSurface(VkInstance instance, const VkAllocationCallbacks *allocator) {
    if (platformSurface_ == VK_NULL_HANDLE) {
        return;
    }

    VkInstance destroyInstance = instance;
    if (destroyInstance == VK_NULL_HANDLE) {
        destroyInstance = platformInstance_;
    }

    if (platformHooks_.destroySurface && destroyInstance != VK_NULL_HANDLE) {
        platformHooks_.destroySurface(destroyInstance, platformSurface_, allocator);
    }

    platformSurface_ = VK_NULL_HANDLE;
    platformInstance_ = VK_NULL_HANDLE;
}

void VulkanRenderer::initializePlatformHooks() {
    platformHooks_ = {};
    platformInstance_ = VK_NULL_HANDLE;
    platformSurface_ = VK_NULL_HANDLE;

    if (vid) {
        platformHooks_.getInstanceExtensions = vid->vk.get_instance_extensions;
        platformHooks_.createSurface = vid->vk.create_surface;
        platformHooks_.destroySurface = vid->vk.destroy_surface;
    }

    collectPlatformInstanceExtensions();
}

void VulkanRenderer::collectPlatformInstanceExtensions() {
    platformInstanceExtensions_.clear();

    std::unordered_set<std::string> seen;
    auto addExtension = [&](const char *name) {
        if (!name || !*name) {
            return;
        }
        if (seen.insert(name).second) {
            platformInstanceExtensions_.emplace_back(name);
        }
    };

    addExtension(VK_KHR_SURFACE_EXTENSION_NAME);

    if (!platformHooks_.getInstanceExtensions) {
        return;
    }

    uint32_t count = 0;
    const char *const *extensions = platformHooks_.getInstanceExtensions(&count);
    if (!extensions || count == 0) {
        return;
    }

    platformInstanceExtensions_.reserve(platformInstanceExtensions_.size() + count);
    for (uint32_t i = 0; i < count; ++i) {
        addExtension(extensions[i]);
    }
}

bool VulkanRenderer::createInstance() {
    if (instance_ != VK_NULL_HANDLE) {
        return true;
    }

    std::vector<const char *> extensions;
    extensions.reserve(platformInstanceExtensions_.size());
    for (const std::string &extension : platformInstanceExtensions_) {
        extensions.push_back(extension.c_str());
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "WORR";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "refresh-vk";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo instanceInfo{};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &appInfo;
    instanceInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    instanceInfo.ppEnabledExtensionNames = extensions.empty() ? nullptr : extensions.data();

    VkResult result = vkCreateInstance(&instanceInfo, nullptr, &instance_);
    if (result != VK_SUCCESS || instance_ == VK_NULL_HANDLE) {
        Com_Printf("refresh-vk: failed to create Vulkan instance (VkResult %d).\n", static_cast<int>(result));
        instance_ = VK_NULL_HANDLE;
        return false;
    }

    return true;
}

void VulkanRenderer::destroyInstance() {
    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }
}

VulkanRenderer::QueueFamilyIndices VulkanRenderer::queryQueueFamilies(VkPhysicalDevice device) const {
    QueueFamilyIndices indices{};

    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> properties(count);
    if (count > 0) {
        vkGetPhysicalDeviceQueueFamilyProperties(device, &count, properties.data());
    }

    for (uint32_t i = 0; i < count; ++i) {
        if (!indices.graphics && (properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            indices.graphics = i;
        }

        if (!indices.present && platformSurface_ != VK_NULL_HANDLE) {
            VkBool32 supported = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, platformSurface_, &supported);
            if (supported) {
                indices.present = i;
            }
        }

        if (indices.complete()) {
            break;
        }
    }

    return indices;
}

VulkanRenderer::SwapchainSupportDetails VulkanRenderer::querySwapchainSupport(VkPhysicalDevice device) const {
    SwapchainSupportDetails details{};
    if (platformSurface_ == VK_NULL_HANDLE) {
        return details;
    }

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, platformSurface_, &details.capabilities);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, platformSurface_, &formatCount, nullptr);
    if (formatCount > 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, platformSurface_, &formatCount, details.formats.data());
    }

    uint32_t presentCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, platformSurface_, &presentCount, nullptr);
    if (presentCount > 0) {
        details.presentModes.resize(presentCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, platformSurface_, &presentCount, details.presentModes.data());
    }

    return details;
}

VkSurfaceFormatKHR VulkanRenderer::chooseSwapchainFormat(const std::vector<VkSurfaceFormatKHR> &formats) const {
    for (const VkSurfaceFormatKHR &format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }

    if (!formats.empty()) {
        return formats.front();
    }

    VkSurfaceFormatKHR fallback{};
    fallback.format = VK_FORMAT_B8G8R8A8_UNORM;
    fallback.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    return fallback;
}

VkPresentModeKHR VulkanRenderer::choosePresentMode(const std::vector<VkPresentModeKHR> &presentModes) const {
    for (VkPresentModeKHR mode : presentModes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return mode;
        }
    }

    for (VkPresentModeKHR mode : presentModes) {
        if (mode == VK_PRESENT_MODE_FIFO_KHR) {
            return mode;
        }
    }

    if (!presentModes.empty()) {
        return presentModes.front();
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanRenderer::chooseSwapchainExtent(const VkSurfaceCapabilitiesKHR &capabilities) const {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    VkExtent2D extent{};
    extent.width = static_cast<uint32_t>(std::max(1, r_config.width));
    extent.height = static_cast<uint32_t>(std::max(1, r_config.height));

    extent.width = std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    extent.height = std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    return extent;
}

bool VulkanRenderer::pickPhysicalDevice() {
    if (instance_ == VK_NULL_HANDLE) {
        return false;
    }

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);
    if (deviceCount == 0) {
        Com_Printf("refresh-vk: no Vulkan physical devices available.\n");
        return false;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());

    for (VkPhysicalDevice candidate : devices) {
        QueueFamilyIndices indices = queryQueueFamilies(candidate);
        if (!indices.complete()) {
            continue;
        }

        uint32_t extensionCount = 0;
        vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> extensions(extensionCount);
        if (extensionCount > 0) {
            vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extensionCount, extensions.data());
        }

        bool hasSwapchain = false;
        for (const VkExtensionProperties &prop : extensions) {
            if (std::strcmp(prop.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
                hasSwapchain = true;
                break;
            }
        }

        if (!hasSwapchain) {
            continue;
        }

        SwapchainSupportDetails support = querySwapchainSupport(candidate);
        if (support.formats.empty() || support.presentModes.empty()) {
            continue;
        }

        physicalDevice_ = candidate;
        graphicsQueueFamily_ = indices.graphics.value();
        presentQueueFamily_ = indices.present.value();
        return true;
    }

    Com_Printf("refresh-vk: failed to find suitable Vulkan physical device.\n");
    return false;
}

bool VulkanRenderer::createLogicalDevice() {
    if (physicalDevice_ == VK_NULL_HANDLE) {
        return false;
    }

    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    std::vector<uint32_t> uniqueFamilies;
    uniqueFamilies.push_back(graphicsQueueFamily_);
    if (presentQueueFamily_ != graphicsQueueFamily_) {
        uniqueFamilies.push_back(presentQueueFamily_);
    }

    float priority = 1.0f;
    for (uint32_t family : uniqueFamilies) {
        VkDeviceQueueCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        info.queueFamilyIndex = family;
        info.queueCount = 1;
        info.pQueuePriorities = &priority;
        queueInfos.push_back(info);
    }

    VkPhysicalDeviceFeatures features{};

    const char *extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkDeviceCreateInfo deviceInfo{};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
    deviceInfo.pQueueCreateInfos = queueInfos.data();
    deviceInfo.pEnabledFeatures = &features;
    deviceInfo.enabledExtensionCount = static_cast<uint32_t>(sizeof(extensions) / sizeof(extensions[0]));
    deviceInfo.ppEnabledExtensionNames = extensions;

    VkResult result = vkCreateDevice(physicalDevice_, &deviceInfo, nullptr, &device_);
    if (result != VK_SUCCESS || device_ == VK_NULL_HANDLE) {
        Com_Printf("refresh-vk: failed to create logical device (VkResult %d).\n", static_cast<int>(result));
        device_ = VK_NULL_HANDLE;
        return false;
    }

    vkGetDeviceQueue(device_, graphicsQueueFamily_, 0, &graphicsQueue_);
    vkGetDeviceQueue(device_, presentQueueFamily_, 0, &presentQueue_);

    return graphicsQueue_ != VK_NULL_HANDLE && presentQueue_ != VK_NULL_HANDLE;
}

bool VulkanRenderer::createCommandPool() {
    if (device_ == VK_NULL_HANDLE) {
        return false;
    }

    if (commandPool_ != VK_NULL_HANDLE) {
        return true;
    }

    VkCommandPoolCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.queueFamilyIndex = graphicsQueueFamily_;
    info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VkResult result = vkCreateCommandPool(device_, &info, nullptr, &commandPool_);
    if (result != VK_SUCCESS || commandPool_ == VK_NULL_HANDLE) {
        Com_Printf("refresh-vk: failed to create command pool (VkResult %d).\n", static_cast<int>(result));
        commandPool_ = VK_NULL_HANDLE;
        return false;
    }

    return true;
}

bool VulkanRenderer::createSwapchainResources(VkSwapchainKHR oldSwapchain) {
    if (device_ == VK_NULL_HANDLE || platformSurface_ == VK_NULL_HANDLE) {
        return false;
    }

    SwapchainSupportDetails support = querySwapchainSupport(physicalDevice_);
    if (support.formats.empty() || support.presentModes.empty()) {
        Com_Printf("refresh-vk: swapchain support incomplete.\n");
        return false;
    }

    VkSurfaceFormatKHR surfaceFormat = chooseSwapchainFormat(support.formats);
    VkPresentModeKHR presentMode = choosePresentMode(support.presentModes);
    VkExtent2D extent = chooseSwapchainExtent(support.capabilities);

    uint32_t imageCount = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0 && imageCount > support.capabilities.maxImageCount) {
        imageCount = support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapchainInfo{};
    swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.surface = platformSurface_;
    swapchainInfo.minImageCount = imageCount;
    swapchainInfo.imageFormat = surfaceFormat.format;
    swapchainInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapchainInfo.imageExtent = extent;
    swapchainInfo.imageArrayLayers = 1;
    swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queueFamilyIndices[] = { graphicsQueueFamily_, presentQueueFamily_ };
    if (graphicsQueueFamily_ != presentQueueFamily_) {
        swapchainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchainInfo.queueFamilyIndexCount = 2;
        swapchainInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchainInfo.queueFamilyIndexCount = 0;
        swapchainInfo.pQueueFamilyIndices = nullptr;
    }

    swapchainInfo.preTransform = support.capabilities.currentTransform;
    swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainInfo.presentMode = presentMode;
    swapchainInfo.clipped = VK_TRUE;
    swapchainInfo.oldSwapchain = oldSwapchain;

    VkResult result = vkCreateSwapchainKHR(device_, &swapchainInfo, nullptr, &swapchain_);
    if (result != VK_SUCCESS || swapchain_ == VK_NULL_HANDLE) {
        Com_Printf("refresh-vk: failed to create swapchain (VkResult %d).\n", static_cast<int>(result));
        swapchain_ = VK_NULL_HANDLE;
        return false;
    }

    vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, nullptr);
    swapchainImages_.resize(imageCount);
    vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, swapchainImages_.data());

    swapchainFormat_ = surfaceFormat.format;
    swapchainExtent_ = extent;

    swapchainImageViews_.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; ++i) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = swapchainImages_[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = swapchainFormat_;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VkResult viewResult = vkCreateImageView(device_, &viewInfo, nullptr, &swapchainImageViews_[i]);
        if (viewResult != VK_SUCCESS || swapchainImageViews_[i] == VK_NULL_HANDLE) {
            Com_Printf("refresh-vk: failed to create swapchain image view (VkResult %d).\n", static_cast<int>(viewResult));
            return false;
        }
    }

    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchainFormat_;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    VkResult renderPassResult = vkCreateRenderPass(device_, &renderPassInfo, nullptr, &renderPass_);
    if (renderPassResult != VK_SUCCESS || renderPass_ == VK_NULL_HANDLE) {
        Com_Printf("refresh-vk: failed to create render pass (VkResult %d).\n", static_cast<int>(renderPassResult));
        return false;
    }

    swapchainFramebuffers_.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; ++i) {
        VkImageView attachments[] = { swapchainImageViews_[i] };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass_;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapchainExtent_.width;
        framebufferInfo.height = swapchainExtent_.height;
        framebufferInfo.layers = 1;

        VkResult framebufferResult = vkCreateFramebuffer(device_, &framebufferInfo, nullptr, &swapchainFramebuffers_[i]);
        if (framebufferResult != VK_SUCCESS || swapchainFramebuffers_[i] == VK_NULL_HANDLE) {
            Com_Printf("refresh-vk: failed to create framebuffer (VkResult %d).\n", static_cast<int>(framebufferResult));
            return false;
        }
    }

    imagesInFlight_.assign(imageCount, VK_NULL_HANDLE);
    vsyncEnabled_ = (presentMode == VK_PRESENT_MODE_FIFO_KHR || presentMode == VK_PRESENT_MODE_FIFO_RELAXED_KHR);

    return true;
}

bool VulkanRenderer::createDescriptorPool() {
    if (device_ == VK_NULL_HANDLE) {
        return false;
    }

    if (descriptorPool_ != VK_NULL_HANDLE) {
        return true;
    }

    std::array<VkDescriptorPoolSize, 3> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 128;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 256;
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[2].descriptorCount = 64;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 512;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    VkResult result = vkCreateDescriptorPool(device_, &poolInfo, nullptr, &descriptorPool_);
    if (result != VK_SUCCESS || descriptorPool_ == VK_NULL_HANDLE) {
        Com_Printf("refresh-vk: failed to create descriptor pool (VkResult %d).\n", static_cast<int>(result));
        descriptorPool_ = VK_NULL_HANDLE;
        return false;
    }

    return true;
}

bool VulkanRenderer::createSyncObjects() {
    if (device_ == VK_NULL_HANDLE) {
        return false;
    }

    inFlightFrames_.resize(kMaxFramesInFlight);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (InFlightFrame &frame : inFlightFrames_) {
        VkResult semaphoreResult = vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &frame.imageAvailable);
        if (semaphoreResult != VK_SUCCESS || frame.imageAvailable == VK_NULL_HANDLE) {
            Com_Printf("refresh-vk: failed to create image-available semaphore (VkResult %d).\n", static_cast<int>(semaphoreResult));
            return false;
        }

        VkResult renderSemaphoreResult = vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &frame.renderFinished);
        if (renderSemaphoreResult != VK_SUCCESS || frame.renderFinished == VK_NULL_HANDLE) {
            Com_Printf("refresh-vk: failed to create render-finished semaphore (VkResult %d).\n", static_cast<int>(renderSemaphoreResult));
            return false;
        }

        VkResult fenceResult = vkCreateFence(device_, &fenceInfo, nullptr, &frame.inFlight);
        if (fenceResult != VK_SUCCESS || frame.inFlight == VK_NULL_HANDLE) {
            Com_Printf("refresh-vk: failed to create in-flight fence (VkResult %d).\n", static_cast<int>(fenceResult));
            return false;
        }

        frame.commandBuffer = VK_NULL_HANDLE;
        frame.imageIndex = 0;
        frame.hasImage = false;
    }

    if (!inFlightFrames_.empty()) {
        std::vector<VkCommandBuffer> buffers(inFlightFrames_.size(), VK_NULL_HANDLE);
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool_;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = static_cast<uint32_t>(buffers.size());

        VkResult allocResult = vkAllocateCommandBuffers(device_, &allocInfo, buffers.data());
        if (allocResult != VK_SUCCESS) {
            Com_Printf("refresh-vk: failed to allocate command buffers (VkResult %d).\n", static_cast<int>(allocResult));
            return false;
        }

        for (size_t i = 0; i < inFlightFrames_.size(); ++i) {
            inFlightFrames_[i].commandBuffer = buffers[i];
        }
    }

    currentFrameIndex_ = 0;
    return true;
}

bool VulkanRenderer::createDeviceResources() {
    if (!pickPhysicalDevice()) {
        return false;
    }

    if (!createLogicalDevice()) {
        return false;
    }

    if (!createCommandPool()) {
        return false;
    }

    if (!createSwapchainResources()) {
        return false;
    }

    if (!createDescriptorPool()) {
        return false;
    }

    if (!createModelDescriptorResources()) {
        return false;
    }

    if (!createSyncObjects()) {
        destroySwapchainResources();
        return false;
    }

    return true;
}

void VulkanRenderer::destroySyncObjects() {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }

    for (InFlightFrame &frame : inFlightFrames_) {
        if (frame.commandBuffer != VK_NULL_HANDLE && commandPool_ != VK_NULL_HANDLE) {
            vkFreeCommandBuffers(device_, commandPool_, 1, &frame.commandBuffer);
            frame.commandBuffer = VK_NULL_HANDLE;
        } else {
            frame.commandBuffer = VK_NULL_HANDLE;
        }
        if (frame.imageAvailable != VK_NULL_HANDLE) {
            vkDestroySemaphore(device_, frame.imageAvailable, nullptr);
            frame.imageAvailable = VK_NULL_HANDLE;
        }
        if (frame.renderFinished != VK_NULL_HANDLE) {
            vkDestroySemaphore(device_, frame.renderFinished, nullptr);
            frame.renderFinished = VK_NULL_HANDLE;
        }
        if (frame.inFlight != VK_NULL_HANDLE) {
            vkDestroyFence(device_, frame.inFlight, nullptr);
            frame.inFlight = VK_NULL_HANDLE;
        }
        frame.hasImage = false;
    }

    inFlightFrames_.clear();
}

void VulkanRenderer::destroySwapchainResources() {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }

    destroySyncObjects();

    for (VkFramebuffer framebuffer : swapchainFramebuffers_) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device_, framebuffer, nullptr);
        }
    }
    swapchainFramebuffers_.clear();

    if (renderPass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device_, renderPass_, nullptr);
        renderPass_ = VK_NULL_HANDLE;
    }

    for (VkImageView view : swapchainImageViews_) {
        if (view != VK_NULL_HANDLE) {
            vkDestroyImageView(device_, view, nullptr);
        }
    }
    swapchainImageViews_.clear();

    if (swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }

    swapchainImages_.clear();
    imagesInFlight_.clear();
    frameAcquired_ = false;
    swapchainExtent_ = { 0u, 0u };
    swapchainFormat_ = VK_FORMAT_UNDEFINED;
    vsyncEnabled_ = true;
}

void VulkanRenderer::destroyDescriptorPool() {
    if (descriptorPool_ != VK_NULL_HANDLE && device_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
        descriptorPool_ = VK_NULL_HANDLE;
    }
}

void VulkanRenderer::destroyCommandPool() {
    if (commandPool_ != VK_NULL_HANDLE && device_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device_, commandPool_, nullptr);
        commandPool_ = VK_NULL_HANDLE;
    }
}

void VulkanRenderer::destroyDeviceResources() {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }

    vkDeviceWaitIdle(device_);
    destroyAllModelGeometry();
    destroySwapchainResources();
    destroyModelDescriptorResources();
    destroyDescriptorPool();
    destroyCommandPool();

    if (device_ != VK_NULL_HANDLE) {
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }

    graphicsQueue_ = VK_NULL_HANDLE;
    presentQueue_ = VK_NULL_HANDLE;
    graphicsQueueFamily_ = VK_QUEUE_FAMILY_IGNORED;
    presentQueueFamily_ = VK_QUEUE_FAMILY_IGNORED;
    physicalDevice_ = VK_NULL_HANDLE;
}

bool VulkanRenderer::recreateSwapchain() {
    if (device_ == VK_NULL_HANDLE || swapchain_ == VK_NULL_HANDLE) {
        return false;
    }

    vkDeviceWaitIdle(device_);

    destroySyncObjects();

    for (VkFramebuffer framebuffer : swapchainFramebuffers_) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device_, framebuffer, nullptr);
        }
    }
    swapchainFramebuffers_.clear();

    for (VkImageView view : swapchainImageViews_) {
        if (view != VK_NULL_HANDLE) {
            vkDestroyImageView(device_, view, nullptr);
        }
    }
    swapchainImageViews_.clear();

    VkSwapchainKHR oldSwapchain = swapchain_;
    swapchain_ = VK_NULL_HANDLE;

    if (!createSwapchainResources(oldSwapchain)) {
        if (oldSwapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(device_, oldSwapchain, nullptr);
        }
        return false;
    }

    if (oldSwapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device_, oldSwapchain, nullptr);
    }

    if (!createSyncObjects()) {
        return false;
    }

    return true;
}

void VulkanRenderer::destroyVulkan() {
    destroyDeviceResources();
    destroyPlatformSurface(instance_, nullptr);
    destroyInstance();
}

void VulkanRenderer::finishFrameRecording() {
    if (!frameActive_ || !frameAcquired_) {
        return;
    }

    InFlightFrame &frame = inFlightFrames_[currentFrameIndex_];
    if (!frame.hasImage || frame.commandBuffer == VK_NULL_HANDLE) {
        return;
    }

    vkCmdEndRenderPass(frame.commandBuffer);
    VkResult endResult = vkEndCommandBuffer(frame.commandBuffer);
    if (endResult != VK_SUCCESS) {
        Com_Printf("refresh-vk: failed to end command buffer (VkResult %d).\n", static_cast<int>(endResult));
    }
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

    initializePlatformHooks();
    collectPlatformInstanceExtensions();

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

    if (!createInstance()) {
        draw2d::shutdown();
        Com_Printf("refresh-vk: unable to create Vulkan instance.\n");
        return false;
    }

    if (!createPlatformSurface(instance_, nullptr)) {
        destroyVulkan();
        draw2d::shutdown();
        return false;
    }

    if (!createDeviceResources()) {
        destroyVulkan();
        draw2d::shutdown();
        return false;
    }

    if (!platformInstanceExtensions_.empty()) {
        Com_DPrintf("refresh-vk: required platform instance extensions:\n");
        for (const std::string &extension : platformInstanceExtensions_) {
            Com_DPrintf("    %s\n", extension.c_str());
        }
    }

    initialized_ = true;
    r_registration_sequence = 1;

    Com_Printf("refresh-vk initialized.\n");
    Com_Printf("------------------------------\n");

    return true;
}

void VulkanRenderer::shutdown(bool total) {
    if (!initialized_) {
        destroyVulkan();
        draw2d::shutdown();
        platformInstanceExtensions_.clear();
        platformHooks_ = {};
        platformInstance_ = VK_NULL_HANDLE;
        return;
    }

    if (frameActive_) {
        draw2d::end();
        finishFrameRecording();
        frameActive_ = false;
    }

    frameAcquired_ = false;

    destroyVulkan();

    platformInstanceExtensions_.clear();
    platformHooks_ = {};
    platformInstance_ = VK_NULL_HANDLE;

    models_.clear();
    images_.clear();
    modelLookup_.clear();
    imageLookup_.clear();
    rawPic_.pixels.clear();
    currentMap_.clear();
    resetTransientState();
    draw2d::shutdown();

    if (total) {
        initialized_ = false;
    } else {
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

    if (name[0] == '*') {
        int inlineIndex = Q_atoi(name + 1);
        if (inlineIndex < 0) {
            return 0;
        }

        qhandle_t handle = ~inlineIndex;
        std::string key{name};
        auto [it, inserted] = modelLookup_.emplace(key, handle);
        if (!inserted) {
            handle = it->second;
        }

        auto &record = models_[handle];
        record.handle = handle;
        record.name = name;
        record.registrationSequence = r_registration_sequence;
        record.type = 0;
        record.numFrames = 0;
        record.numMeshes = 0;
        record.inlineModel = true;
        record.aliasFrames.clear();
        record.spriteFrames.clear();
        destroyModelRecord(record);

        return handle;
    }

    qhandle_t handle = registerResource(modelLookup_, name);
    auto &record = models_[handle];
    record.handle = handle;
    record.name = name;
    record.registrationSequence = r_registration_sequence;
    record.inlineModel = false;
    record.aliasFrames.clear();
    record.spriteFrames.clear();
    destroyModelRecord(record);
    record.type = 0;
    record.numFrames = 0;
    record.numMeshes = 0;

    model_t *model = nullptr;
    char normalized[MAX_QPATH];
    size_t normalizedLength = FS_NormalizePathBuffer(normalized, name, MAX_QPATH);
    if (normalizedLength > 0 && normalizedLength < MAX_QPATH) {
        model = MOD_Find(normalized);
    }
    if (!model) {
        model = MOD_Find(name);
    }
    if (!model && handle > 0) {
        model_t *fromHandle = MOD_ForHandle(handle);
        if (fromHandle) {
            if (normalizedLength > 0 && normalizedLength < MAX_QPATH) {
                if (FS_pathcmp(fromHandle->name, normalized) == 0) {
                    model = fromHandle;
                }
            } else {
                model = fromHandle;
            }
        }
    }

    if (model) {
        record.type = model->type;
        record.numFrames = model->numframes;
        record.numMeshes = model->nummeshes;

        if (model->type == MOD_ALIAS && model->frames) {
            record.aliasFrames.reserve(static_cast<size_t>(model->numframes));
            for (int i = 0; i < model->numframes; ++i) {
                record.aliasFrames.emplace_back(makeAliasFrameMetadata(model->frames[i]));
            }
        } else if (model->type == MOD_SPRITE && model->spriteframes) {
            record.spriteFrames.reserve(static_cast<size_t>(model->numframes));
            for (int i = 0; i < model->numframes; ++i) {
                const mspriteframe_t &frame = model->spriteframes[i];
                ModelRecord::SpriteFrameMetadata metadata{};
                metadata.width = frame.width;
                metadata.height = frame.height;
                metadata.originX = frame.origin_x;
                metadata.originY = frame.origin_y;
                record.spriteFrames.emplace_back(metadata);
            }
        }

        allocateModelGeometry(record, *model);
    }

    return handle;
}

void VulkanRenderer::allocateModelGeometry(ModelRecord &record, const model_t &model) {
    destroyModelRecord(record);

    if (model.type != MOD_ALIAS) {
        return;
    }

    if (!model.meshes || model.nummeshes <= 0) {
        return;
    }

    record.meshGeometry.reserve(static_cast<size_t>(model.nummeshes));

    for (int meshIndex = 0; meshIndex < model.nummeshes; ++meshIndex) {
        const maliasmesh_t &mesh = model.meshes[meshIndex];
        ModelRecord::MeshGeometry geometry{};
        geometry.descriptor.binding = static_cast<uint32_t>(meshIndex);

        if (mesh.numverts > 0 && mesh.verts && mesh.tcoords) {
            const size_t vertexSize = static_cast<size_t>(mesh.numverts) * sizeof(*mesh.verts);
            const size_t texCoordSize = static_cast<size_t>(mesh.numverts) * sizeof(*mesh.tcoords);
            geometry.vertexStaging.resize(vertexSize + texCoordSize);
            std::memcpy(geometry.vertexStaging.data(), mesh.verts, vertexSize);
            std::memcpy(geometry.vertexStaging.data() + vertexSize, mesh.tcoords, texCoordSize);
            geometry.vertexCount = static_cast<size_t>(mesh.numverts);
        }

        if (mesh.numindices > 0 && mesh.indices) {
            const size_t indexSize = static_cast<size_t>(mesh.numindices) * sizeof(*mesh.indices);
            geometry.indexStaging.resize(indexSize);
            std::memcpy(geometry.indexStaging.data(), mesh.indices, indexSize);
            geometry.indexCount = static_cast<size_t>(mesh.numindices);
            geometry.indexType = VK_INDEX_TYPE_UINT16;
        }

        if (geometry.vertexCount == 0 && geometry.indexCount == 0) {
            continue;
        }

        if (!geometry.vertexStaging.empty() || !geometry.indexStaging.empty()) {
            if (device_ != VK_NULL_HANDLE && !uploadMeshGeometry(geometry)) {
                Com_Printf("refresh-vk: failed to upload mesh %d for model %s.\n",
                           meshIndex,
                           record.name.c_str());
            }
        }

        record.meshGeometry.emplace_back(std::move(geometry));
    }
}

void VulkanRenderer::bindModelGeometryBuffers(ModelRecord &record) {
    if (record.meshGeometry.empty()) {
        return;
    }

    if (inFlightFrames_.empty() || !frameActive_) {
        return;
    }

    InFlightFrame &frame = inFlightFrames_[currentFrameIndex_];
    VkCommandBuffer commandBuffer = frame.commandBuffer;
    if (commandBuffer == VK_NULL_HANDLE) {
        return;
    }

    for (size_t meshIndex = 0; meshIndex < record.meshGeometry.size(); ++meshIndex) {
        auto &geometry = record.meshGeometry[meshIndex];

        if (!geometry.uploaded && (!geometry.vertexStaging.empty() || !geometry.indexStaging.empty())) {
            if (!uploadMeshGeometry(geometry)) {
                continue;
            }
        }

        if (geometry.vertex.buffer == VK_NULL_HANDLE && geometry.index.buffer == VK_NULL_HANDLE) {
            continue;
        }

        std::string entry{"bind.model."};
        entry.append(record.name);
        entry.push_back('#');
        entry.append(std::to_string(meshIndex));
        commandLog_.push_back(std::move(entry));

        if (geometry.vertex.buffer != VK_NULL_HANDLE && geometry.vertex.size > 0) {
            VkBuffer buffers[] = { geometry.vertex.buffer };
            VkDeviceSize offsets[] = { geometry.vertex.offset };
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);
        }

        if (geometry.index.buffer != VK_NULL_HANDLE && geometry.index.size > 0) {
            vkCmdBindIndexBuffer(commandBuffer, geometry.index.buffer, geometry.index.offset, geometry.indexType);
        }

        if (geometry.descriptor.set != VK_NULL_HANDLE && modelPipelineLayout_ != VK_NULL_HANDLE) {
            vkCmdBindDescriptorSets(commandBuffer,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    modelPipelineLayout_,
                                    0,
                                    1,
                                    &geometry.descriptor.set,
                                    0,
                                    nullptr);
        }
    }
}

qhandle_t VulkanRenderer::registerImage(const char *name, imagetype_t type, imageflags_t flags) {
    if (!name || !*name) {
        return 0;
    }

    qhandle_t handle = ::R_RegisterImage(name, type, flags);
    if (!handle) {
        return 0;
    }

    const image_t *image = IMG_ForHandle(handle);
    if (!image) {
        return 0;
    }

    auto &record = images_[handle];
    record.handle = handle;
    record.name = image->name;
    record.type = static_cast<imagetype_t>(image->type);
    record.flags = static_cast<imageflags_t>(image->flags);
    record.width = image->width;
    record.height = image->height;
    record.transparent = (record.flags & IF_TRANSPARENT) != 0;
    record.registrationSequence = image->registration_sequence;

    imageLookup_[record.name] = handle;
    if (std::strcmp(record.name.c_str(), name) != 0) {
        imageLookup_[name] = handle;
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
    for (auto it = models_.begin(); it != models_.end();) {
        if (it->second.registrationSequence != r_registration_sequence) {
            destroyModelRecord(it->second);
            it = models_.erase(it);
        } else {
            ++it;
        }
    }

    auto predicate = [](auto &pair) {
        return pair.second.registrationSequence != r_registration_sequence;
    };

    std::erase_if(images_, predicate);
}

void VulkanRenderer::beginFrame() {
    if (!initialized_ || device_ == VK_NULL_HANDLE || swapchain_ == VK_NULL_HANDLE) {
        return;
    }

    if (vid && vid->pump_events) {
        vid->pump_events();
    }

    resetFrameState();
    commandLog_.clear();

    if (inFlightFrames_.empty()) {
        return;
    }

    InFlightFrame &frame = inFlightFrames_[currentFrameIndex_];
    if (frame.inFlight != VK_NULL_HANDLE) {
        vkWaitForFences(device_, 1, &frame.inFlight, VK_TRUE, std::numeric_limits<uint64_t>::max());
    }

    uint32_t imageIndex = 0;
    VkResult acquireResult = vkAcquireNextImageKHR(device_, swapchain_, std::numeric_limits<uint64_t>::max(), frame.imageAvailable, VK_NULL_HANDLE, &imageIndex);

    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return;
    }

    if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
        Com_Printf("refresh-vk: failed to acquire swapchain image (VkResult %d).\n", static_cast<int>(acquireResult));
        return;
    }

    frame.imageIndex = imageIndex;
    frame.hasImage = true;

    if (imageIndex < imagesInFlight_.size() && imagesInFlight_[imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(device_, 1, &imagesInFlight_[imageIndex], VK_TRUE, std::numeric_limits<uint64_t>::max());
    }

    if (imageIndex < imagesInFlight_.size()) {
        imagesInFlight_[imageIndex] = frame.inFlight;
    }

    if (frame.inFlight != VK_NULL_HANDLE) {
        vkResetFences(device_, 1, &frame.inFlight);
    }

    if (frame.commandBuffer == VK_NULL_HANDLE) {
        return;
    }

    VkResult resetResult = vkResetCommandBuffer(frame.commandBuffer, 0);
    if (resetResult != VK_SUCCESS) {
        Com_Printf("refresh-vk: failed to reset command buffer (VkResult %d).\n", static_cast<int>(resetResult));
        return;
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VkResult beginResult = vkBeginCommandBuffer(frame.commandBuffer, &beginInfo);
    if (beginResult != VK_SUCCESS) {
        Com_Printf("refresh-vk: failed to begin command buffer (VkResult %d).\n", static_cast<int>(beginResult));
        return;
    }

    VkClearValue clearValue{};
    clearValue.color = { { 0.0f, 0.0f, 0.0f, 1.0f } };

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass_;
    renderPassInfo.framebuffer = (frame.imageIndex < swapchainFramebuffers_.size()) ? swapchainFramebuffers_[frame.imageIndex] : VK_NULL_HANDLE;
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = swapchainExtent_;
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearValue;

    if (renderPassInfo.framebuffer == VK_NULL_HANDLE) {
        Com_Printf("refresh-vk: framebuffer unavailable for frame.\n");
        vkEndCommandBuffer(frame.commandBuffer);
        return;
    }

    vkCmdBeginRenderPass(frame.commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    frameActive_ = true;
    frameAcquired_ = true;

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

    draw2d::end();
    finishFrameRecording();

    if (inFlightFrames_.empty()) {
        frameActive_ = false;
        frameAcquired_ = false;
        commandLog_.clear();
        frameStats_.reset();
        return;
    }

    InFlightFrame &frame = inFlightFrames_[currentFrameIndex_];
    if (!frame.hasImage || frame.commandBuffer == VK_NULL_HANDLE) {
        frameActive_ = false;
        frameAcquired_ = false;
        commandLog_.clear();
        frameStats_.reset();
        return;
    }

    VkSemaphore waitSemaphores[] = { frame.imageAvailable };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSemaphore signalSemaphores[] = { frame.renderFinished };

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &frame.commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    VkResult submitResult = vkQueueSubmit(graphicsQueue_, 1, &submitInfo, frame.inFlight);
    if (submitResult != VK_SUCCESS) {
        Com_Printf("refresh-vk: failed to submit draw commands (VkResult %d).\n", static_cast<int>(submitResult));
        if (frame.imageIndex < imagesInFlight_.size()) {
            imagesInFlight_[frame.imageIndex] = VK_NULL_HANDLE;
        }
        vkQueueSubmit(graphicsQueue_, 0, nullptr, frame.inFlight);
        vkWaitForFences(device_, 1, &frame.inFlight, VK_TRUE, std::numeric_limits<uint64_t>::max());
        frame.hasImage = false;
        frameActive_ = false;
        frameAcquired_ = false;
        commandLog_.clear();
        frameStats_.reset();
        return;
    }

    VkSwapchainKHR swapchains[] = { swapchain_ };

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &frame.imageIndex;

    VkResult presentResult = vkQueuePresentKHR(presentQueue_, &presentInfo);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
        recreateSwapchain();
    } else if (presentResult != VK_SUCCESS) {
        Com_Printf("refresh-vk: failed to present swapchain image (VkResult %d).\n", static_cast<int>(presentResult));
    }

    commandLog_.clear();
    frameStats_.reset();

    frame.hasImage = false;
    frameActive_ = false;
    frameAcquired_ = false;
    currentFrameIndex_ = (currentFrameIndex_ + 1) % inFlightFrames_.size();
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
            const entity_t *entity = *it;
            if (ModelRecord *record = findModelRecord(entity->model)) {
                bindModelGeometryBuffers(*record);
            }

            PipelineKind kind = selectPipelineForEntity(*entity);
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

    ViewParameters viewParams = computeViewParameters(*fd);

    bool cylindricalBeams = gl_beamstyle && gl_beamstyle->integer != 0;
    streamBeamPrimitives(viewParams, cylindricalBeams);
    if (!effectStreams_.beamIndices.empty()) {
        PipelineKind beamKind = cylindricalBeams ? PipelineKind::BeamCylindrical : PipelineKind::BeamSimple;
        recordDrawCall(ensurePipeline(beamKind), "fx.beams", framePrimitives_.beams.size());
    }

    bool additiveParticles = gl_partstyle && gl_partstyle->integer != 0;
    streamParticlePrimitives(viewParams, additiveParticles);
    if (!effectStreams_.particleVertices.empty()) {
        PipelineKind particleKind = additiveParticles ? PipelineKind::ParticleAdditive : PipelineKind::ParticleAlpha;
        recordDrawCall(ensurePipeline(particleKind), "fx.particles", framePrimitives_.particles.size());
    }

    bool flaresEnabled = true;
    if (cl_flares) {
        flaresEnabled = cl_flares->integer != 0;
    }
    if (flaresEnabled) {
        streamFlarePrimitives(viewParams);
        if (!effectStreams_.flareVertices.empty()) {
            recordDrawCall(ensurePipeline(PipelineKind::Flare), "fx.flares", framePrimitives_.flares.size());
        }
    } else {
        effectStreams_.flareVertices.clear();
        effectStreams_.flareIndices.clear();
        frameStats_.flares = 0;
    }

    processQueue(frameQueues_.alphaFront, "entities.alpha_front");

    streamDebugLinePrimitives();
    if (!effectStreams_.debugLinesDepth.empty()) {
        recordDrawCall(ensurePipeline(PipelineKind::DebugLineDepth), "debug.lines.depth", effectStreams_.debugLinesDepth.size() / 2);
    }
    if (!effectStreams_.debugLinesNoDepth.empty()) {
        recordDrawCall(ensurePipeline(PipelineKind::DebugLineNoDepth), "debug.lines.nodepth", effectStreams_.debugLinesNoDepth.size() / 2);
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
    if (drawingActive && !clipRect_) {
        draw2d::flush();
    }

    scale_ = target;
    activeScissor_.reset();

    if (clipRect_) {
        setClipRect(&*clipRect_);
    } else if (drawingActive) {
        setClipRect(nullptr);
    }
  
    float newScale = 1.0f;
    if (std::isfinite(scale)) {
        newScale = std::clamp(scale, 0.25f, 4.0f);
    }

    if (std::abs(newScale - scale_) > std::numeric_limits<float>::epsilon() && draw2d::isActive()) {
        draw2d::flush();
    }

    scale_ = newScale;
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

    if (images_.find(font) == images_.end()) {
        return;
    }

    uint8_t glyph = static_cast<uint8_t>(ch);
    if ((glyph & 127u) == 32u) {
        return;
    }

    if (flags & kUIAltColor) {
        glyph |= 0x80u;
    }
    if (flags & kUIXorColor) {
        glyph ^= 0x80u;
    }

    float scale = scale_;
    if (scale <= 0.0f) {
        return;
    }

    float fx = static_cast<float>(x) * scale;
    float fy = static_cast<float>(y) * scale;
    float fw = static_cast<float>(w) * scale;
    float fh = static_cast<float>(h) * scale;

    float u0 = static_cast<float>(glyph & 15u) * kFontCellSize;
    float v0 = static_cast<float>(glyph >> 4) * kFontCellSize;
    auto uvs = makeUV(u0, v0, u0 + kFontCellSize, v0 + kFontCellSize);

    if ((flags & kUIDropShadow) && glyph != 0x83u) {
        float offset = kShadowOffset * scale;
        color_t shadow{};
        shadow.r = 0;
        shadow.g = 0;
        shadow.b = 0;
        shadow.a = color.a;
        auto shadowPositions = makeQuad(fx + offset, fy + offset, fw, fh);
        draw2d::submitQuad(shadowPositions, uvs, shadow.u32, font);
    }

    if (glyph & 0x80u) {
        color.r = 255u;
        color.g = 255u;
        color.b = 255u;
    }

    auto positions = makeQuad(fx, fy, fw, fh);
    draw2d::submitQuad(positions, uvs, color.u32, font);
    if (!findImageRecord(font)) {
        return;
    }

    int glyph = ch & 0xFF;
    if ((glyph & 127) == 32) {
        return;
    }

    if (flags & UI_ALTCOLOR) {
        glyph |= 0x80;
    }
    if (flags & UI_XORCOLOR) {
        glyph ^= 0x80;
    }

    constexpr float kCell = 1.0f / 16.0f;
    float s0 = (glyph & 15) * kCell;
    float t0 = (glyph >> 4) * kCell;
    float s1 = s0 + kCell;
    float t1 = t0 + kCell;

    auto submit = [&](int px, int py, color_t tint) {
        submitTexturedQuad(static_cast<float>(px), static_cast<float>(py),
                           static_cast<float>(px + w), static_cast<float>(py + h),
                           s0, t0, s1, t1, tint, font);
    };

    if ((flags & UI_DROPSHADOW) && glyph != 0x83) {
        color_t shadow = ColorA(color.a);
        submit(x + 1, y + 1, shadow);
    }

    color_t finalColor = color;
    if (glyph >> 7) {
        finalColor = ColorSetAlpha(COLOR_WHITE, color.a);
    }

    submit(x, y, finalColor);
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

    float sw = record ? record->sw : kfont->sw;
    float sh = record ? record->sh : kfont->sh;
    float s0 = metrics->x * sw;
    float t0 = metrics->y * sh;
    float s1 = s0 + metrics->w * sw;
    float t1 = t0 + metrics->h * sh;

    auto submit = [&](int px, int py, color_t tint) {
        submitTexturedQuad(static_cast<float>(px), static_cast<float>(py),
                           static_cast<float>(px + w), static_cast<float>(py + h),
                           s0, t0, s1, t1, tint, texture);
    };

    if (flags & UI_DROPSHADOW) {
        int offset = std::max(1, effectiveScale);
        color_t shadow = ColorA(color.a);
        submit(x + offset, y + offset, shadow);
    }

    submit(x, y, color);
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

    if (!findImageRecord(pic)) {
        return;
    }

    submitTexturedQuad(static_cast<float>(x), static_cast<float>(y),
                       static_cast<float>(x + w), static_cast<float>(y + h),
                       0.0f, 0.0f, 1.0f, 1.0f,
                       color, pic);
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

    float scale = scale_;
    if (scale <= 0.0f) {
        return;
    }

    float fx = static_cast<float>(x) * scale;
    float fy = static_cast<float>(y) * scale;
    float fw = static_cast<float>(w) * scale;
    float fh = static_cast<float>(h) * scale;
    float pivotX = static_cast<float>(pivot_x) * scale;
    float pivotY = static_cast<float>(pivot_y) * scale;

    float halfW = fw * 0.5f;
    float halfH = fh * 0.5f;

    std::array<std::array<float, 2>, 4> local{{
        {-halfW + pivotX, -halfH + pivotY},
        { halfW + pivotX, -halfH + pivotY},
        { halfW + pivotX,  halfH + pivotY},
        {-halfW + pivotX,  halfH + pivotY},
    }};

    std::array<std::array<float, 2>, 4> positions{};
    float s = std::sin(angle);
    float c = std::cos(angle);
    for (size_t i = 0; i < local.size(); ++i) {
        float lx = local[i][0];
        float ly = local[i][1];
        positions[i][0] = fx + (lx * c - ly * s);
        positions[i][1] = fy + (lx * s + ly * c);
    }

    draw2d::submitQuad(positions, kFullUVs, color.u32, pic);

    if (!findImageRecord(pic)) {
        return;
    }

    float x0 = static_cast<float>(x);
    float y0 = static_cast<float>(y);
    float x1 = static_cast<float>(x + w);
    float y1 = static_cast<float>(y + h);

    auto positions = makeQuadPositions(x0, y0, x1, y1);
    float originX = static_cast<float>(x + pivot_x);
    float originY = static_cast<float>(y + pivot_y);
    float radians = DEG2RAD(angle);
    float sine = std::sin(radians);
    float cosine = std::cos(radians);

    for (auto &pos : positions) {
        float dx = pos[0] - originX;
        float dy = pos[1] - originY;
        pos[0] = originX + dx * cosine - dy * sine;
        pos[1] = originY + dx * sine + dy * cosine;
    }

    draw2d::submitQuad(positions, makeQuadUVs(0.0f, 0.0f, 1.0f, 1.0f), color.u32, pic);
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

    auto it = images_.find(pic);
    if (it == images_.end()) {
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

    float imageWidth = static_cast<float>(std::max(1, it->second.width));
    float imageHeight = static_cast<float>(std::max(1, it->second.height));
    float aspect = imageHeight / imageWidth;
  
    float aspect = 1.0f;
    if (image->height > 0) {
        aspect = static_cast<float>(image->width) / static_cast<float>(image->height);
    }

    float scaleW = static_cast<float>(w);
    float scaleH = static_cast<float>(h) * aspect;
    float scale = std::max(scaleW, scaleH);

    float s = 0.5f * (1.0f - (scaleW / scale));
    float t = 0.5f * (1.0f - (scaleH / scale));

    float scaleFactor = scale_;
    if (scaleFactor <= 0.0f) {
        return;
    }

    float fx = static_cast<float>(x) * scaleFactor;
    float fy = static_cast<float>(y) * scaleFactor;
    float fw = static_cast<float>(w) * scaleFactor;
    float fh = static_cast<float>(h) * scaleFactor;

    auto positions = makeQuad(fx, fy, fw, fh);
    auto uvs = makeUV(s, t, 1.0f - s, 1.0f - t);
    draw2d::submitQuad(positions, uvs, color.u32, pic);
    if (scale <= 0.0f) {
        return;
    }

    float s = (1.0f - scaleW / scale) * 0.5f;
    float t = (1.0f - scaleH / scale) * 0.5f;
    float s1 = 1.0f - s;
    float t1 = 1.0f - t;

    submitTexturedQuad(static_cast<float>(x), static_cast<float>(y),
                       static_cast<float>(x + w), static_cast<float>(y + h),
                       s, t, s1, t1, color, pic);
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
    submitTexturedQuad(static_cast<float>(x), static_cast<float>(y),
                       static_cast<float>(x + w), static_cast<float>(y + h),
                       0.0f, 0.0f, 1.0f, 1.0f,
                       COLOR_WHITE, texture);
}

void VulkanRenderer::updateRawPic(int pic_w, int pic_h, const uint32_t *pic) {
    if (draw2d::isActive()) {
        draw2d::flush();
    }

    if (pic_w <= 0 || pic_h <= 0 || !pic) {
        rawPic_ = {};
        return;
    }

    rawPic_.width = pic_w;
    rawPic_.height = pic_h;
    rawPic_.pixels.assign(pic, pic + (static_cast<size_t>(pic_w) * static_cast<size_t>(pic_h)));
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

    float scale = scale_;
    if (scale <= 0.0f) {
        return;
    }

    float fx = static_cast<float>(x) * scale;
    float fy = static_cast<float>(y) * scale;
    float fw = static_cast<float>(w) * scale;
    float fh = static_cast<float>(h) * scale;

    auto positions = makeQuad(fx, fy, fw, fh);
    float s0 = static_cast<float>(x) * kTileDivisor;
    float t0 = static_cast<float>(y) * kTileDivisor;
    float s1 = static_cast<float>(x + w) * kTileDivisor;
    float t1 = static_cast<float>(y + h) * kTileDivisor;
    auto uvs = makeUV(s0, t0, s1, t1);
    draw2d::submitQuad(positions, uvs, COLOR_WHITE.u32, pic);

    if (!findImageRecord(pic)) {
        return;
    }

    constexpr float kDiv64 = 1.0f / 64.0f;
    float s0 = static_cast<float>(x) * kDiv64;
    float t0 = static_cast<float>(y) * kDiv64;
    float s1 = static_cast<float>(x + w) * kDiv64;
    float t1 = static_cast<float>(y + h) * kDiv64;

    submitTexturedQuad(static_cast<float>(x), static_cast<float>(y),
                       static_cast<float>(x + w), static_cast<float>(y + h),
                       s0, t0, s1, t1,
                       COLOR_WHITE, pic);
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

    float scale = scale_;
    if (scale <= 0.0f) {
        return;
    }

    float fx = static_cast<float>(x) * scale;
    float fy = static_cast<float>(y) * scale;
    float fw = static_cast<float>(w) * scale;
    float fh = static_cast<float>(h) * scale;

    color_t color{};
    color.u32 = d_8to24table[c & 0xFF];

    auto positions = makeQuad(fx, fy, fw, fh);
    draw2d::submitQuad(positions, kFullUVs, color.u32, 0);

    qhandle_t texture = ensureWhiteTexture();
    color_t color = ColorU32(d_8to24table[c & 0xFF]);

    submitTexturedQuad(static_cast<float>(x), static_cast<float>(y),
                       static_cast<float>(x + w), static_cast<float>(y + h),
                       0.0f, 0.0f, 1.0f, 1.0f,
                       color, texture);
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

    float scale = scale_;
    if (scale <= 0.0f) {
        return;
    }

    float fx = static_cast<float>(x) * scale;
    float fy = static_cast<float>(y) * scale;
    float fw = static_cast<float>(w) * scale;
    float fh = static_cast<float>(h) * scale;

    auto positions = makeQuad(fx, fy, fw, fh);
    draw2d::submitQuad(positions, kFullUVs, color.u32, 0);
    qhandle_t texture = ensureWhiteTexture();

    submitTexturedQuad(static_cast<float>(x), static_cast<float>(y),
                       static_cast<float>(x + w), static_cast<float>(y + h),
                       0.0f, 0.0f, 1.0f, 1.0f,
                       color, texture);
}

void VulkanRenderer::modeChanged(int width, int height, int flags) {
    VideoGeometry geometry{};
    geometry.width = std::max(1, width);
    geometry.height = std::max(1, height);
    geometry.flags = static_cast<vidFlags_t>(flags);
    applyVideoGeometry(geometry);

    if (!initialized_ || device_ == VK_NULL_HANDLE || swapchain_ == VK_NULL_HANDLE) {
        return;
    }

    if (geometry.width <= 0 || geometry.height <= 0) {
        return;
    }

    recreateSwapchain();
}

bool VulkanRenderer::videoSync() const {
    return vsyncEnabled_;
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

    auto assignFallback = [this, font]() {
        std::memset(font, 0, sizeof(*font));

        font->pic = registerImage("_kfont", IT_FONT, IF_PERMANENT);

        uint16_t cursorX = 0;
        for (auto &glyph : font->chars) {
            glyph.x = cursorX;
            glyph.y = 0;
            glyph.w = defaultKFontWidth();
            glyph.h = defaultKFontHeight();
            cursorX = static_cast<uint16_t>(cursorX + glyph.w);
        }

        font->line_height = defaultKFontHeight();
        font->sw = 1.0f;
        font->sh = 1.0f;
        kfontCache_.erase(font);
    };

    kfontCache_.erase(font);

    if (!filename || !*filename) {
        assignFallback();
        return;
    }

    char *buffer = nullptr;
    if (FS_LoadFile(filename, reinterpret_cast<void **>(&buffer)) < 0 || !buffer) {
        assignFallback();
        return;
    }

    KFontRecord record{};
    std::memset(font, 0, sizeof(*font));

    const char *data = buffer;
    while (true) {
        const char *token = COM_Parse(&data);

        if (!token || !*token) {
            break;
        }

        if (!std::strcmp(token, "texture")) {
            const char *textureToken = COM_Parse(&data);
            if (textureToken && *textureToken) {
                std::string imagePath;
                if (textureToken[0] == '/') {
                    imagePath = textureToken;
                } else {
                    imagePath = '/';
                    imagePath += textureToken;
                }
                record.texture = registerImage(imagePath.c_str(), IT_FONT, IF_PERMANENT);
            }
        } else if (!std::strcmp(token, "unicode")) {
            token = COM_Parse(&data);
            while (true) {
                token = COM_Parse(&data);
                if (!token || !*token || !std::strcmp(token, "}")) {
                    break;
                }
            }
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

                if (!xToken || !*xToken || !yToken || !*yToken || !wToken || !*wToken || !hToken || !*hToken) {
                    continue;
                }

                uint32_t codepoint = static_cast<uint32_t>(std::strtoul(token, nullptr, 10));
                uint32_t xValue = static_cast<uint32_t>(std::strtoul(xToken, nullptr, 10));
                uint32_t yValue = static_cast<uint32_t>(std::strtoul(yToken, nullptr, 10));
                uint32_t wValue = static_cast<uint32_t>(std::strtoul(wToken, nullptr, 10));
                uint32_t hValue = static_cast<uint32_t>(std::strtoul(hToken, nullptr, 10));

                if (codepoint < KFONT_ASCII_MIN || codepoint > KFONT_ASCII_MAX) {
                    continue;
                }

                size_t index = static_cast<size_t>(codepoint - KFONT_ASCII_MIN);
                record.glyphs[index].x = static_cast<uint16_t>(xValue);
                record.glyphs[index].y = static_cast<uint16_t>(yValue);
                record.glyphs[index].w = static_cast<uint16_t>(wValue);
                record.glyphs[index].h = static_cast<uint16_t>(hValue);
                record.lineHeight = std::max<uint16_t>(record.lineHeight, static_cast<uint16_t>(hValue));
            }
        }
    }

    FS_FreeFile(buffer);

    if (!record.texture) {
        assignFallback();
        return;
    }

    const image_t *image = IMG_ForHandle(record.texture);
    if (!image || image->width <= 0 || image->height <= 0) {
        assignFallback();
        return;
    }

    record.sw = 1.0f / static_cast<float>(image->width);
    record.sh = 1.0f / static_cast<float>(image->height);
    if (record.lineHeight == 0) {
        record.lineHeight = defaultKFontHeight();
    }

    font->pic = record.texture;
    font->line_height = record.lineHeight;
    font->sw = record.sw;
    font->sh = record.sh;
    std::copy(record.glyphs.begin(), record.glyphs.end(), std::begin(font->chars));

    kfontCache_[font] = record;
}

const kfont_char_t *VulkanRenderer::lookupKFontChar(const kfont_t *kfont, uint32_t codepoint) const {
    if (!kfont) {
        return nullptr;
    }

    if (codepoint < KFONT_ASCII_MIN || codepoint > KFONT_ASCII_MAX) {
        return nullptr;
    }

    size_t index = static_cast<size_t>(codepoint - KFONT_ASCII_MIN);
    if (const KFontRecord *record = findKFontRecord(kfont)) {
        const kfont_char_t &glyph = record->glyphs[index];
        if (glyph.w == 0) {
            return nullptr;
        }
        return &record->glyphs[index];
    }

    const kfont_char_t &glyph = kfont->chars[index];
    if (glyph.w == 0) {
        return nullptr;
    }
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
