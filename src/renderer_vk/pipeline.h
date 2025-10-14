#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

#include <vulkan/vulkan_core.h>

#include "renderer.h"

namespace refresh::vk {

class PipelineLibrary {
public:
    explicit PipelineLibrary(VulkanRenderer &renderer);
    PipelineLibrary(const PipelineLibrary &) = delete;
    PipelineLibrary &operator=(const PipelineLibrary &) = delete;
    ~PipelineLibrary();

    bool initialize();
    void shutdown();

    VkPipeline requestPipeline(const VulkanRenderer::PipelineKey &key);

private:
    struct ShaderModule {
        VkShaderModule handle = VK_NULL_HANDLE;
        std::string name;
    };

    struct PipelineObject {
        VulkanRenderer::PipelineDesc desc;
        ShaderModule vertex;
        ShaderModule fragment;
        VkPipeline handle = VK_NULL_HANDLE;
    };

    using PipelineMap = std::unordered_map<VulkanRenderer::PipelineKey, PipelineObject, VulkanRenderer::PipelineKeyHash>;

    ShaderModule loadShaderModule(std::string_view logicalName, VkShaderStageFlagBits stage);
    void destroyShaderModule(ShaderModule &module) const;
    void destroyPipelineObject(PipelineObject &object);
    bool createGraphicsPipeline(PipelineObject &object);

    VulkanRenderer &renderer_;
    PipelineMap pipelines_;
};

} // namespace refresh::vk

