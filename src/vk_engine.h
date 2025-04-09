#pragma once

#include <vector>
#include <glm/glm.hpp>

#include "vk_types.h"
#include "vk_descriptors.h"
#include "vk_loader.h"

struct ComputePushConstants
{
	glm::vec4 data1;
	glm::vec4 data2;
	glm::vec4 data3;
	glm::vec4 data4;
};

struct ComputeEffect
{
	const char* name;

	VkPipeline pipeline;
	VkPipelineLayout layout;

	ComputePushConstants data;
};

class VulkanEngine
{
public:
	bool m_IsInitialized{ false };
	bool m_ResizeRequested{ false };
	int m_FrameNumber{ 0 };
	float m_DeltaTime{ 0 };
	VkExtent2D m_WindowExtent{ 1920 , 1080 };
	struct GLFWwindow* m_Window{ nullptr };
	std::vector<ComputeEffect> m_BGEffects;
	int m_CurrentBGEffect{ 0 };

	VkExtent2D m_DrawExtent;
	float m_RenderScale = 1.0f;
	FrameData m_Frames[MAX_FRAMES_IN_FLIGHT];
	DeletionQueue m_MainDeletionQueue;
	VmaAllocator m_Allocator;
	AllocatedImage m_DrawImage;
	AllocatedImage m_DepthImage;
	DescriptorAllocatorDynamic m_GlobalDescriptorAllocator;
	GPUMeshBuffers m_Rectangle;
	std::vector<std::shared_ptr<MeshAsset>> m_TestMeshes;
	GPUSceneData m_SceneData;
	AllocatedImage m_WhiteImage;
	AllocatedImage m_BlackImage;
	AllocatedImage m_GreyImage;
	AllocatedImage m_ErrorCheckerboardImage;
	VkSampler m_DefaultSamplerLinear;
	VkSampler m_DefaultSamplerNearest;
	VkDescriptorSetLayout m_SingleImageDescriptorLayout;

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
	VkDescriptorSetLayout m_GPUSceneDataDescriptorLayout;
	VkFence m_ImmediateFence;
	VkCommandBuffer m_ImmediateCommandBuffer;
	VkCommandPool m_ImmediateCommandPool;

	VkPipeline m_GradientPipeline;
	VkPipelineLayout m_GradientPipelineLayout;
	VkPipeline m_TrianglePipeline;
	VkPipelineLayout m_TrianglePipelineLayout;
	VkPipeline m_MeshPipeline;
	VkPipelineLayout m_MeshPipelineLayout;

public:
	void Init();
	void Cleanup();
	void DrawFrame();
	void MainLoop();

	FrameData& GetCurrentFrame() { return m_Frames[m_FrameNumber % 2]; };
	void ImmediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);
	GPUMeshBuffers UploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);

private:
	void InitVulkan();
	void InitSwapchain();
	void InitCommands();
	void InitSyncStructures();
	void InitDescriptors();
	void InitImGui();
	void InitPipelines();
	void InitBackgroundPipelines();
	void InitTrianglePipeline();
	void InitMeshPipeline();
	void InitDefaultData();

	void AddFPSToTitle();
	void CreateSwapchain(uint32_t width, uint32_t height);
	void DestroySwapchain();
	void ResizeSwapchain();
	void DrawBackground(VkCommandBuffer& cmd);
	void DrawGeometry(VkCommandBuffer& cmd);
	void DrawImgui(VkCommandBuffer cmd, VkImageView targetImageView);
	AllocatedBuffer CreateBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
	void DestroyBuffer(const AllocatedBuffer& buffer);
	AllocatedImage CreateImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
	AllocatedImage CreateImage(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
	void DestroyImage(const AllocatedImage& image);
};