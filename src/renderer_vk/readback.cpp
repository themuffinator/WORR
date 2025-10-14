#include "renderer.h"

#include "renderer/images.h"

#include "common/error.h"
#include "common/zone.h"

#include <cstdint>
#include <limits>

namespace refresh::vk {

VulkanRenderer &GetRendererInstance();

int VulkanRenderer::readPixels(screenshot_s *s) {
    if (!s) {
        return Q_ERR(EINVAL);
    }

    s->pixels = nullptr;
    s->width = 0;
    s->height = 0;
    s->rowbytes = 0;
    s->bpp = 0;

    if (!initialized_ || device_ == VK_NULL_HANDLE || swapchain_ == VK_NULL_HANDLE) {
        return Q_ERR(EINVAL);
    }

    if (inFlightFrames_.empty()) {
        return Q_ERR(EINVAL);
    }

    auto frameHasData = [](const InFlightFrame &frame) {
        return frame.readbackValid && frame.readbackBuffer != VK_NULL_HANDLE &&
               frame.readbackMemory != VK_NULL_HANDLE && frame.readbackExtent.width > 0 &&
               frame.readbackExtent.height > 0;
    };

    InFlightFrame *framePtr = nullptr;
    if (lastCompletedReadback_.has_value() && *lastCompletedReadback_ < inFlightFrames_.size()) {
        InFlightFrame &candidate = inFlightFrames_[*lastCompletedReadback_];
        if (frameHasData(candidate)) {
            framePtr = &candidate;
        }
    }

    if (!framePtr) {
        for (InFlightFrame &candidate : inFlightFrames_) {
            if (frameHasData(candidate)) {
                framePtr = &candidate;
                break;
            }
        }
    }

    if (!framePtr) {
        vkQueueWaitIdle(graphicsQueue_);
        if (lastCompletedReadback_.has_value() && *lastCompletedReadback_ < inFlightFrames_.size()) {
            InFlightFrame &candidate = inFlightFrames_[*lastCompletedReadback_];
            if (frameHasData(candidate)) {
                framePtr = &candidate;
            }
        }
        if (!framePtr) {
            for (InFlightFrame &candidate : inFlightFrames_) {
                if (frameHasData(candidate)) {
                    framePtr = &candidate;
                    break;
                }
            }
        }
        if (!framePtr) {
            return Q_ERR(EAGAIN);
        }
    }

    InFlightFrame &frame = *framePtr;
    if (frame.inFlight != VK_NULL_HANDLE) {
        vkWaitForFences(device_, 1, &frame.inFlight, VK_TRUE, std::numeric_limits<uint64_t>::max());
    } else {
        vkQueueWaitIdle(graphicsQueue_);
    }

    const uint32_t width = frame.readbackExtent.width;
    const uint32_t height = frame.readbackExtent.height;
    if (width == 0 || height == 0) {
        return Q_ERR(EINVAL);
    }

    constexpr int bpp = 4;
    if (width > static_cast<uint32_t>(std::numeric_limits<int>::max()) ||
        height > static_cast<uint32_t>(std::numeric_limits<int>::max())) {
        return Q_ERR(EOVERFLOW);
    }

    int rowbytes = bpp * static_cast<int>(width);
    if (rowbytes < 0) {
        return Q_ERR(EOVERFLOW);
    }

    int64_t totalSize = static_cast<int64_t>(rowbytes) * static_cast<int>(height);
    if (totalSize <= 0 || totalSize > std::numeric_limits<int>::max()) {
        return Q_ERR(EOVERFLOW);
    }

    VkDeviceSize requiredSize = static_cast<VkDeviceSize>(rowbytes) * static_cast<VkDeviceSize>(height);
    if (requiredSize == 0 || requiredSize > frame.readbackSize) {
        return Q_ERR(EOVERFLOW);
    }

    byte *pixels = static_cast<byte *>(Z_TagMalloc(static_cast<size_t>(totalSize), TAG_RENDERER));
    if (!pixels) {
        return Q_ERR(ENOMEM);
    }

    void *mapped = nullptr;
    VkResult mapResult = vkMapMemory(device_, frame.readbackMemory, 0, frame.readbackSize, 0, &mapped);
    if (mapResult != VK_SUCCESS || !mapped) {
        Z_Free(pixels);
        return Q_ERR_FAILURE;
    }

    const auto *srcBase = static_cast<const uint8_t *>(mapped);
    const size_t srcRowStride = static_cast<size_t>(rowbytes);
    const size_t dstRowStride = static_cast<size_t>(rowbytes);

    for (uint32_t y = 0; y < height; ++y) {
        const uint8_t *srcRow = srcBase + (static_cast<size_t>(height - 1 - y) * srcRowStride);
        uint8_t *dstRow = pixels + (static_cast<size_t>(y) * dstRowStride);
        for (uint32_t x = 0; x < width; ++x) {
            const uint8_t *src = srcRow + static_cast<size_t>(x) * bpp;
            uint8_t *dst = dstRow + static_cast<size_t>(x) * bpp;
            dst[0] = src[2];
            dst[1] = src[1];
            dst[2] = src[0];
            dst[3] = src[3];
        }
    }

    vkUnmapMemory(device_, frame.readbackMemory);

    s->pixels = pixels;
    s->width = static_cast<int>(width);
    s->height = static_cast<int>(height);
    s->rowbytes = rowbytes;
    s->bpp = bpp;

    return Q_ERR_SUCCESS;
}

} // namespace refresh::vk

int IMG_ReadPixels(screenshot_t *s) {
    if (!s) {
        return Q_ERR(EINVAL);
    }

    return refresh::vk::GetRendererInstance().readPixels(s);
}
