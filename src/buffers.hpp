#pragma once

#include <vector>

#include <vulkan/vulkan_core.h>

namespace VE
{
	class Buffers
	{
	public:
		Buffers() = default;
		~Buffers();

		void CreateCommandPool();
		void CreateCommandBuffer();
		void CreateVertexBuffer();
		void CreateIndexBuffer();
		void CreateDepthResources();
		
		static void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
		void RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
		VkCommandBuffer BeginSingleTimeCommands();
		void EndSingleTimeCommands(VkCommandBuffer commandBuffer);
		static VkFormat FindDepthFormat();
		static uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
		void DestroyDepthImage();
		
		std::vector<VkCommandBuffer>& GetCommandBuffers() { return m_CommandBuffers; }
		VkImageView& GetDepthImageView() { return m_DepthImageView; }

	private:
		void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
		static VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

	private:
		std::vector<VkCommandBuffer> m_CommandBuffers;
		VkCommandPool m_CommandPool;
		VkBuffer m_VertexBuffer;
		VkDeviceMemory m_VertexBufferMemory;
		VkBuffer m_IndexBuffer;
		VkDeviceMemory m_IndexBufferMemory;
		VkImage m_DepthImage;
		VkDeviceMemory m_DepthImageMemory;
		VkImageView m_DepthImageView;
	};
}