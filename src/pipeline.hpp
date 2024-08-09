#pragma once

#include <vector>
#include <string>

#include <vulkan/vulkan_core.h>

namespace VE
{
	class Pipeline
	{
	public:
		Pipeline() = default;
		~Pipeline();

		void CreateGraphicsPipeline();
		void CreateRenderPass();
		const VkRenderPass& GetRenderPass() { return m_RenderPass; }
		const VkPipeline& GetGraphicsPipeline() { return m_GraphicsPipeline; }
		const VkPipelineLayout& GetPipelineLayout() { return m_PipelineLayout; }

	private:
		VkShaderModule CreateShaderModule(const std::vector<char>& code);
		static std::vector<char> ReadFile(const std::string& filename);

	private:
		VkRenderPass m_RenderPass;
		VkPipelineLayout m_PipelineLayout;
		VkPipeline m_GraphicsPipeline;
	};
}