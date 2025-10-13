#include "vk_draw2d.h"

#include <algorithm>
#include <limits>
#include <new>
#include <utility>

#include "common/common.h"

namespace refresh::vk::draw2d {

namespace {
    constexpr size_t kVerticesPerQuad = 4;
    constexpr size_t kIndicesPerQuad = 6;
    constexpr size_t kInitialVertexCapacity = 256;
    constexpr size_t kInitialIndexCapacity = 384;
    constexpr uint32_t kInvalidColor = 0xFFFFFFFFu;

    struct State {
        bool initialized = false;
        bool active = false;
        qhandle_t texture = 0;
        std::vector<Vertex> vertices;
        std::vector<uint16_t> indices;
        FlushCallback flushCallback;
    } state;

    void resetBuffers() {
        state.vertices.clear();
        state.indices.clear();
        state.texture = 0;
    }

    void ensureCapacity(size_t vertexCount, size_t indexCount) {
        if (state.vertices.capacity() < vertexCount) {
            state.vertices.reserve(std::max(vertexCount, state.vertices.capacity() * 2));
        }
        if (state.indices.capacity() < indexCount) {
            state.indices.reserve(std::max(indexCount, state.indices.capacity() * 2));
        }
    }

    void issueFlush() {
        if (state.vertices.empty() || state.indices.empty() || !state.flushCallback) {
            resetBuffers();
            return;
        }

        Submission submission{};
        submission.vertices = state.vertices.data();
        submission.vertexCount = state.vertices.size();
        submission.indices = state.indices.data();
        submission.indexCount = state.indices.size();
        submission.texture = state.texture;

        state.flushCallback(submission);

        resetBuffers();
    }
}

bool initialize() {
    if (state.initialized) {
        return true;
    }

    try {
        state.vertices.reserve(kInitialVertexCapacity);
        state.indices.reserve(kInitialIndexCapacity);
    } catch (const std::bad_alloc &) {
        Com_Printf("vk_draw2d: failed to allocate initial buffers.\n");
        return false;
    }

    state.initialized = true;
    return true;
}

void shutdown() {
    if (!state.initialized) {
        return;
    }

    if (state.active) {
        end();
    }

    state.vertices.clear();
    state.indices.clear();
    state.vertices.shrink_to_fit();
    state.indices.shrink_to_fit();
    state.flushCallback = {};
    state.initialized = false;
}

bool begin(FlushCallback callback) {
    if (!state.initialized) {
        Com_Printf("vk_draw2d: begin() called before initialize().\n");
        return false;
    }

    if (state.active) {
        Com_Printf("vk_draw2d: begin() called while already active.\n");
        return false;
    }

    state.flushCallback = std::move(callback);
    state.active = true;
    resetBuffers();

    return true;
}

void submitQuad(const std::array<std::array<float, 2>, 4> &positions,
                const std::array<std::array<float, 2>, 4> &uvs,
                uint32_t color,
                qhandle_t texture) {
    if (!state.active) {
        Com_Printf("vk_draw2d: submitQuad() called outside begin()/end().\n");
        return;
    }

    if (color == kInvalidColor) {
        color = 0xFFFFFFFFu;
    }

    bool needsFlush = false;
    if (!state.vertices.empty()) {
        needsFlush |= (state.texture != texture);
        needsFlush |= (state.vertices.size() + kVerticesPerQuad > std::numeric_limits<uint16_t>::max());
    }

    if (needsFlush) {
        flush();
    }

    if (state.vertices.empty()) {
        state.texture = texture;
    }

    size_t nextVertexCount = state.vertices.size() + kVerticesPerQuad;
    size_t nextIndexCount = state.indices.size() + kIndicesPerQuad;
    ensureCapacity(nextVertexCount, nextIndexCount);

    uint16_t baseIndex = static_cast<uint16_t>(state.vertices.size());
    for (size_t i = 0; i < kVerticesPerQuad; ++i) {
        Vertex vertex{};
        vertex.position = positions[i];
        vertex.uv = uvs[i];
        vertex.color = color;
        state.vertices.emplace_back(vertex);
    }

    state.indices.emplace_back(baseIndex + 0);
    state.indices.emplace_back(baseIndex + 2);
    state.indices.emplace_back(baseIndex + 3);
    state.indices.emplace_back(baseIndex + 0);
    state.indices.emplace_back(baseIndex + 1);
    state.indices.emplace_back(baseIndex + 2);
}

void flush() {
    if (!state.active) {
        return;
    }

    issueFlush();
}

void end() {
    if (!state.active) {
        return;
    }

    flush();
    state.active = false;
    state.flushCallback = {};
}

bool isActive() {
    return state.active;
}

} // namespace refresh::vk::draw2d

