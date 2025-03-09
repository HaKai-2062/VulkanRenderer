#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <deque>
#include <functional>

constexpr unsigned int MAX_FRAMES_IN_FLIGHT = 2;

struct DeletionQueue
{
	std::deque<std::function<void()>> deletors;

	void pushFunction(std::function<void()>&& function)
	{
		deletors.push_back(function);
	}

	void flush()
	{
		// reverse iterate the deletion queue to execute all the functions
		for (auto it = deletors.rbegin(); it != deletors.rend(); it++)
		{
			(*it)(); //call functors
		}

		deletors.clear();
	}
};

struct FrameData
{
	VkCommandPool commandPool;
	VkCommandBuffer commandBuffer;
	// Wait till we get ImageFromSwapchain, Wait till gpu has rendered to present on the screen
	VkSemaphore swapchainSemaphore, renderSemaphore;
	// Wait till gpu has rendered to prevent overwriting gpu commands
	VkFence renderFence;
	DeletionQueue deletionQueue;
};

struct AllocatedImage
{
	VkImage image;
	VkImageView imageView;
	VmaAllocation allocation;
	VkExtent3D imageExtent;
	VkFormat imageFormat;
};