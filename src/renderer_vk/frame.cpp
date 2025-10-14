#include "renderer.h"

#include "vk_draw2d.h"

#include "renderer/common.h"
#include "renderer/images.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>

#include "client/client.h"
#include "common/cmodel.h"
#include "common/files.h"

namespace refresh::vk {

namespace {

constexpr float kInverseLightIntensity = 1.0f / 255.0f;
constexpr float kParticleSize = 1.0f + M_SQRT1_2f;
constexpr float kParticleScale = 1.0f / (2.0f * kParticleSize);
constexpr float kParticleDistanceBias = 20.0f;
constexpr float kParticleDistanceScale = 0.004f;
constexpr int kBeamCylinderSides = 12;

std::array<float, 3> toArray(const vec3_t value) {
    return { value[0], value[1], value[2] };
}

void arrayToVec3(const std::array<float, 3> &src, vec3_t dst) {
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

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

VulkanRenderer::ModelRecord::AliasFrameMetadata makeAliasFrameMetadata(const maliasframe_t &frame) {
    VulkanRenderer::ModelRecord::AliasFrameMetadata metadata{};
    metadata.boundsMin = toArray(frame.bounds[0]);
    metadata.boundsMax = toArray(frame.bounds[1]);
    metadata.scale = toArray(frame.scale);
    metadata.translate = toArray(frame.translate);
    metadata.radius = frame.radius;
    return metadata;
}

std::string describeFogBits(refresh::vk::VulkanRenderer::FogBits bits) {
    using FogBits = refresh::vk::VulkanRenderer::FogBits;
    if (bits == FogBits::FogNone) {
        return {};
    }

    std::string description{"("};
    bool first = true;
    auto append = [&](const char *label) {
        if (!first) {
            description.append("|");
        }
        description.append(label);
        first = false;
    };

    if ((bits & FogBits::FogGlobal) != FogBits::FogNone) {
        append("global");
    }
    if ((bits & FogBits::FogHeight) != FogBits::FogNone) {
        append("height");
    }
    if ((bits & FogBits::FogSky) != FogBits::FogNone) {
        append("sky");
    }

    if (first) {
        return {};
    }
    description.push_back(')');
    return description;
}

cvar_t *vk_fog = nullptr;
cvar_t *vk_bloom = nullptr;
cvar_t *vk_polyblend = nullptr;
cvar_t *vk_waterwarp = nullptr;
cvar_t *vk_dynamic = nullptr;
cvar_t *vk_perPixelLighting = nullptr;

bool legacyToggleValue(const char *name, bool defaultValue) {
    if (!name || !*name) {
        return defaultValue;
    }
    if (cvar_t *legacy = Cvar_FindVar(name)) {
        return legacy->integer > 0;
    }
    return defaultValue;
}

bool resolveToggle(cvar_t *primary, const char *legacyName, bool defaultValue) {
    bool fallback = legacyToggleValue(legacyName, defaultValue);
    if (!primary) {
        return fallback;
    }
    int value = primary->integer;
    if (value < 0) {
        return fallback;
    }
    return value > 0;
}

} // namespace

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
void VulkanRenderer::clearFrameTransientQueues() {
    frameQueues_.clear();
    framePrimitives_.clear();
    effectStreams_.clear();
}
void VulkanRenderer::resetFrameStatistics() {
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
VulkanRenderer::PipelineDesc VulkanRenderer::makePipeline(const PipelineKey &key) const {
    PipelineDesc desc{};
    desc.key = key;

    switch (key.kind) {
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
    case PipelineKind::Draw2D:
        desc.debugName = "draw2d";
        desc.depthTest = false;
        desc.depthWrite = false;
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

    desc.usesFog = key.fogBits != FogNone;
    desc.usesSkyFog = key.fogSkyBits != FogNone;
    desc.usesDynamicLights = key.perPixelLighting && key.dynamicLights;

    auto appendFogSuffix = [&](const char *label, FogBits bits) {
        std::string fog = describeFogBits(bits);
        if (!fog.empty()) {
            desc.debugName.push_back('.');
            desc.debugName.append(label);
            desc.debugName.append(fog);
        }
    };

    appendFogSuffix("fog", key.fogBits);
    if (key.fogSkyBits != FogNone) {
        appendFogSuffix("sky", key.fogSkyBits);
    }
    if (key.perPixelLighting) {
        desc.debugName.append(".ppl");
    }
    if (key.dynamicLights) {
        desc.debugName.append(".dl");
    }

    return desc;
}
VulkanRenderer::PipelineKey VulkanRenderer::buildPipelineKey(PipelineKind kind) const {
    PipelineKey key{};
    key.kind = kind;

    switch (kind) {
    case PipelineKind::InlineBsp:
        key.fogBits = frameState_.fogBits;
        key.fogSkyBits = frameState_.fogBitsSky;
        key.perPixelLighting = frameState_.perPixelLighting;
        key.dynamicLights = frameState_.dynamicLightsUploaded;
        break;
    case PipelineKind::Alias:
    case PipelineKind::Sprite:
    case PipelineKind::Weapon:
        key.fogBits = frameState_.fogBits;
        key.perPixelLighting = frameState_.perPixelLighting;
        key.dynamicLights = frameState_.dynamicLightsUploaded;
        break;
    default:
        break;
    }

    return key;
}
const VulkanRenderer::PipelineDesc &VulkanRenderer::ensurePipeline(const PipelineKey &key) {
    if (auto it = pipelines_.find(key); it != pipelines_.end()) {
        return it->second;
    }

    PipelineDesc desc = makePipeline(key);
    auto [it, inserted] = pipelines_.emplace(key, std::move(desc));
    if (inserted) {
        frameStats_.pipelinesBound += 1;
    }
    if (pipelineLibrary_) {
        pipelineLibrary_->requestPipeline(key);
    }
    return it->second;
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

    static cvar_t *drawEntities = Cvar_FindVar("gl_drawentities");
    if (drawEntities && drawEntities->integer == 0) {
        return;
    }

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
void VulkanRenderer::sortTransparentQueues(const refdef_t &fd) {
    if (frameQueues_.alphaBack.empty() && frameQueues_.alphaFront.empty()) {
        return;
    }

    const vec3_t &viewOrg = fd.vieworg;
    auto distanceSquared = [&](const entity_t *ent) {
        if (!ent) {
            return std::numeric_limits<float>::infinity();
        }

        float dx = ent->origin[0] - viewOrg[0];
        float dy = ent->origin[1] - viewOrg[1];
        float dz = ent->origin[2] - viewOrg[2];
        float dist = dx * dx + dy * dy + dz * dz;
        if (!std::isfinite(dist)) {
            return std::numeric_limits<float>::infinity();
        }
        return dist;
    };

    auto sortByDepth = [&](std::vector<const entity_t *> &queue) {
        if (queue.size() < 2) {
            return;
        }

        std::stable_sort(queue.begin(), queue.end(), [&](const entity_t *lhs, const entity_t *rhs) {
            float left = distanceSquared(lhs);
            float right = distanceSquared(rhs);
            if (left == right) {
                return lhs < rhs;
            }
            return left < right;
        });
    };

    sortByDepth(frameQueues_.alphaBack);
    sortByDepth(frameQueues_.alphaFront);
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

    if (!initialized_ || device_ == VK_NULL_HANDLE || inFlightFrames_.empty()) {
        return;
    }

    if (currentFrameIndex_ >= inFlightFrames_.size()) {
        return;
    }

    InFlightFrame &frame = inFlightFrames_[currentFrameIndex_];
    if (frame.commandBuffer == VK_NULL_HANDLE || !frame.hasImage) {
        return;
    }

    VkCommandBuffer commandBuffer = frame.commandBuffer;

    if (pipelineLibrary_) {
        PipelineKey pipelineKey = buildPipelineKey(PipelineKind::Draw2D);
        VkPipeline pipelineHandle = pipelineLibrary_->requestPipeline(pipelineKey);
        if (pipelineHandle != VK_NULL_HANDLE) {
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineHandle);
        }
    }

    Draw2DBatch batch{};
    batch.vertexCount = submission.vertexCount;
    batch.indexCount = submission.indexCount;
    batch.texture = submission.texture;

    const ImageRecord *image = findImageRecord(submission.texture);
    if (!image) {
        if (submission.texture == rawTextureHandle_) {
            ensureRawTexture();
            image = findImageRecord(submission.texture);
        } else {
            qhandle_t fallback = ensureWhiteTexture();
            image = findImageRecord(fallback);
        }
    }
    VkDescriptorSet textureDescriptor = image ? image->descriptorSet : VK_NULL_HANDLE;

    const VkDeviceSize vertexSize = static_cast<VkDeviceSize>(submission.vertexCount * sizeof(draw2d::Vertex));
    const VkDeviceSize indexSize = static_cast<VkDeviceSize>(submission.indexCount * sizeof(uint16_t));

    if (vertexSize == 0 || indexSize == 0) {
        return;
    }

    if (!createBuffer(batch.vertex,
                      vertexSize,
                      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
        return;
    }

    void *mappedVertices = nullptr;
    if (vkMapMemory(device_, batch.vertex.memory, batch.vertex.offset, batch.vertex.size, 0, &mappedVertices) != VK_SUCCESS ||
        mappedVertices == nullptr) {
        destroyBuffer(batch.vertex);
        return;
    }
    std::memcpy(mappedVertices, submission.vertices, static_cast<size_t>(vertexSize));
    vkUnmapMemory(device_, batch.vertex.memory);

    if (!createBuffer(batch.index,
                      indexSize,
                      VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
        destroyBuffer(batch.vertex);
        return;
    }

    void *mappedIndices = nullptr;
    if (vkMapMemory(device_, batch.index.memory, batch.index.offset, batch.index.size, 0, &mappedIndices) != VK_SUCCESS ||
        mappedIndices == nullptr) {
        destroyBuffer(batch.vertex);
        destroyBuffer(batch.index);
        return;
    }
    std::memcpy(mappedIndices, submission.indices, static_cast<size_t>(indexSize));
    vkUnmapMemory(device_, batch.index.memory);

    batch.descriptor.vertex.buffer = batch.vertex.buffer;
    batch.descriptor.vertex.offset = batch.vertex.offset;
    batch.descriptor.vertex.range = batch.vertex.size;
    batch.descriptor.index.buffer = batch.index.buffer;
    batch.descriptor.index.offset = batch.index.offset;
    batch.descriptor.index.range = batch.index.size;

    if (descriptorPool_ != VK_NULL_HANDLE && createModelDescriptorResources()) {
        if (batch.descriptor.set == VK_NULL_HANDLE) {
            VkDescriptorSetAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool = descriptorPool_;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = &modelDescriptorSetLayout_;
            VkResult allocResult = vkAllocateDescriptorSets(device_, &allocInfo, &batch.descriptor.set);
            if (allocResult != VK_SUCCESS) {
                batch.descriptor.set = VK_NULL_HANDLE;
            }
        }

        if (batch.descriptor.set != VK_NULL_HANDLE) {
            std::array<VkWriteDescriptorSet, 2> writes{};

            VkWriteDescriptorSet &vertexWrite = writes[0];
            vertexWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            vertexWrite.dstSet = batch.descriptor.set;
            vertexWrite.dstBinding = 0;
            vertexWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            vertexWrite.descriptorCount = 1;
            vertexWrite.pBufferInfo = &batch.descriptor.vertex;

            VkWriteDescriptorSet &indexWrite = writes[1];
            indexWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            indexWrite.dstSet = batch.descriptor.set;
            indexWrite.dstBinding = 1;
            indexWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            indexWrite.descriptorCount = 1;
            indexWrite.pBufferInfo = &batch.descriptor.index;

            vkUpdateDescriptorSets(device_, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
        }
    }

    if (batch.vertex.buffer != VK_NULL_HANDLE) {
        VkBuffer buffers[] = { batch.vertex.buffer };
        VkDeviceSize offsets[] = { batch.vertex.offset };
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);
    }

    if (batch.index.buffer != VK_NULL_HANDLE) {
        vkCmdBindIndexBuffer(commandBuffer, batch.index.buffer, batch.index.offset, batch.indexType);
    }

    if (batch.descriptor.set != VK_NULL_HANDLE && modelPipelineLayout_ != VK_NULL_HANDLE) {
        std::array<VkDescriptorSet, 2> descriptorSets{ batch.descriptor.set, textureDescriptor };
        uint32_t descriptorCount = 1;

        if (textureDescriptor != VK_NULL_HANDLE && textureDescriptorSetLayout_ != VK_NULL_HANDLE) {
            descriptorCount = 2;
        }

        vkCmdBindDescriptorSets(commandBuffer,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                modelPipelineLayout_,
                                0,
                                descriptorCount,
                                descriptorSets.data(),
                                0,
                                nullptr);
    }

    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(submission.indexCount), 1, 0, 0, 0);

    frame2DBatches_.push_back(batch);

    const PipelineDesc &pipeline = ensurePipeline(buildPipelineKey(PipelineKind::Draw2D));
    recordDrawCall(pipeline, "draw2d", submission.indexCount / 3);
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

        if (mesh.numskins > 0 && mesh.skins) {
            geometry.skinHandles.reserve(static_cast<size_t>(mesh.numskins));
            for (int skinIndex = 0; skinIndex < mesh.numskins; ++skinIndex) {
                qhandle_t skinHandle = 0;
                if (image_t *skin = mesh.skins[skinIndex]) {
                    imagetype_t type = static_cast<imagetype_t>(skin->type);
                    imageflags_t flags = static_cast<imageflags_t>(skin->flags);
                    skinHandle = registerImage(skin->name, type, flags);
                }
                if (!skinHandle) {
                    skinHandle = ensureWhiteTexture();
                }
                geometry.skinHandles.push_back(skinHandle);
            }
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

    for (size_t meshIndex = 0; meshIndex < record.meshGeometry.size(); ++meshIndex) {
        auto &geometry = record.meshGeometry[meshIndex];

        if (!geometry.uploaded && (!geometry.vertexStaging.empty() || !geometry.indexStaging.empty())) {
            if (!uploadMeshGeometry(geometry)) {
                continue;
            }
        }
    }
}
VulkanRenderer::EntityPushConstants VulkanRenderer::buildEntityPushConstants(const entity_t &entity,
                                                                            const ModelRecord &) const {
    EntityPushConstants constants{};

    vec3_t axis[3]{};
    AnglesToAxis(entity.angles, axis);

    vec3_t scale{1.0f, 1.0f, 1.0f};
    if (!VectorCompare(entity.scale, vec3_origin)) {
        scale[0] = (entity.scale[0] == 0.0f) ? 1.0f : entity.scale[0];
        scale[1] = (entity.scale[1] == 0.0f) ? 1.0f : entity.scale[1];
        scale[2] = (entity.scale[2] == 0.0f) ? 1.0f : entity.scale[2];
    }

    constants.modelMatrix = {
        axis[0][0] * scale[0], axis[1][0] * scale[0], axis[2][0] * scale[0], 0.0f,
        axis[0][1] * scale[1], axis[1][1] * scale[1], axis[2][1] * scale[1], 0.0f,
        axis[0][2] * scale[2], axis[1][2] * scale[2], axis[2][2] * scale[2], 0.0f,
        entity.origin[0], entity.origin[1], entity.origin[2], 1.0f,
    };

    std::array<float, 4> baseColor = toColorArray(entity.rgba);
    float entityAlpha = std::clamp(entity.alpha, 0.0f, 1.0f);
    if (entity.flags & RF_TRANSLUCENT) {
        baseColor[3] = entityAlpha;
    } else {
        baseColor[3] = 1.0f;
    }
    constants.color = baseColor;

    vec3_t lightColor{};
    lightPoint(entity.origin, lightColor);
    constants.lighting = { lightColor[0], lightColor[1], lightColor[2], 1.0f };

    constants.misc = { entity.backlerp, entityAlpha, 0.0f, 0.0f };

    constants.indices[0] = static_cast<uint32_t>(entity.frame);
    constants.indices[1] = static_cast<uint32_t>(entity.oldframe);
    constants.indices[2] = static_cast<uint32_t>(entity.flags & 0xFFFFFFFFu);
    constants.indices[3] = static_cast<uint32_t>((entity.flags >> 32) & 0xFFFFFFFFu);

    return constants;
}
VkDescriptorSet VulkanRenderer::selectTextureDescriptor(const entity_t &entity,
                                                        const ModelRecord::MeshGeometry &geometry) {
    const ImageRecord *image = nullptr;

    if (entity.skin) {
        image = findImageRecord(entity.skin);
    }

    if (!image && !geometry.skinHandles.empty()) {
        int skinIndex = entity.skinnum;
        if (skinIndex < 0 || skinIndex >= static_cast<int>(geometry.skinHandles.size())) {
            skinIndex = 0;
        }
        qhandle_t handle = geometry.skinHandles[static_cast<size_t>(skinIndex)];
        image = findImageRecord(handle);
    }

    if (!image) {
        qhandle_t fallback = ensureWhiteTexture();
        image = findImageRecord(fallback);
    }

    if (image) {
        return image->descriptorSet;
    }

    return VK_NULL_HANDLE;
}
qhandle_t VulkanRenderer::registerImage(const char *name, imagetype_t type, imageflags_t flags) {
    if (!name || !*name) {
        return 0;
    }

    image_pixels_t pixels{};
    if (!IMG_LoadPixels(name, type, flags, &pixels)) {
        return 0;
    }

    std::string canonical = pixels.name[0] ? pixels.name : std::string{name};
    qhandle_t handle = registerResource(imageLookup_, canonical);
    if (!handle) {
        IMG_FreePixels(&pixels);
        return 0;
    }

    auto &record = images_[handle];
    destroyImageRecord(record);

    record.handle = handle;
    record.name = canonical;
    record.type = static_cast<imagetype_t>(pixels.type);
    record.flags = static_cast<imageflags_t>(pixels.flags);
    record.width = pixels.width;
    record.height = pixels.height;
    record.uploadWidth = pixels.upload_width;
    record.uploadHeight = pixels.upload_height;
    record.transparent = (record.flags & IF_TRANSPARENT) != 0;
    record.registrationSequence = r_registration_sequence;

    uint32_t texWidth = static_cast<uint32_t>(std::max(1, record.uploadWidth));
    uint32_t texHeight = static_cast<uint32_t>(std::max(1, record.uploadHeight));
    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
    const uint8_t *pixelData = reinterpret_cast<const uint8_t *>(pixels.pixels);
    size_t pixelSize = pixels.size;

    bool uploaded = (pixelData && pixelSize > 0) &&
                    ensureTextureResources(record, pixelData, pixelSize, texWidth, texHeight, format);

    IMG_FreePixels(&pixels);

    if (!uploaded) {
        images_.erase(handle);
        imageLookup_.erase(canonical);
        imageLookup_.erase(std::string{name});
        return 0;
    }

    imageLookup_[canonical] = handle;
    if (canonical != name) {
        imageLookup_[std::string{name}] = handle;
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

    refreshSwapInterval();
    if (swapchain_ == VK_NULL_HANDLE) {
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

    if (lastSubmittedFrame_.has_value() && *lastSubmittedFrame_ == currentFrameIndex_) {
        lastSubmittedFrame_.reset();
    }

    clear2DBatches();

    uint32_t imageIndex = 0;
    VkResult acquireResult = vkAcquireNextImageKHR(device_, swapchain_, std::numeric_limits<uint64_t>::max(), frame.imageAvailable, VK_NULL_HANDLE, &imageIndex);

    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
        rebuildSwapchain();
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

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
    clearValues[1].depthStencil = { 1.0f, 0 };

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass_;
    renderPassInfo.framebuffer = (frame.imageIndex < swapchainFramebuffers_.size()) ? swapchainFramebuffers_[frame.imageIndex] : VK_NULL_HANDLE;
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = swapchainExtent_;
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    if (renderPassInfo.framebuffer == VK_NULL_HANDLE) {
        Com_Printf("refresh-vk: framebuffer unavailable for frame.\n");
        vkEndCommandBuffer(frame.commandBuffer);
        return;
    }

    vkCmdBeginRenderPass(frame.commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    frameActive_ = true;
    frameAcquired_ = true;
    frameRenderPassActive_ = false;
    draw2DBegun_ = false;

    if (draw2d::isActive()) {
        draw2d::end();
    }
}
void VulkanRenderer::endFrame() {
    if (!initialized_ || !frameActive_) {
        return;
    }

    if (draw2DBegun_) {
        draw2d::end();
        draw2DBegun_ = false;
    }
    finishFrameRecording();

    if (inFlightFrames_.empty()) {
        frameActive_ = false;
        frameAcquired_ = false;
        commandLog_.clear();
        frameStats_.reset();
        return;
    }

    InFlightFrame &frame = inFlightFrames_[currentFrameIndex_];
    size_t submittedIndex = currentFrameIndex_;
    if (!frame.hasImage || frame.commandBuffer == VK_NULL_HANDLE) {
        frameActive_ = false;
        frameAcquired_ = false;
        commandLog_.clear();
        frameStats_.reset();
        return;
    }

    VkSemaphore waitSemaphores[] = { frame.imageAvailable };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT };
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

    lastSubmittedFrame_ = submittedIndex;

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

    if (currentFrameIndex_ >= inFlightFrames_.size()) {
        return;
    }

    InFlightFrame &frame = inFlightFrames_[currentFrameIndex_];
    if (!frame.hasImage || frame.commandBuffer == VK_NULL_HANDLE) {
        return;
    }

    VkCommandBuffer commandBuffer = frame.commandBuffer;
    VkImage swapchainImage = (frame.imageIndex < swapchainImages_.size()) ? swapchainImages_[frame.imageIndex] : VK_NULL_HANDLE;

    auto beginPass = [&](VkRenderPass pass, VkFramebuffer framebuffer, VkExtent2D extent, const VkClearColorValue &clearColor) {
        if (commandBuffer == VK_NULL_HANDLE || pass == VK_NULL_HANDLE || framebuffer == VK_NULL_HANDLE) {
            return false;
        }

        VkRenderPassBeginInfo info{};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        info.renderPass = pass;
        info.framebuffer = framebuffer;
        info.renderArea.offset = { 0, 0 };
        info.renderArea.extent = extent;

        VkClearValue clear{};
        clear.color = clearColor;
        info.clearValueCount = 1;
        info.pClearValues = &clear;

        vkCmdBeginRenderPass(commandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
        frameRenderPassActive_ = true;

        if (!draw2DBegun_) {
            if (!draw2d::begin([this](const draw2d::Submission &submission) {
                    submit2DDraw(submission);
                })) {
                Com_Printf("vk_draw2d: failed to begin 2D batch for frame.\n");
            } else {
                draw2DBegun_ = true;
            }
        }

        return true;
    };

    auto endPass = [&]() {
        if (!frameRenderPassActive_) {
            return;
        }

        if (draw2d::isActive()) {
            draw2d::flush();
        }

        vkCmdEndRenderPass(commandBuffer);
        frameRenderPassActive_ = false;
    };

    VkClearColorValue clearColor{ { 0.0f, 0.0f, 0.0f, 1.0f } };

    prepareFrameState(*fd);
    evaluateFrameSettings();
    uploadDynamicLights();
    updateSkyState();

    bool worldVisible = (fd->rdflags & RDF_NOWORLDMODEL) == 0;
    bool waterwarpEnabled = resolveToggle(vk_waterwarp, "gl_waterwarp", true);
    bool waterwarp = waterwarpEnabled && (fd->rdflags & RDF_UNDERWATER) != 0;
    bool bloomEnabled = resolveToggle(vk_bloom, "gl_bloom", true);
    bool bloom = bloomEnabled && worldVisible;
    bool overlayEnabled = resolveToggle(vk_polyblend, "gl_polyblend", true);
    bool overlayBlend = overlayEnabled && (fd->screen_blend[3] > 0.0f || fd->damage_blend[3] > 0.0f);

    frameState_.waterwarpActive = waterwarp;
    frameState_.bloomActive = bloom;
    frameState_.overlayBlendActive = overlayBlend;

    bool usePostProcess = postProcessAvailable_ && (waterwarp || bloom || overlayBlend);
    bool offscreenActive = false;

    if (usePostProcess && sceneTarget_.framebuffer != VK_NULL_HANDLE && offscreenRenderPass_ != VK_NULL_HANDLE) {
        if (!sceneTargetReady_ && sceneTarget_.image != VK_NULL_HANDLE) {
            transitionImageLayout(commandBuffer,
                                   sceneTarget_.image,
                                   VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            sceneTargetReady_ = true;
        }
        if (beginPass(offscreenRenderPass_, sceneTarget_.framebuffer, sceneTarget_.extent, clearColor)) {
            offscreenActive = true;
        }
    }

    if (!offscreenActive) {
        if (swapchainImage != VK_NULL_HANDLE) {
            transitionImageLayout(commandBuffer,
                                   swapchainImage,
                                   VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            VkImageSubresourceRange clearRange{};
            clearRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            clearRange.baseMipLevel = 0;
            clearRange.levelCount = 1;
            clearRange.baseArrayLayer = 0;
            clearRange.layerCount = 1;

            vkCmdClearColorImage(commandBuffer,
                                 swapchainImage,
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                 &clearColor,
                                 1,
                                 &clearRange);

            transitionImageLayout(commandBuffer,
                                   swapchainImage,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        }

        VkFramebuffer framebufferHandle = (frame.imageIndex < swapchainFramebuffers_.size()) ?
                                              swapchainFramebuffers_[frame.imageIndex] :
                                              VK_NULL_HANDLE;
        if (!beginPass(renderPass_, framebufferHandle, swapchainExtent_, clearColor)) {
            return;
        }
    }

    beginWorldPass();
    if (!(frameState_.refdef.rdflags & RDF_NOWORLDMODEL)) {
        renderWorld();
    }
    endWorldPass();
    recordStage("frame.begin");

    classifyEntities(*fd);
    sortTransparentQueues(frameState_.refdef);
    buildEffectBuffers(*fd);

    if (!(fd->rdflags & RDF_NOWORLDMODEL)) {
        recordStage("world.draw");
    }

    auto processQueue = [&](const std::vector<const entity_t *> &queue, std::string_view label, bool backToFront) {
        if (queue.empty()) {
            return;
        }

        VkCommandBuffer commandBuffer = frame.commandBuffer;
        if (commandBuffer == VK_NULL_HANDLE) {
            return;
        }

        PipelineKey currentKey{};
        const PipelineDesc *currentPipelineDesc = nullptr;
        VkPipeline currentPipelineHandle = VK_NULL_HANDLE;
        size_t drawCountForPipeline = 0;

        auto flushDrawLog = [&]() {
            if (currentPipelineDesc && drawCountForPipeline > 0) {
                recordDrawCall(*currentPipelineDesc, label, drawCountForPipeline);
            }
            drawCountForPipeline = 0;
        };

        auto iterateQueue = [&](auto &&callback) {
            if (backToFront) {
                for (auto it = queue.rbegin(); it != queue.rend(); ++it) {
                    callback(*it);
                }
            } else {
                for (const entity_t *entity : queue) {
                    callback(entity);
                }
            }
        };

        iterateQueue([&](const entity_t *entity) {
            if (!entity) {
                return;
            }

            ModelRecord *record = findModelRecord(entity->model);
            if (!record || record->meshGeometry.empty()) {
                return;
            }

            bindModelGeometryBuffers(*record);

            PipelineKind kind = selectPipelineForEntity(*entity);
            PipelineKey key = buildPipelineKey(kind);

            if (!currentPipelineDesc || !(key == currentKey)) {
                flushDrawLog();
                currentKey = key;
                currentPipelineDesc = &ensurePipeline(currentKey);
                currentPipelineHandle = VK_NULL_HANDLE;

                if (pipelineLibrary_) {
                    currentPipelineHandle = pipelineLibrary_->requestPipeline(currentKey);
                }

                if (currentPipelineHandle != VK_NULL_HANDLE) {
                    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, currentPipelineHandle);

                    std::string entry{"bind.pipeline."};
                    entry.append(currentPipelineDesc->debugName);
                    commandLog_.push_back(std::move(entry));
                }
            }

            if (currentPipelineHandle == VK_NULL_HANDLE) {
                return;
            }

            EntityPushConstants constants = buildEntityPushConstants(*entity, *record);
            if (modelPipelineLayout_ != VK_NULL_HANDLE) {
                vkCmdPushConstants(commandBuffer,
                                   modelPipelineLayout_,
                                   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                   0,
                                   sizeof(EntityPushConstants),
                                   &constants);
            }

            for (size_t meshIndex = 0; meshIndex < record->meshGeometry.size(); ++meshIndex) {
                auto &geometry = record->meshGeometry[meshIndex];

                if (geometry.vertex.buffer == VK_NULL_HANDLE && geometry.index.buffer == VK_NULL_HANDLE) {
                    continue;
                }

                if (geometry.vertex.buffer != VK_NULL_HANDLE && geometry.vertex.size > 0) {
                    VkBuffer buffers[] = { geometry.vertex.buffer };
                    VkDeviceSize offsets[] = { geometry.vertex.offset };
                    vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);
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

                VkDescriptorSet textureSet = selectTextureDescriptor(*entity, geometry);
                if (textureSet != VK_NULL_HANDLE && modelPipelineLayout_ != VK_NULL_HANDLE &&
                    textureDescriptorSetLayout_ != VK_NULL_HANDLE) {
                    vkCmdBindDescriptorSets(commandBuffer,
                                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                                            modelPipelineLayout_,
                                            1,
                                            1,
                                            &textureSet,
                                            0,
                                            nullptr);
                }

                bool submitted = false;
                if (geometry.index.buffer != VK_NULL_HANDLE && geometry.indexCount > 0) {
                    vkCmdBindIndexBuffer(commandBuffer, geometry.index.buffer, geometry.index.offset, geometry.indexType);
                    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(geometry.indexCount), 1, 0, 0, 0);
                    submitted = true;
                } else if (geometry.vertexCount > 0) {
                    vkCmdDraw(commandBuffer, static_cast<uint32_t>(geometry.vertexCount), 1, 0, 0);
                    submitted = true;
                }

                if (submitted) {
                    ++drawCountForPipeline;

                    std::string entry{"draw.model."};
                    entry.append(record->name);
                    entry.push_back('#');
                    entry.append(std::to_string(meshIndex));
                    commandLog_.push_back(std::move(entry));
                }
            }
        });

        flushDrawLog();
    };

    processQueue(frameQueues_.bmodels, "entities.inline", false);
    processQueue(frameQueues_.opaque, "entities.opaque", false);
    processQueue(frameQueues_.alphaBack, "entities.alpha_back", true);

    ViewParameters viewParams = computeViewParameters(*fd);

    bool cylindricalBeams = gl_beamstyle && gl_beamstyle->integer != 0;
    streamBeamPrimitives(viewParams, cylindricalBeams);
    if (!effectStreams_.beamIndices.empty()) {
        PipelineKind beamKind = cylindricalBeams ? PipelineKind::BeamCylindrical : PipelineKind::BeamSimple;
        recordDrawCall(ensurePipeline(buildPipelineKey(beamKind)), "fx.beams", framePrimitives_.beams.size());
    }

    bool additiveParticles = gl_partstyle && gl_partstyle->integer != 0;
    streamParticlePrimitives(viewParams, additiveParticles);
    if (!effectStreams_.particleVertices.empty()) {
        PipelineKind particleKind = additiveParticles ? PipelineKind::ParticleAdditive : PipelineKind::ParticleAlpha;
        recordDrawCall(ensurePipeline(buildPipelineKey(particleKind)), "fx.particles", framePrimitives_.particles.size());
    }

    bool flaresEnabled = true;
    if (cl_flares) {
        flaresEnabled = cl_flares->integer != 0;
    }
    if (flaresEnabled) {
        streamFlarePrimitives(viewParams);
        if (!effectStreams_.flareVertices.empty()) {
            recordDrawCall(ensurePipeline(buildPipelineKey(PipelineKind::Flare)), "fx.flares", framePrimitives_.flares.size());
        }
    } else {
        effectStreams_.flareVertices.clear();
        effectStreams_.flareIndices.clear();
        frameStats_.flares = 0;
    }

    processQueue(frameQueues_.alphaFront, "entities.alpha_front", true);

    streamDebugLinePrimitives();
    if (!effectStreams_.debugLinesDepth.empty()) {
        recordDrawCall(ensurePipeline(buildPipelineKey(PipelineKind::DebugLineDepth)), "debug.lines.depth", effectStreams_.debugLinesDepth.size() / 2);
    }
    if (!effectStreams_.debugLinesNoDepth.empty()) {
        recordDrawCall(ensurePipeline(buildPipelineKey(PipelineKind::DebugLineNoDepth)), "debug.lines.nodepth", effectStreams_.debugLinesNoDepth.size() / 2);
    }

    if (offscreenActive) {
        endPass();

        if (sceneTarget_.image != VK_NULL_HANDLE && swapchainImage != VK_NULL_HANDLE) {
            transitionImageLayout(commandBuffer,
                                   sceneTarget_.image,
                                   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            transitionImageLayout(commandBuffer,
                                   swapchainImage,
                                   VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            VkImageBlit region{};
            region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.srcSubresource.mipLevel = 0;
            region.srcSubresource.baseArrayLayer = 0;
            region.srcSubresource.layerCount = 1;
            region.srcOffsets[0] = { 0, 0, 0 };
            region.srcOffsets[1] = { static_cast<int32_t>(sceneTarget_.extent.width), static_cast<int32_t>(sceneTarget_.extent.height), 1 };

            region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.dstSubresource.mipLevel = 0;
            region.dstSubresource.baseArrayLayer = 0;
            region.dstSubresource.layerCount = 1;
            region.dstOffsets[0] = { 0, 0, 0 };
            region.dstOffsets[1] = { static_cast<int32_t>(swapchainExtent_.width), static_cast<int32_t>(swapchainExtent_.height), 1 };

            vkCmdBlitImage(commandBuffer,
                           sceneTarget_.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1, &region,
                           VK_FILTER_LINEAR);

            transitionImageLayout(commandBuffer,
                                   swapchainImage,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            transitionImageLayout(commandBuffer,
                                   sceneTarget_.image,
                                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        }

        VkFramebuffer framebufferHandle = (frame.imageIndex < swapchainFramebuffers_.size()) ?
                                              swapchainFramebuffers_[frame.imageIndex] :
                                              VK_NULL_HANDLE;
        if (!beginPass(renderPass_, framebufferHandle, swapchainExtent_, clearColor)) {
            return;
        }
    }

    if (frameState_.waterwarpActive) {
        recordStage("post.waterwarp");
    }

    if (frameState_.bloomActive && frameState_.worldRendered) {
        recordStage("post.bloom");
    }

    if (frameState_.overlayBlendActive) {
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
void VulkanRenderer::resetFrameState() {
    clearFrameTransientQueues();
    resetFrameStatistics();

    frameState_.refdef = {};
    frameState_.entities.clear();
    frameState_.dlights.clear();
    frameState_.dynamicLights.clear();
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
    frameState_.fogEnabled = false;
    frameState_.fogSkyEnabled = false;
    frameState_.perPixelLighting = false;
    frameState_.dynamicLightsEnabled = false;
    frameState_.dynamicLightCount = 0;
    frameState_.waterwarpActive = false;
    frameState_.bloomActive = false;
    frameState_.overlayBlendActive = false;
}
void VulkanRenderer::prepareFrameState(const refdef_t &fd) {
    frameState_.refdef = fd;

    frameState_.entities.clear();
    frameState_.dynamicLights.clear();
    frameState_.dynamicLightCount = 0;
    frameState_.dynamicLightsUploaded = false;
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

    frameState_.fogEnabled = resolveToggle(vk_fog, "gl_fog", true);
    if (frameState_.fogEnabled && frameState_.refdef.fog.density > 0.0f) {
        frameState_.fogBits = static_cast<FogBits>(frameState_.fogBits | FogGlobal);
    }

    if (frameState_.fogEnabled && frameState_.refdef.heightfog.density > 0.0f && frameState_.refdef.heightfog.falloff > 0.0f) {
        frameState_.fogBits = static_cast<FogBits>(frameState_.fogBits | FogHeight);
    }

    if (frameState_.fogEnabled && frameState_.refdef.fog.sky_factor > 0.0f) {
        frameState_.fogBitsSky = static_cast<FogBits>(frameState_.fogBitsSky | FogSky);
    }
    frameState_.fogSkyEnabled = frameState_.fogBitsSky != FogNone;

    bool dynamicAllowed = resolveToggle(vk_dynamic, "gl_dynamic", true);
    bool perPixelAllowed = resolveToggle(vk_perPixelLighting, "gl_per_pixel_lighting", true);
    frameState_.dynamicLightsEnabled = dynamicAllowed && perPixelAllowed;

    if (!dynamicAllowed) {
        frameState_.refdef.num_dlights = 0;
    }

    frameState_.perPixelLighting = frameState_.dynamicLightsEnabled;
}
void VulkanRenderer::uploadDynamicLights() {
    frameState_.dynamicLights.clear();
    frameState_.dynamicLightCount = 0;
    frameState_.dynamicLightsUploaded = false;

    if (!frameState_.dynamicLightsEnabled || frameState_.dlights.empty()) {
        return;
    }

    size_t count = std::min(frameState_.dlights.size(), static_cast<size_t>(MAX_DLIGHTS));
    frameState_.dynamicLights.reserve(count);

    for (size_t i = 0; i < count; ++i) {
        const dlight_t &src = frameState_.dlights[i];
        FrameState::DynamicLightGPU gpu{};
        gpu.positionRadius = { src.origin[0], src.origin[1], src.origin[2], src.radius };
        gpu.colorIntensity = { src.color[0], src.color[1], src.color[2], src.intensity };
        gpu.cone = { src.cone[0], src.cone[1], src.cone[2], src.cone[3] };
        frameState_.dynamicLights.emplace_back(gpu);
    }

    frameState_.dynamicLightCount = static_cast<int>(count);
    frameState_.dynamicLightsUploaded = count > 0;

    if (frameState_.dynamicLightsUploaded) {
        std::string label = "lighting.dynamic.upload";
        label.push_back('(');
        label.append(std::to_string(frameState_.dynamicLightCount));
        label.push_back(')');
        recordStage(label);
    }
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

} // namespace refresh::vk
