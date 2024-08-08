#pragma once

namespace VE
{
	class Texture
	{
	public:
		Texture() = default;
		~Texture();

		void CreateTextureImage();
		void CreateTextureImageView();
		void CreateTextureSampler();

		VkImageView& GetTextureImageView() { return m_TextureImageView; }
		VkSampler& GetTextureSampler() { return m_TextureSampler; }

	private:
		void CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
		void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
		void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

	private:
		VkBuffer m_StagingBuffer;
		VkDeviceMemory m_StagingBufferMemory;
		VkImage m_TextureImage;
		VkDeviceMemory m_TextureImageMemory;
		VkImageView m_TextureImageView;
		VkSampler m_TextureSampler;
	};
}