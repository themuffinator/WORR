#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <vector>

#include "refresh/refresh.h"

namespace refresh::vk::draw2d {

struct Vertex {
    std::array<float, 2> position{};
    std::array<float, 2> uv{};
    uint32_t color = 0xFFFFFFFFu;
};

struct Submission {
    const Vertex *vertices = nullptr;
    size_t vertexCount = 0;
    const uint16_t *indices = nullptr;
    size_t indexCount = 0;
    qhandle_t texture = 0;
};

using FlushCallback = std::function<void(const Submission &)>;

bool initialize();
void shutdown();

bool begin(FlushCallback callback);
void submitQuad(const std::array<std::array<float, 2>, 4> &positions,
                const std::array<std::array<float, 2>, 4> &uvs,
                uint32_t color,
                qhandle_t texture);
void flush();
void end();

bool isActive();

} // namespace refresh::vk::draw2d

