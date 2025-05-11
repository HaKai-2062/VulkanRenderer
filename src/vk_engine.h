#pragma once

#include <vector>

#include "vk_types.h"
#include "vk_loader.h"
#include "camera.h"

struct ComputePushConstants
{
	glm::vec4 Data1;
	glm::vec4 Data2;
	glm::vec4 Data3;
	glm::vec4 Data4;
};

struct ComputeEffect
{
	const char* Name;

	VkPipeline Pipeline;
	VkPipelineLayout Layout;

	ComputePushConstants Data;
};

struct MeshNode : public Node
{
	std::shared_ptr<MeshAsset> Mesh;

	virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx) override;
};

struct RenderObject
{
	uint32_t IndexCount;
	uint32_t FirstIndex;
	VkBuffer IndexBuffer;

	MaterialInstance* Material;
	Bounds BoundingBox;
	glm::mat4 Transform;
	VkDeviceAddress VertexBufferAddress;
};

struct DrawContext
{
	std::vector<RenderObject> OpaqueSurfaces;
	std::vector<RenderObject> TransparentSurfaces;
};

struct EngineStats
{
	float FrameTime;
	int TriangleCount;
	int DrawcallCount;
	float SceneUpdateTime;
	float MeshDrawTime;
};

class VulkanEngine
{
public:
	VkDevice Device;
	AllocatedImage DrawImage;
	AllocatedImage DepthImage;
	AllocatedImage ErrorCheckerboardImage;
	AllocatedImage WhiteImage;
	AllocatedImage PurpleImage;
	AllocatedImage CubeMap;
	VkSampler CubeMapSampler;
	VkDescriptorSetLayout GPUSceneDataDescriptorLayout;
	VkSampler DefaultSamplerLinear;
	GLTFMetallic_Roughness MetalRoughMaterial;

private:
	bool m_IsInitialized{ false };
	bool m_ResizeRequested{ false };
	int m_FrameNumber{ 0 };
	float m_LastFrameTime{ 0 };
	float m_DeltaTime{ 0 };
	float m_TitleUpdateTime{ 0 };
	VkExtent2D m_WindowExtent{ 100 , 100 };
	struct GLFWwindow* m_Window{ nullptr };
	std::vector<ComputeEffect> m_BGEffects;
	int m_CurrentBGEffect{ 1 };

	VkExtent2D m_DrawExtent;
	float m_RenderScale = 1.0f;
	FrameData m_Frames[MAX_FRAMES_IN_FLIGHT];
	DeletionQueue m_MainDeletionQueue;
	VmaAllocator m_Allocator;
	EngineStats Stats;

	DescriptorAllocatorDynamic m_GlobalDescriptorAllocator;
	GPUMeshBuffers m_Cube;
	std::vector<std::shared_ptr<MeshAsset>> m_TestMeshes;
	GPUSceneData m_SceneData;
	LightData m_Lights;
	//AllocatedImage m_BlackImage;
	//AllocatedImage m_GreyImage;
	VkSampler m_DefaultSamplerNearest;
	VkDescriptorSetLayout m_SingleImageDescriptorLayout;
	MaterialInstance m_DefaultData;
	DrawContext m_MainDrawContext;
	std::unordered_map<std::string, std::shared_ptr<Node>> m_LoadedNodes;
	Camera m_Camera;
	std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> m_LoadedScenes;

	VkQueue m_GraphicsQueue;
	uint32_t m_GraphicsQueueFamily;
	VkInstance m_Instance;
	VkDebugUtilsMessengerEXT m_DebugMessenger;
	VkPhysicalDevice m_PhysicalDevice;
	VkSurfaceKHR m_Surface;
	VkSwapchainKHR m_Swapchain;
	VkFormat m_SwapchainFormat;
	std::vector<VkImage> m_SwapchainImages;
	std::vector<VkImageView> m_SwapchainImageViews;
	VkExtent2D m_SwapchainExtent;
	VkDescriptorSet m_CubeMapDescriptors;
	VkDescriptorSetLayout m_CubeMapDescriptorLayout;
	VkFence m_ImmediateFence;
	VkCommandBuffer m_ImmediateCommandBuffer;
	VkCommandPool m_ImmediateCommandPool;

	VkPipeline m_CubeMapPipeline;
	VkPipelineLayout m_CubeMapPipelineLayout;
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
	AllocatedBuffer CreateBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
	void DestroyBuffer(const AllocatedBuffer& buffer);
	AllocatedImage UploadImage(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
	AllocatedImage CreateImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
	AllocatedImage CreateCubeMapImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, uint32_t mipMapLevels);
	AllocatedImage UploadCubeMapImage(void* data, const std::span<VkBufferImageCopy> bufferCopyRegions, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, uint32_t mapMapLevels, size_t dataSize);
	void DestroyImage(const AllocatedImage& image);

private:
	void InitVulkan();
	void InitSwapchain();
	void InitCommands();
	void InitSyncStructures();
	void InitDescriptors();
	void InitImGui();
	void InitPipelines();
	void InitCubeMapPipeline();
	void InitMeshPipeline();
	void InitDefaultData();

	void UpdateDeltaTimeAndTitle();
	void CreateSwapchain(uint32_t width, uint32_t height);
	void DestroySwapchain();
	void ResizeSwapchain();
	void DrawMain(VkCommandBuffer cmd);
	void DrawGeometry(VkCommandBuffer cmd);
	void DrawMesh(VkCommandBuffer cmd);
	void DrawCubeMap(VkCommandBuffer cmd);
	void DrawImgui(VkCommandBuffer cmd, VkImageView targetImageView);
	void UpdateScene();
};