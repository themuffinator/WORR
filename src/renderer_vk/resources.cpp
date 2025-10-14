#include "renderer.h"

#include "renderer/common.h"
#include "renderer/images.h"

#include <array>
#include <cstring>
#include <utility>

namespace refresh::vk {

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
bool VulkanRenderer::createTextureDescriptorSetLayout() {
    if (device_ == VK_NULL_HANDLE) {
        return false;
    }

    if (textureDescriptorSetLayout_ != VK_NULL_HANDLE) {
        return true;
    }

    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorCount = 1;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = 1;
    info.pBindings = &binding;

    VkResult result = vkCreateDescriptorSetLayout(device_, &info, nullptr, &textureDescriptorSetLayout_);
    if (result != VK_SUCCESS || textureDescriptorSetLayout_ == VK_NULL_HANDLE) {
        textureDescriptorSetLayout_ = VK_NULL_HANDLE;
        Com_Printf("refresh-vk: failed to create texture descriptor set layout (VkResult %d).\n", static_cast<int>(result));
        return false;
    }

    return true;
}
void VulkanRenderer::destroyTextureDescriptorSetLayout() {
    if (device_ == VK_NULL_HANDLE) {
        textureDescriptorSetLayout_ = VK_NULL_HANDLE;
        return;
    }

    if (textureDescriptorSetLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, textureDescriptorSetLayout_, nullptr);
        textureDescriptorSetLayout_ = VK_NULL_HANDLE;
    }
}
void VulkanRenderer::destroyImageRecord(ImageRecord &record) {
    if (device_ == VK_NULL_HANDLE) {
        record.image = VK_NULL_HANDLE;
        record.memory = VK_NULL_HANDLE;
        record.view = VK_NULL_HANDLE;
        record.sampler = VK_NULL_HANDLE;
        record.descriptorSet = VK_NULL_HANDLE;
        record.layout = VK_IMAGE_LAYOUT_UNDEFINED;
        return;
    }

    if (record.descriptorSet != VK_NULL_HANDLE && descriptorPool_ != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(device_, descriptorPool_, 1, &record.descriptorSet);
        record.descriptorSet = VK_NULL_HANDLE;
    }

    if (record.sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device_, record.sampler, nullptr);
        record.sampler = VK_NULL_HANDLE;
    }

    if (record.view != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, record.view, nullptr);
        record.view = VK_NULL_HANDLE;
    }

    if (record.image != VK_NULL_HANDLE) {
        vkDestroyImage(device_, record.image, nullptr);
        record.image = VK_NULL_HANDLE;
    }

    if (record.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device_, record.memory, nullptr);
        record.memory = VK_NULL_HANDLE;
    }

    record.layout = VK_IMAGE_LAYOUT_UNDEFINED;
    record.extent = { 0u, 0u, 1u };
    record.format = VK_FORMAT_UNDEFINED;
}
void VulkanRenderer::destroyAllImageResources() {
    for (auto &entry : images_) {
        destroyImageRecord(entry.second);
    }
}
bool VulkanRenderer::allocateTextureDescriptor(ImageRecord &record) {
    if (device_ == VK_NULL_HANDLE || descriptorPool_ == VK_NULL_HANDLE || textureDescriptorSetLayout_ == VK_NULL_HANDLE) {
        return false;
    }

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool_;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &textureDescriptorSetLayout_;

    VkResult result = vkAllocateDescriptorSets(device_, &allocInfo, &record.descriptorSet);
    if (result != VK_SUCCESS || record.descriptorSet == VK_NULL_HANDLE) {
        record.descriptorSet = VK_NULL_HANDLE;
        Com_Printf("refresh-vk: failed to allocate texture descriptor set (VkResult %d).\n", static_cast<int>(result));
        return false;
    }

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = record.layout;
    imageInfo.imageView = record.view;
    imageInfo.sampler = record.sampler;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = record.descriptorSet;
    write.dstBinding = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
    return true;
}
uint32_t VulkanRenderer::findMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties) const {
    for (uint32_t i = 0; i < memoryProperties_.memoryTypeCount; ++i) {
        if ((typeBits & (1u << i)) && (memoryProperties_.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return UINT32_MAX;
}
bool VulkanRenderer::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                                  VkBuffer &buffer, VkDeviceMemory &memory) {
    if (device_ == VK_NULL_HANDLE) {
        return false;
    }

    VkBufferCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size = size;
    info.usage = usage;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult result = vkCreateBuffer(device_, &info, nullptr, &buffer);
    if (result != VK_SUCCESS || buffer == VK_NULL_HANDLE) {
        buffer = VK_NULL_HANDLE;
        Com_Printf("refresh-vk: failed to create buffer (VkResult %d).\n", static_cast<int>(result));
        return false;
    }

    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device_, buffer, &requirements);

    uint32_t memoryType = findMemoryType(requirements.memoryTypeBits, properties);
    if (memoryType == UINT32_MAX) {
        vkDestroyBuffer(device_, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
        Com_Printf("refresh-vk: failed to find suitable memory type for buffer.\n");
        return false;
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = requirements.size;
    allocInfo.memoryTypeIndex = memoryType;

    result = vkAllocateMemory(device_, &allocInfo, nullptr, &memory);
    if (result != VK_SUCCESS || memory == VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
        memory = VK_NULL_HANDLE;
        Com_Printf("refresh-vk: failed to allocate buffer memory (VkResult %d).\n", static_cast<int>(result));
        return false;
    }

    result = vkBindBufferMemory(device_, buffer, memory, 0);
    if (result != VK_SUCCESS) {
        vkDestroyBuffer(device_, buffer, nullptr);
        vkFreeMemory(device_, memory, nullptr);
        buffer = VK_NULL_HANDLE;
        memory = VK_NULL_HANDLE;
        Com_Printf("refresh-vk: failed to bind buffer memory (VkResult %d).\n", static_cast<int>(result));
        return false;
    }

    return true;
}
void VulkanRenderer::destroyBuffer(VkBuffer buffer, VkDeviceMemory memory) {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }

    if (buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, buffer, nullptr);
    }
    if (memory != VK_NULL_HANDLE) {
        vkFreeMemory(device_, memory, nullptr);
    }
}
VkCommandBuffer VulkanRenderer::beginSingleTimeCommands() {
    if (device_ == VK_NULL_HANDLE || commandPool_ == VK_NULL_HANDLE) {
        return VK_NULL_HANDLE;
    }

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool_;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VkResult result = vkAllocateCommandBuffers(device_, &allocInfo, &commandBuffer);
    if (result != VK_SUCCESS || commandBuffer == VK_NULL_HANDLE) {
        Com_Printf("refresh-vk: failed to allocate command buffer for upload (VkResult %d).\n", static_cast<int>(result));
        return VK_NULL_HANDLE;
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
    if (result != VK_SUCCESS) {
        vkFreeCommandBuffers(device_, commandPool_, 1, &commandBuffer);
        Com_Printf("refresh-vk: failed to begin upload command buffer (VkResult %d).\n", static_cast<int>(result));
        return VK_NULL_HANDLE;
    }

    return commandBuffer;
}
void VulkanRenderer::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
    if (device_ == VK_NULL_HANDLE || commandBuffer == VK_NULL_HANDLE) {
        return;
    }

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkResult result = vkQueueSubmit(graphicsQueue_, 1, &submitInfo, VK_NULL_HANDLE);
    if (result != VK_SUCCESS) {
        Com_Printf("refresh-vk: failed to submit upload command buffer (VkResult %d).\n", static_cast<int>(result));
    }

    vkQueueWaitIdle(graphicsQueue_);
    vkFreeCommandBuffers(device_, commandPool_, 1, &commandBuffer);
}
void VulkanRenderer::transitionImageLayout(VkCommandBuffer commandBuffer, VkImage image,
                                           VkImageLayout oldLayout, VkImageLayout newLayout) {
    if (commandBuffer == VK_NULL_HANDLE || image == VK_NULL_HANDLE) {
        return;
    }

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }

    vkCmdPipelineBarrier(commandBuffer,
                         srcStage, dstStage,
                         0,
                         0, nullptr,
                         0, nullptr,
                         1, &barrier);
}
void VulkanRenderer::copyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer buffer, VkImage image,
                                       uint32_t width, uint32_t height) {
    if (commandBuffer == VK_NULL_HANDLE || buffer == VK_NULL_HANDLE || image == VK_NULL_HANDLE) {
        return;
    }

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { width, height, 1 };

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}
bool VulkanRenderer::uploadImagePixels(ImageRecord &record, const uint8_t *pixels, size_t size,
                                       uint32_t width, uint32_t height, VkFormat format) {
    if (!pixels || size == 0 || width == 0 || height == 0) {
        return false;
    }

    VkDeviceSize dataSize = static_cast<VkDeviceSize>(size);
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;

    if (!createBuffer(dataSize,
                      VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      stagingBuffer,
                      stagingMemory)) {
        return false;
    }

    void *mapped = nullptr;
    VkResult result = vkMapMemory(device_, stagingMemory, 0, dataSize, 0, &mapped);
    if (result != VK_SUCCESS || mapped == nullptr) {
        destroyBuffer(stagingBuffer, stagingMemory);
        Com_Printf("refresh-vk: failed to map staging buffer memory (VkResult %d).\n", static_cast<int>(result));
        return false;
    }

    std::memcpy(mapped, pixels, static_cast<size_t>(dataSize));
    vkUnmapMemory(device_, stagingMemory);

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    result = vkCreateImage(device_, &imageInfo, nullptr, &record.image);
    if (result != VK_SUCCESS || record.image == VK_NULL_HANDLE) {
        destroyBuffer(stagingBuffer, stagingMemory);
        record.image = VK_NULL_HANDLE;
        Com_Printf("refresh-vk: failed to create image (VkResult %d).\n", static_cast<int>(result));
        return false;
    }

    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(device_, record.image, &requirements);

    uint32_t memoryType = findMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (memoryType == UINT32_MAX) {
        destroyBuffer(stagingBuffer, stagingMemory);
        vkDestroyImage(device_, record.image, nullptr);
        record.image = VK_NULL_HANDLE;
        Com_Printf("refresh-vk: failed to find suitable memory type for image.\n");
        return false;
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = requirements.size;
    allocInfo.memoryTypeIndex = memoryType;

    result = vkAllocateMemory(device_, &allocInfo, nullptr, &record.memory);
    if (result != VK_SUCCESS || record.memory == VK_NULL_HANDLE) {
        destroyBuffer(stagingBuffer, stagingMemory);
        vkDestroyImage(device_, record.image, nullptr);
        record.image = VK_NULL_HANDLE;
        record.memory = VK_NULL_HANDLE;
        Com_Printf("refresh-vk: failed to allocate image memory (VkResult %d).\n", static_cast<int>(result));
        return false;
    }

    result = vkBindImageMemory(device_, record.image, record.memory, 0);
    if (result != VK_SUCCESS) {
        destroyBuffer(stagingBuffer, stagingMemory);
        vkDestroyImage(device_, record.image, nullptr);
        vkFreeMemory(device_, record.memory, nullptr);
        record.image = VK_NULL_HANDLE;
        record.memory = VK_NULL_HANDLE;
        Com_Printf("refresh-vk: failed to bind image memory (VkResult %d).\n", static_cast<int>(result));
        return false;
    }

    VkCommandBuffer commandBuffer = beginSingleTimeCommands();
    if (commandBuffer == VK_NULL_HANDLE) {
        destroyBuffer(stagingBuffer, stagingMemory);
        return false;
    }

    transitionImageLayout(commandBuffer, record.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(commandBuffer, stagingBuffer, record.image, width, height);
    transitionImageLayout(commandBuffer, record.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    endSingleTimeCommands(commandBuffer);

    destroyBuffer(stagingBuffer, stagingMemory);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = record.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.components = {
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY,
    };
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    result = vkCreateImageView(device_, &viewInfo, nullptr, &record.view);
    if (result != VK_SUCCESS || record.view == VK_NULL_HANDLE) {
        vkDestroyImage(device_, record.image, nullptr);
        vkFreeMemory(device_, record.memory, nullptr);
        record.image = VK_NULL_HANDLE;
        record.memory = VK_NULL_HANDLE;
        record.view = VK_NULL_HANDLE;
        Com_Printf("refresh-vk: failed to create image view (VkResult %d).\n", static_cast<int>(result));
        return false;
    }

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    result = vkCreateSampler(device_, &samplerInfo, nullptr, &record.sampler);
    if (result != VK_SUCCESS || record.sampler == VK_NULL_HANDLE) {
        vkDestroyImageView(device_, record.view, nullptr);
        vkDestroyImage(device_, record.image, nullptr);
        vkFreeMemory(device_, record.memory, nullptr);
        record.view = VK_NULL_HANDLE;
        record.image = VK_NULL_HANDLE;
        record.memory = VK_NULL_HANDLE;
        record.sampler = VK_NULL_HANDLE;
        Com_Printf("refresh-vk: failed to create sampler (VkResult %d).\n", static_cast<int>(result));
        return false;
    }

    record.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    record.extent = { width, height, 1 };
    record.format = format;
    return true;
}
bool VulkanRenderer::ensureTextureResources(ImageRecord &record, const uint8_t *pixels, size_t size,
                                            uint32_t width, uint32_t height, VkFormat format) {
    if (device_ == VK_NULL_HANDLE) {
        return false;
    }

    destroyImageRecord(record);

    if (!uploadImagePixels(record, pixels, size, width, height, format)) {
        destroyImageRecord(record);
        return false;
    }

    if (!allocateTextureDescriptor(record)) {
        destroyImageRecord(record);
        return false;
    }

    return true;
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

} // namespace refresh::vk
