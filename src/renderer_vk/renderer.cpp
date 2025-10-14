#include "renderer.h"

namespace refresh::vk {

VulkanRenderer::VulkanRenderer()
    : handleCounter_{1} {}

VulkanRenderer::~VulkanRenderer() = default;

VkPipelineLayout VulkanRenderer::pipelineLayoutFor(PipelineKind kind) const {
    switch (kind) {
    default:
        return modelPipelineLayout_;
    }
}

} // namespace refresh::vk
