#include "pipeline.h"

#include "renderer.h"

#include "renderer/common.h"

#include "common/files.h"

#include "vk_draw2d.h"

#include <array>
#include <cstddef>

namespace refresh::vk {

namespace {

constexpr std::string_view shaderStageSuffix(VkShaderStageFlagBits stage) {
    switch (stage) {
    case VK_SHADER_STAGE_VERTEX_BIT:
        return ".vert.spv";
    case VK_SHADER_STAGE_FRAGMENT_BIT:
        return ".frag.spv";
    default:
        return ".spv";
    }
}

} // namespace

PipelineLibrary::PipelineLibrary(VulkanRenderer &renderer)
    : renderer_{renderer} {}

PipelineLibrary::~PipelineLibrary() {
    shutdown();
}

bool PipelineLibrary::initialize() {
    pipelines_.clear();
    return true;
}

void PipelineLibrary::shutdown() {
    for (auto &[key, pipeline] : pipelines_) {
        destroyPipelineObject(pipeline);
    }
    pipelines_.clear();
}

VkPipeline PipelineLibrary::requestPipeline(const VulkanRenderer::PipelineKey &key) {
    if (auto it = pipelines_.find(key); it != pipelines_.end()) {
        return it->second.handle;
    }

    PipelineObject object{};
    object.desc = renderer_.makePipeline(key);

    if (!createGraphicsPipeline(object)) {
        destroyPipelineObject(object);
        pipelines_.emplace(key, PipelineObject{});
        return VK_NULL_HANDLE;
    }

    auto [it, inserted] = pipelines_.emplace(key, std::move(object));
    if (!inserted) {
        destroyPipelineObject(it->second);
        it->second = std::move(object);
    }

    return it->second.handle;
}

PipelineLibrary::ShaderModule PipelineLibrary::loadShaderModule(std::string_view logicalName,
                                                                VkShaderStageFlagBits stage) {
    ShaderModule module{};

    std::string filename;
    filename.reserve(logicalName.size() + 16);
    filename.append("vk/");
    filename.append(logicalName);
    filename.append(shaderStageSuffix(stage));

    void *rawData = nullptr;
    int fileLength = FS_LoadFile(filename.c_str(), &rawData);
    if (fileLength <= 0 || !rawData) {
        std::string altName{logicalName};
        altName.append(shaderStageSuffix(stage));
        fileLength = FS_LoadFile(altName.c_str(), &rawData);
        if (fileLength <= 0 || !rawData) {
            Com_Printf("refresh-vk: unable to load shader module %s.\n", filename.c_str());
            return module;
        }
        filename = std::move(altName);
    }

    uint8_t *fileData = static_cast<uint8_t *>(rawData);
    if (fileLength <= 0 || !fileData) {
        Com_Printf("refresh-vk: failed to read shader module %s.\n", filename.c_str());
        return module;
    }

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = static_cast<size_t>(fileLength);
    createInfo.pCode = reinterpret_cast<const uint32_t *>(fileData);

    VkDevice device = renderer_.device();
    if (device == VK_NULL_HANDLE) {
        FS_FreeFile(fileData);
        return module;
    }

    VkResult result = vkCreateShaderModule(device, &createInfo, nullptr, &module.handle);
    if (result != VK_SUCCESS || module.handle == VK_NULL_HANDLE) {
        Com_Printf("refresh-vk: failed to create shader module %s (VkResult %d).\n",
                   filename.c_str(),
                   static_cast<int>(result));
        module.handle = VK_NULL_HANDLE;
    }

    module.name = filename;
    FS_FreeFile(rawData);
    return module;
}

void PipelineLibrary::destroyShaderModule(ShaderModule &module) const {
    if (module.handle == VK_NULL_HANDLE) {
        return;
    }
    VkDevice device = renderer_.device();
    if (device != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, module.handle, nullptr);
    }
    module.handle = VK_NULL_HANDLE;
    module.name.clear();
}

void PipelineLibrary::destroyPipelineObject(PipelineObject &object) {
    VkDevice device = renderer_.device();
    if (device != VK_NULL_HANDLE && object.handle != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, object.handle, nullptr);
        object.handle = VK_NULL_HANDLE;
    }
    destroyShaderModule(object.vertex);
    destroyShaderModule(object.fragment);
}

bool PipelineLibrary::createGraphicsPipeline(PipelineObject &object) {
    VkDevice device = renderer_.device();
    VkRenderPass renderPass = renderer_.renderPass();
    VkPipelineLayout layout = renderer_.pipelineLayoutFor(object.desc.key.kind);
    if (device == VK_NULL_HANDLE || renderPass == VK_NULL_HANDLE || layout == VK_NULL_HANDLE) {
        return false;
    }

    object.vertex = loadShaderModule(object.desc.debugName, VK_SHADER_STAGE_VERTEX_BIT);
    object.fragment = loadShaderModule(object.desc.debugName, VK_SHADER_STAGE_FRAGMENT_BIT);
    if (object.vertex.handle == VK_NULL_HANDLE || object.fragment.handle == VK_NULL_HANDLE) {
        return false;
    }

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = object.vertex.handle;
    stages[0].pName = "main";

    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = object.fragment.handle;
    stages[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 0;
    vertexInput.pVertexBindingDescriptions = nullptr;
    vertexInput.vertexAttributeDescriptionCount = 0;
    vertexInput.pVertexAttributeDescriptions = nullptr;

    std::array<VkVertexInputBindingDescription, 1> bindingDescriptions{};
    std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};
    switch (object.desc.key.kind) {
    case VulkanRenderer::PipelineKind::Draw2D: {
        VkVertexInputBindingDescription &binding = bindingDescriptions[0];
        binding.binding = 0;
        binding.stride = sizeof(draw2d::Vertex);
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription &position = attributeDescriptions[0];
        position.location = 0;
        position.binding = 0;
        position.format = VK_FORMAT_R32G32_SFLOAT;
        position.offset = offsetof(draw2d::Vertex, position);

        VkVertexInputAttributeDescription &uv = attributeDescriptions[1];
        uv.location = 1;
        uv.binding = 0;
        uv.format = VK_FORMAT_R32G32_SFLOAT;
        uv.offset = offsetof(draw2d::Vertex, uv);

        VkVertexInputAttributeDescription &color = attributeDescriptions[2];
        color.location = 2;
        color.binding = 0;
        color.format = VK_FORMAT_R8G8B8A8_UNORM;
        color.offset = offsetof(draw2d::Vertex, color);

        vertexInput.vertexBindingDescriptionCount = 1;
        vertexInput.pVertexBindingDescriptions = bindingDescriptions.data();
        vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInput.pVertexAttributeDescriptions = attributeDescriptions.data();
        break;
    }
    default:
        break;
    }

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = object.desc.topology;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = object.desc.depthTest ? VK_TRUE : VK_FALSE;
    depthStencil.depthWriteEnable = object.desc.depthWrite ? VK_TRUE : VK_FALSE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    switch (object.desc.blend) {
    case VulkanRenderer::PipelineDesc::BlendMode::Alpha:
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        break;
    case VulkanRenderer::PipelineDesc::BlendMode::Additive:
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        break;
    default:
        colorBlendAttachment.blendEnable = VK_FALSE;
        break;
    }

    VkPipelineColorBlendStateCreateInfo colorBlend{};
    colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments = &colorBlendAttachment;

    VkDynamicState dynamics[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamics;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisample;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlend;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = layout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;

    VkResult result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &object.handle);
    if (result != VK_SUCCESS || object.handle == VK_NULL_HANDLE) {
        Com_Printf("refresh-vk: failed to create pipeline %s (VkResult %d).\n",
                   object.desc.debugName.c_str(),
                   static_cast<int>(result));
        object.handle = VK_NULL_HANDLE;
        return false;
    }

    return true;
}

} // namespace refresh::vk

