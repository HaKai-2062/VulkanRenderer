#pragma once

#include <vulkan/vulkan.h>
#include <string_view>

namespace VkUtils
{
	void transitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);
	void copyImageToImage(VkCommandBuffer cmd, VkImage source, VkImage destinatidon, VkExtent2D srcSize, VkExtent2D dstSize);
	void generateMipmaps(VkCommandBuffer cmd, VkImage image, VkExtent2D imageSize);
	bool loadCubeMap(VkCommandBuffer cmd, std::string_view filename, VkFormat format);
}