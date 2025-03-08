#pragma once

#include <vulkan/vulkan.h>

struct FrameData
{
	VkCommandPool CommandPool;
	VkCommandBuffer CommandBuffer;
	// Wait till we get ImageFromSwapchain, Wait till gpu has rendered to present on the screen
	VkSemaphore SwapchainSemaphore, RenderSemaphore;
	// Wait till gpu has rendered to prevent overwriting gpu commands
	VkFence RenderFence;
};

constexpr unsigned int MAX_FRAMES_IN_FLIGHT = 2;