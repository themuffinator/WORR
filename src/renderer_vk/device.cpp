#include "renderer.h"

#include "renderer/common.h"
#include "pipeline.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <unordered_set>

#include "client/client.h"

namespace refresh::vk {

int VulkanRenderer::readSwapIntervalSetting() const {
    int desired = 1;
    if (gl_swapinterval) {
        desired = gl_swapinterval->integer;
    } else {
        if (cvar_t *swap = Cvar_Find("gl_swapinterval")) {
            desired = swap->integer;
        }
    }

    if (desired < 0) {
        desired = 0;
    }

    return desired;
}
bool VulkanRenderer::refreshSwapInterval(bool allowRecreate) {
    int desired = readSwapIntervalSetting();
    if (desired == swapInterval_) {
        return false;
    }

    swapInterval_ = desired;

    if (swapInterval_ > 0) {
        r_config.flags = static_cast<vidFlags_t>(r_config.flags | QVF_VIDEOSYNC);
    } else {
        r_config.flags = static_cast<vidFlags_t>(r_config.flags & ~QVF_VIDEOSYNC);
    }

    if (allowRecreate && initialized_ && device_ != VK_NULL_HANDLE && swapchain_ != VK_NULL_HANDLE) {
        rebuildSwapchain();
    }

    return true;
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
    supportsDebugUtils_ = false;

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
        if (std::strcmp(name, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0) {
            supportsDebugUtils_ = true;
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
    auto hasMode = [&](VkPresentModeKHR desired) {
        return std::find(presentModes.begin(), presentModes.end(), desired) != presentModes.end();
    };

    bool wantVsync = swapInterval_ > 0;

    if (!wantVsync) {
        if (hasMode(VK_PRESENT_MODE_MAILBOX_KHR)) {
            return VK_PRESENT_MODE_MAILBOX_KHR;
        }
        if (hasMode(VK_PRESENT_MODE_IMMEDIATE_KHR)) {
            return VK_PRESENT_MODE_IMMEDIATE_KHR;
        }
        if (hasMode(VK_PRESENT_MODE_FIFO_RELAXED_KHR)) {
            return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
        }
    } else {
        if (swapInterval_ > 1 && hasMode(VK_PRESENT_MODE_FIFO_RELAXED_KHR)) {
            return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
        }
        if (hasMode(VK_PRESENT_MODE_FIFO_KHR)) {
            return VK_PRESENT_MODE_FIFO_KHR;
        }
        if (hasMode(VK_PRESENT_MODE_FIFO_RELAXED_KHR)) {
            return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
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

    memoryProperties_ = {};
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

        deviceExtensions_.clear();
        deviceExtensions_.reserve(extensions.size());
        for (const VkExtensionProperties &prop : extensions) {
            deviceExtensions_.emplace_back(prop.extensionName);
            if (std::strcmp(prop.extensionName, VK_EXT_DEBUG_MARKER_EXTENSION_NAME) == 0) {
                supportsDebugUtils_ = true;
            }
        }

        vkGetPhysicalDeviceProperties(candidate, &physicalDeviceProperties_);
        vkGetPhysicalDeviceFeatures(candidate, &supportedFeatures_);

        physicalDevice_ = candidate;
        graphicsQueueFamily_ = indices.graphics.value();
        presentQueueFamily_ = indices.present.value();
        vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memoryProperties_);
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

    enabledFeatures_ = features;

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
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
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

    if (!createPostProcessResources()) {
        Com_Printf("refresh-vk: post-process resources unavailable, disabling effects.\n");
    }

    imagesInFlight_.assign(imageCount, VK_NULL_HANDLE);
    vsyncEnabled_ = (presentMode == VK_PRESENT_MODE_FIFO_KHR || presentMode == VK_PRESENT_MODE_FIFO_RELAXED_KHR);
    if (vsyncEnabled_) {
        r_config.flags = static_cast<vidFlags_t>(r_config.flags | QVF_VIDEOSYNC);
    } else {
        r_config.flags = static_cast<vidFlags_t>(r_config.flags & ~QVF_VIDEOSYNC);
    }

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

    if (!createTextureDescriptorSetLayout()) {
        destroyDescriptorPool();
    }
  
    if (!createModelDescriptorResources()) {
        return false;
    }

    if (!pipelineLibrary_) {
        pipelineLibrary_ = std::make_unique<PipelineLibrary>(*this);
    }
    if (pipelineLibrary_ && !pipelineLibrary_->initialize()) {
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
    lastSubmittedFrame_.reset();
}
void VulkanRenderer::destroySwapchainResources() {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }

    destroySyncObjects();
    destroyPostProcessResources();

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
    frameActive_ = false;
    swapchainExtent_ = { 0u, 0u };
    swapchainFormat_ = VK_FORMAT_UNDEFINED;
    vsyncEnabled_ = (swapInterval_ > 0);
    if (vsyncEnabled_) {
        r_config.flags = static_cast<vidFlags_t>(r_config.flags | QVF_VIDEOSYNC);
    } else {
        r_config.flags = static_cast<vidFlags_t>(r_config.flags & ~QVF_VIDEOSYNC);
    }
    lastSubmittedFrame_.reset();
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
    clear2DBatches();
    destroyAllModelGeometry();
    destroySwapchainResources();
    destroyAllImageResources();
    destroyModelDescriptorResources();
    if (pipelineLibrary_) {
        pipelineLibrary_->shutdown();
        pipelineLibrary_.reset();
    }
    destroyDescriptorPool();
    destroyTextureDescriptorSetLayout();
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
    memoryProperties_ = {};
    physicalDeviceProperties_ = {};
    supportedFeatures_ = {};
    enabledFeatures_ = {};
    deviceExtensions_.clear();
    lastSubmittedFrame_.reset();
}
bool VulkanRenderer::rebuildSwapchain() {
    if (device_ == VK_NULL_HANDLE) {
        return false;
    }

    vkDeviceWaitIdle(device_);

    destroySwapchainResources();

    if (!createSwapchainResources()) {
        return false;
    }

    if (!createSyncObjects()) {
        destroySwapchainResources();
        return false;
    }

    return true;
}
bool VulkanRenderer::recreateSwapchain() {
    return rebuildSwapchain();
}
void VulkanRenderer::destroyVulkan() {
    destroyDeviceResources();
    destroyPlatformSurface(instance_, nullptr);
    destroyInstance();
}
bool VulkanRenderer::createOffscreenTarget(OffscreenTarget &target, VkExtent2D extent, VkFormat format,
                                           VkRenderPass renderPass, VkImageUsageFlags usage) {
    destroyOffscreenTarget(target);

    if (device_ == VK_NULL_HANDLE || renderPass == VK_NULL_HANDLE || extent.width == 0 || extent.height == 0) {
        return false;
    }

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = { extent.width, extent.height, 1 };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult imageResult = vkCreateImage(device_, &imageInfo, nullptr, &target.image);
    if (imageResult != VK_SUCCESS || target.image == VK_NULL_HANDLE) {
        target.image = VK_NULL_HANDLE;
        return false;
    }

    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(device_, target.image, &requirements);
    uint32_t memoryType = findMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (memoryType == UINT32_MAX) {
        vkDestroyImage(device_, target.image, nullptr);
        target.image = VK_NULL_HANDLE;
        return false;
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = requirements.size;
    allocInfo.memoryTypeIndex = memoryType;

    VkResult allocResult = vkAllocateMemory(device_, &allocInfo, nullptr, &target.memory);
    if (allocResult != VK_SUCCESS || target.memory == VK_NULL_HANDLE) {
        vkDestroyImage(device_, target.image, nullptr);
        target.image = VK_NULL_HANDLE;
        target.memory = VK_NULL_HANDLE;
        return false;
    }

    vkBindImageMemory(device_, target.image, target.memory, 0);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = target.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkResult viewResult = vkCreateImageView(device_, &viewInfo, nullptr, &target.view);
    if (viewResult != VK_SUCCESS || target.view == VK_NULL_HANDLE) {
        destroyOffscreenTarget(target);
        return false;
    }

    VkImageView attachments[] = { target.view };

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = renderPass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = attachments;
    framebufferInfo.width = extent.width;
    framebufferInfo.height = extent.height;
    framebufferInfo.layers = 1;

    VkResult framebufferResult = vkCreateFramebuffer(device_, &framebufferInfo, nullptr, &target.framebuffer);
    if (framebufferResult != VK_SUCCESS || target.framebuffer == VK_NULL_HANDLE) {
        destroyOffscreenTarget(target);
        return false;
    }

    target.extent = extent;
    return true;
}
void VulkanRenderer::destroyOffscreenTarget(OffscreenTarget &target) {
    if (device_ == VK_NULL_HANDLE) {
        target = {};
        return;
    }

    if (target.framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device_, target.framebuffer, nullptr);
        target.framebuffer = VK_NULL_HANDLE;
    }
    if (target.view != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, target.view, nullptr);
        target.view = VK_NULL_HANDLE;
    }
    if (target.image != VK_NULL_HANDLE) {
        vkDestroyImage(device_, target.image, nullptr);
        target.image = VK_NULL_HANDLE;
    }
    if (target.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device_, target.memory, nullptr);
        target.memory = VK_NULL_HANDLE;
    }

    target.extent = { 0u, 0u };
}
bool VulkanRenderer::createPostProcessResources() {
    destroyPostProcessResources();

    if (device_ == VK_NULL_HANDLE || swapchainExtent_.width == 0 || swapchainExtent_.height == 0 ||
        swapchainFormat_ == VK_FORMAT_UNDEFINED) {
        postProcessAvailable_ = false;
        return false;
    }

    VkAttachmentDescription attachment{};
    attachment.format = swapchainFormat_;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &attachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    VkResult passResult = vkCreateRenderPass(device_, &renderPassInfo, nullptr, &offscreenRenderPass_);
    if (passResult != VK_SUCCESS || offscreenRenderPass_ == VK_NULL_HANDLE) {
        offscreenRenderPass_ = VK_NULL_HANDLE;
        postProcessAvailable_ = false;
        return false;
    }

    VkImageUsageFlags sceneUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (!createOffscreenTarget(sceneTarget_, swapchainExtent_, swapchainFormat_, offscreenRenderPass_, sceneUsage)) {
        destroyPostProcessResources();
        postProcessAvailable_ = false;
        return false;
    }

    VkExtent2D bloomExtent{ std::max(1u, swapchainExtent_.width / 4), std::max(1u, swapchainExtent_.height / 4) };
    VkImageUsageFlags bloomUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                   VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    for (auto &target : bloomTargets_) {
        if (!createOffscreenTarget(target, bloomExtent, swapchainFormat_, offscreenRenderPass_, bloomUsage)) {
            destroyPostProcessResources();
            postProcessAvailable_ = false;
            return false;
        }
    }

    postProcessAvailable_ = true;
    sceneTargetReady_ = false;
    bloomTargetsReady_.fill(false);
    return true;
}
void VulkanRenderer::destroyPostProcessResources() {
    if (device_ == VK_NULL_HANDLE) {
        postProcessAvailable_ = false;
        sceneTarget_ = {};
        bloomTargets_ = {};
        offscreenRenderPass_ = VK_NULL_HANDLE;
        sceneTargetReady_ = false;
        bloomTargetsReady_.fill(false);
        return;
    }

    for (auto &target : bloomTargets_) {
        destroyOffscreenTarget(target);
    }
    bloomTargets_ = {};
    destroyOffscreenTarget(sceneTarget_);

    if (offscreenRenderPass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device_, offscreenRenderPass_, nullptr);
        offscreenRenderPass_ = VK_NULL_HANDLE;
    }

    postProcessAvailable_ = false;
    sceneTargetReady_ = false;
    bloomTargetsReady_.fill(false);
}
void VulkanRenderer::finishFrameRecording() {
    if (!frameActive_ || !frameAcquired_) {
        return;
    }

    InFlightFrame &frame = inFlightFrames_[currentFrameIndex_];
    if (!frame.hasImage || frame.commandBuffer == VK_NULL_HANDLE) {
        return;
    }

    if (frameRenderPassActive_) {
        vkCmdEndRenderPass(frame.commandBuffer);
        frameRenderPassActive_ = false;
    }
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

    Renderer_InitSharedCvars();

    vk_fog = Cvar_Get("vk_fog", "-1", 0);
    vk_bloom = Cvar_Get("vk_bloom", "-1", 0);
    vk_polyblend = Cvar_Get("vk_polyblend", "-1", 0);
    vk_waterwarp = Cvar_Get("vk_waterwarp", "-1", 0);
    vk_dynamic = Cvar_Get("vk_dynamic", "-1", 0);
    vk_perPixelLighting = Cvar_Get("vk_per_pixel_lighting", "-1", 0);

    Com_Printf("------- refresh-vk init -------\n");

    initializePlatformHooks();
    collectPlatformInstanceExtensions();

    VideoGeometry geometry = queryVideoGeometry();
    applyVideoGeometry(geometry);
    Com_Printf("Using video geometry %dx%d%s\n", r_config.width, r_config.height,
               (r_config.flags & QVF_FULLSCREEN) ? " (fullscreen)" : "");

    resetTransientState();

    swapInterval_ = readSwapIntervalSetting();
    if (swapInterval_ > 0) {
        r_config.flags = static_cast<vidFlags_t>(r_config.flags | QVF_VIDEOSYNC);
    } else {
        r_config.flags = static_cast<vidFlags_t>(r_config.flags & ~QVF_VIDEOSYNC);
    }

    updateUIScaling();

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
    rawPic_ = {};
    whiteTextureHandle_ = 0;
    rawTextureHandle_ = 0;
    currentMap_.clear();
    resetTransientState();
    draw2d::shutdown();

    if (total) {
        initialized_ = false;
    } else {
        initialized_ = false;
    }
}

} // namespace refresh::vk
