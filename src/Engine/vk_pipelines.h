#pragma once

#include "vk_types.h"

class PipelineBuilder
{
private:
	std::vector<VkPipelineShaderStageCreateInfo> m_ShaderStage;

	VkPipelineInputAssemblyStateCreateInfo m_InputAssembly;
	VkPipelineRasterizationStateCreateInfo m_Rasterizer;
	VkPipelineColorBlendAttachmentState m_ColorBlendAttachment;
	VkPipelineMultisampleStateCreateInfo m_Multisampling;
	VkPipelineDepthStencilStateCreateInfo m_DepthStencil;
	VkPipelineRenderingCreateInfo m_RenderInfo;
	
	VkFormat m_ColorAttachmentFormat;

public:
	VkPipelineLayout PipelineLayout;

public:
	PipelineBuilder() { Clear(); }

	void Clear();
	VkPipeline BuildPipeline(VkDevice device);
	void SetShaders(VkShaderModule vertexShader, VkShaderModule fragmentShader);
	void SetInputTopology(VkPrimitiveTopology topology);
	void SetPolygonMode(VkPolygonMode mode);
	void SetCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace);
	void SetMultiSamplingNone();
	void DisableBlending();
	void EnableBlendingAdditive();
	void EnableBlendingAlphaBlend();
	void SetColorAttachmentFormat(VkFormat format);
	void SetDepthFormat(VkFormat format);
	void EnableDepthtest(bool depthWriteEnable, VkCompareOp op);
	void DisableDepthTest();
};

namespace VkUtils
{
	bool loadShaderModule(const char* filePath, VkDevice device, VkShaderModule* outShaderModule);
}