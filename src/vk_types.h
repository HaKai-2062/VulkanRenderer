#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vk_mem_alloc.h>
#include <fmt/core.h>
#include <fmt/os.h>
#include <fmt/color.h>

#include <deque>
#include <functional>

constexpr unsigned int MAX_FRAMES_IN_FLIGHT = 2;

#define VK_CHECK(x)                                                 \
	do                                                              \
	{                                                               \
		VkResult err = x;                                           \
		if (err)                                                    \
		{                                                           \
			fmt::print("{} {}\n",\
				fmt::styled("Vulkan error:", fmt::fg(fmt::color::red) | fmt::emphasis::bold),\
				fmt::styled(string_VkResult(err), fmt::fg(fmt::color::yellow)));\
			abort();                                                \
		}                                                           \
	} while (0)

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