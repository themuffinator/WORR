#include "renderer.h"

namespace refresh::vk {

VulkanRenderer::VulkanRenderer()
    : handleCounter_{1} {}

VulkanRenderer::~VulkanRenderer() = default;

} // namespace refresh::vk
