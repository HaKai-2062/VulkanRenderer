#pragma once

#include <vector>

#include "vk_types.h"

class VulkanEngine
{
public:

	bool m_IsInitialized{ false };
	int m_FrameNumber{ 0 };
	float m_DeltaTime{ 0 };
	VkExtent2D m_WindowExtent{ 1700 , 900 };
	struct GLFWwindow* m_Window{ nullptr };

	FrameData m_Frames[MAX_FRAMES_IN_FLIGHT];
	DeletionQueue m_DeletionQueue;
	VmaAllocator m_Allocator;
	AllocatedImage m_DrawImage;
	VkExtent2D m_DrawExtent;

	VkQueue m_GraphicsQueue;
	uint32_t m_GraphicsQueueFamily;
	VkInstance m_Instance;
	VkDebugUtilsMessengerEXT m_DebugMessenger;
	VkPhysicalDevice m_PhysicalDevice;
	VkDevice m_Device;
	VkSurfaceKHR m_Surface;
	VkSwapchainKHR m_Swapchain;
	VkFormat m_SwapchainFormat;
	std::vector<VkImage> m_SwapchainImages;
	std::vector<VkImageView> m_SwapchainImageViews;
	VkExtent2D m_SwapchainExtent;

public:
	
	void Init();
	void Cleanup();
	void DrawFrame();
	void MainLoop();

	FrameData& GetCurrentFrame() { return m_Frames[m_FrameNumber % 2]; };

private:

	void InitVulkan();
	void InitSwapchain();
	void InitCommands();
	void InitSyncStructures();

	void CreateSwapchain(uint32_t width, uint32_t height);
	void DestroySwapchain();
	void DrawBackground(VkCommandBuffer& currentCMD);

	void AddFPSToTitle();
};