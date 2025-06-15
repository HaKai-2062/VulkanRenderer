#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vk_mem_alloc.h>
#include <fmt/core.h>
#include <fmt/os.h>
#include <fmt/color.h>

#include <vector>
#include <memory>
#include <deque>
#include <functional>
#include <span>

#include <glm/glm.hpp>

constexpr unsigned int MAX_FRAMES_IN_FLIGHT = 2;
constexpr unsigned int MAX_POINT_LIGHTS = 8;
constexpr unsigned int MAX_SPOT_LIGHTS = 8;

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
	std::deque<std::function<void()>> Deletors;

	void PushFunction(std::function<void()>&& function)
	{
		Deletors.push_back(function);
	}

	void Flush()
	{
		// reverse iterate the deletion queue to execute all the functions
		for (auto it = Deletors.rbegin(); it != Deletors.rend(); it++)
		{
			(*it)(); //call functors
		}

		Deletors.clear();
	}
};

struct DescriptorLayoutBuilder
{
	std::vector<VkDescriptorSetLayoutBinding> Bindings;

	void AddBinding(uint32_t binding, VkDescriptorType type, uint32_t count = 1);
	void Clear();
	VkDescriptorSetLayout Build(VkDevice device, VkShaderStageFlags shaderStages, void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);
};

class DescriptorAllocatorDynamic
{
public:
	struct PoolSizeRatio
	{
		VkDescriptorType Type;
		float Ratio;
	};

	void Init(VkDevice device, uint32_t initialSets, std::span<PoolSizeRatio> poolRatios);
	void ClearPools(VkDevice device);
	void DestroyPools(VkDevice device);
	VkDescriptorSet Allocate(VkDevice device, VkDescriptorSetLayout layout, void* pNext = nullptr);

private:
	VkDescriptorPool GetPool(VkDevice device);
	VkDescriptorPool CreatePool(VkDevice device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios);

	std::vector<PoolSizeRatio> m_Ratios;
	std::vector<VkDescriptorPool> m_FullPools;
	std::vector<VkDescriptorPool> m_ReadyPools;
	uint32_t m_SetsPerPool;
};

struct DescriptorWriter
{
	std::deque<VkDescriptorImageInfo> ImageInfos;
	std::deque<VkDescriptorBufferInfo> BufferInfos;
	std::vector<VkWriteDescriptorSet> Writes;

	void WriteImage(int binding, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type);
	void WriteImage(int binding, const std::vector<VkImageView>& images, VkSampler sampler, VkImageLayout layout, VkDescriptorType type);
	void WriteBuffer(int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type);
	void Clear();
	void UpdateSet(VkDevice device, VkDescriptorSet set);
};

struct FrameData
{
	VkCommandPool CommandPool;
	VkCommandBuffer CommandBuffer;
	// Wait till we get ImageFromSwapchain, Wait till gpu has rendered to present on the screen
	VkSemaphore SwapchainSemaphore, RenderSemaphore;
	// Wait till gpu has rendered to prevent overwriting gpu commands
	VkFence RenderFence;
	DeletionQueue FrameDeletionQueue;
	DescriptorAllocatorDynamic FrameDescriptors;
};

struct AllocatedImage
{
	VkImage Image;
	VkImageView ImageView;
	VmaAllocation Allocation;
	VkExtent3D ImageExtent;
	VkFormat ImageFormat;
};

struct AllocatedBuffer
{
	VkBuffer Buffer;
	VmaAllocation Allocation;
	VmaAllocationInfo Info;
};

struct Vertex
{
	glm::vec3 Position;
	float UVX;
	glm::vec3 Normal;
	float UVY;
	glm::vec4 Color;
};

struct GPUMeshBuffers
{
	AllocatedBuffer IndexBuffer;
	AllocatedBuffer VertexBuffer;
	VkDeviceAddress VertexDeviceAddress;
};

struct GPUDrawPushConstants
{
	glm::mat4 WorldMatrix;
	VkDeviceAddress VertexBufferAddress;
	glm::vec2 Padding;
	glm::vec4 OverrideColor;
};

struct GPUSceneData
{
	glm::mat4 View;
	glm::mat4 Proj;
	glm::mat4 ViewProj;
	glm::vec4 AmbientColor = glm::vec4(glm::vec3(0.03f), 1.0f);
	glm::vec4 CameraPosition;
	float Time;
	glm::vec3 Padding;
};

struct PointLight
{
	glm::vec3 Position;
	float Intensity = 10.0f;
	glm::vec3 Color{ 1.0f };
	uint32_t Padding;
	glm::mat4 LightProj;
};

struct Directional
{
	glm::vec3 Direction;
	float Intensity = 1.0f;
	glm::vec3 Color{ 1.0f };
	uint32_t Padding;
	glm::mat4 LightProj;
};

struct SpotLight
{
	glm::vec3 Position;
	float Cutoff = std::cos(glm::radians(25.0f));
	glm::vec3 Direction = { 0.0f, -1.0, 0.0f };
	float OuterCutoff = std::cos(glm::radians(35.0f));
	glm::vec3 Color = glm::vec3(1.0f);
	float Constant = 1.0f;
	float Linear = 0.09f;
	float Quadratic = 0.032f;
	glm::vec2 Padding;
	glm::mat4 LightProj;
};

struct LightData
{
	Directional DirectionalLight;
	PointLight PointLights[MAX_POINT_LIGHTS];
	SpotLight SpotLights[MAX_SPOT_LIGHTS];

	uint32_t TotalPointLights;
	uint32_t TotalSpotLights;
	glm::vec2 Padding;
};

enum class MaterialPass : uint8_t
{
	MainColor,
	Transparent,
	Other
};

struct MaterialPipeline
{
	VkPipeline Pipeline;
	VkPipelineLayout Layout;
};

struct MaterialInstance
{
	MaterialPipeline* Pipeline;
	VkDescriptorSet MaterialSet;
	MaterialPass PassType;
};

struct GLTFMetallic_Roughness
{
	MaterialPipeline OpaquePipeline;
	MaterialPipeline TransparentPipeline;

	VkDescriptorSetLayout MaterialLayout;

	// 256 bytes in total because good default alignment
	struct MaterialConstants
	{
		glm::vec4 ColorFactors;
		glm::vec4 MetalRoughFactors;
		// padding, may need for uniform buffer
		glm::vec4 Extra[14];
	};

	struct MaterialResources
	{
		AllocatedImage ColorImage;
		VkSampler ColorSampler;
		AllocatedImage MetalRoughImage;
		VkSampler MetalRoughSampler;
		AllocatedImage AOImage;
		VkSampler AOSampler;
		AllocatedImage NormalMapImage;
		VkSampler NormalMapSampler;
		VkBuffer DataBuffer;
		uint32_t DataBufferOffset;
	};

	DescriptorWriter Writer;

	void BuildPipelines(class VulkanEngine* engine);
	void ClearResources(VkDevice device);
	MaterialInstance WriteMaterial(VkDevice device, MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorDynamic& descriptorAllocator);
};

struct DrawContext;

class IRenderable
{
	virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx) = 0;
};

struct Node : public IRenderable
{
	void RefreshTransform(const glm::mat4& parentMatrix)
	{
		WorldTransform = parentMatrix * LocalTransform;
		for (auto& c : Children)
		{
			c->RefreshTransform(WorldTransform);
		}
	}

	virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx)
	{
		for (auto& c : Children)
		{
			c->Draw(topMatrix, ctx);
		}
	}

	std::weak_ptr<Node> Parent;
	std::vector<std::shared_ptr<Node>> Children;

	glm::mat4 LocalTransform;
	glm::mat4 WorldTransform;
};

enum class CameraMotion : uint8_t
{
	LEFT = 0,
	RIGHT,
	FORWARD,
	BACKWARD,
	UP,
	DOWN
};