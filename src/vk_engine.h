#pragma once

#include <vector>

#include "vk_types.h"
#include "vk_descriptors.h"

class VulkanEngine
{
public:

	bool m_IsInitialized{ false };
	int m_FrameNumber{ 0 };
	float m_DeltaTime{ 0 };
	VkExtent2D m_WindowExtent{ 1700 , 900 };
	struct GLFWwindow* m_Window{ nullptr };

	VkExtent2D m_DrawExtent;
	FrameData m_Frames[MAX_FRAMES_IN_FLIGHT];
	DeletionQueue m_MainDeletionQueue;
	VmaAllocator m_Allocator;
	AllocatedImage m_DrawImage;
	DescriptorAllocator m_GlobalDescriptorAllocator;

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
	VkDescriptorSet m_DrawImageDescriptors;
	VkDescriptorSetLayout m_DrawImageDescriptorLayout;
	VkPipeline m_GradientPipeline;
	VkPipelineLayout m_GradientPipelineLayout;

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
	void InitDescriptors();
	void InitPipelines();
	void InitBackgroundPipelines();

	void CreateSwapchain(uint32_t width, uint32_t height);
	void DestroySwapchain();
	void DrawBackground(VkCommandBuffer& currentCMD);

	void AddFPSToTitle();
};