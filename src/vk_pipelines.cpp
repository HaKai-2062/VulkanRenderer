#include <fstream>

#include "vk_pipelines.h"
#include "vk_initializers.h"

void PipelineBuilder::Clear()
{
    m_InputAssembly = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    m_Rasterizer = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    m_ColorBlendAttachment = {};
    m_Multisampling = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    m_PipelineLayout = {};
    m_DepthStencil = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    m_RenderInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };

    m_ShaderStage.clear();
}

VkPipeline PipelineBuilder::BuildPipeline(VkDevice device)
{
    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.pNext = nullptr;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // We dont have transparent object so no blend is set
    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.pNext = nullptr;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &m_ColorBlendAttachment;

    // clears VertexInputStateCreateInfo
    VkPipelineVertexInputStateCreateInfo vertexInputState = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

    VkGraphicsPipelineCreateInfo pipelineInfo = { .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    // connect m_RenderInfo to pNext
    pipelineInfo.pNext = &m_RenderInfo;
    pipelineInfo.stageCount = (uint32_t)m_ShaderStage.size();
    pipelineInfo.pStages = m_ShaderStage.data();
    pipelineInfo.pVertexInputState = &vertexInputState;
    pipelineInfo.pInputAssemblyState = &m_InputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &m_Rasterizer;
    pipelineInfo.pMultisampleState = &m_Multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDepthStencilState = &m_DepthStencil;
    pipelineInfo.layout = m_PipelineLayout;

    VkDynamicState state[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dynamicInfo.pDynamicStates = &state[0];
    dynamicInfo.dynamicStateCount = 2;

    pipelineInfo.pDynamicState = &dynamicInfo;

    VkPipeline newPipeline;
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline) != VK_SUCCESS)
    {
        fmt::print(fmt::fg(fmt::color::red), "Error: Failed to create the new graphics pipeline\n");
        return VK_NULL_HANDLE;
    }

    return newPipeline;
}

void PipelineBuilder::SetShaders(VkShaderModule vertexShader, VkShaderModule fragmentShader)
{
    m_ShaderStage.clear();
    m_ShaderStage.push_back(VkInit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, vertexShader));
    m_ShaderStage.push_back(VkInit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader));
}

void PipelineBuilder::SetInputTopology(VkPrimitiveTopology topology)
{
    m_InputAssembly.topology = topology;
    m_InputAssembly.primitiveRestartEnable = VK_FALSE;
}

void PipelineBuilder::SetPolygonMode(VkPolygonMode mode)
{
    m_Rasterizer.polygonMode = mode;
    m_Rasterizer.lineWidth = 1.0f;
}

void PipelineBuilder::SetCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace)
{
    m_Rasterizer.cullMode = cullMode;
    m_Rasterizer.frontFace = frontFace;
}

void PipelineBuilder::SetMultiSamplingNone()
{
    m_Multisampling.sampleShadingEnable = VK_FALSE;
    m_Multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    m_Multisampling.minSampleShading = 1.0f;
    m_Multisampling.pSampleMask = nullptr;
    m_Multisampling.alphaToCoverageEnable = VK_FALSE;
    m_Multisampling.alphaToOneEnable = VK_FALSE;
}

void PipelineBuilder::DisableBlending()
{
    m_ColorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    m_ColorBlendAttachment.blendEnable = VK_FALSE;
}

void PipelineBuilder::SetColorAttachmentFormat(VkFormat format)
{
    m_ColorAttachmentFormat = format;
    m_RenderInfo.colorAttachmentCount = 1;
    m_RenderInfo.pColorAttachmentFormats = &m_ColorAttachmentFormat;
}

void PipelineBuilder::SetDepthFormat(VkFormat format)
{
    m_RenderInfo.depthAttachmentFormat = format;
}

void PipelineBuilder::EnableDepthtest(bool depthWriteEnable, VkCompareOp op)
{
    m_DepthStencil.depthTestEnable = VK_TRUE;
    m_DepthStencil.depthWriteEnable = depthWriteEnable;
    m_DepthStencil.depthCompareOp = op;
    m_DepthStencil.depthBoundsTestEnable = VK_FALSE;
    m_DepthStencil.stencilTestEnable = VK_FALSE;
    m_DepthStencil.front = {};
    m_DepthStencil.back = {};
    m_DepthStencil.minDepthBounds = 0.0f;
    m_DepthStencil.maxDepthBounds = 1.0f;
}

void PipelineBuilder::DisableDepthTest()
{
    m_DepthStencil.depthTestEnable = VK_FALSE;
    m_DepthStencil.depthWriteEnable = VK_FALSE;
    m_DepthStencil.depthCompareOp = VK_COMPARE_OP_NEVER;
    m_DepthStencil.depthBoundsTestEnable = VK_FALSE;
    m_DepthStencil.stencilTestEnable = VK_FALSE;
    m_DepthStencil.front = {};
    m_DepthStencil.back = {};
    m_DepthStencil.minDepthBounds = 0.0f;
    m_DepthStencil.maxDepthBounds = 1.0f;
}

bool VkUtils::loadShaderModule(const char* filePath, VkDevice device, VkShaderModule* outShaderModule)
{
    // open the file. With cursor at the end
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        return false;
    }

    // find what the size of the file is by looking up the location of the cursor
    // because the cursor is at the end, it gives the size directly in bytes
    size_t fileSize = (size_t)file.tellg();

    // spirv expects the buffer to be on uint32, so make sure to reserve a int
    // vector big enough for the entire file
    std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

    // put file cursor at beginning
    file.seekg(0);

    // load the entire file into the buffer
    file.read((char*)buffer.data(), fileSize);

    // now that the file is loaded into the buffer, we can close it
    file.close();

    // create a new shader module, using the buffer we loaded
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.pNext = nullptr;

    // codeSize has to be in bytes, so multply the ints in the buffer by size of
    // int to know the real size of the buffer
    createInfo.codeSize = buffer.size() * sizeof(uint32_t);
    createInfo.pCode = buffer.data();

    // check that the creation goes well.
    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
    {
        return false;
    }
    *outShaderModule = shaderModule;
    return true;
}