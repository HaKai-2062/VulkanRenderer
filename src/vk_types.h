#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vk_mem_alloc.h>
#include <fmt/core.h>
#include <fmt/os.h>
#include <fmt/color.h>

#include <deque>
#include <functional>
#include <span>

#include <glm/glm.hpp>

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

struct DescriptorLayoutBuilder
{
	std::vector<VkDescriptorSetLayoutBinding> bindings;

	void addBinding(uint32_t binding, VkDescriptorType type);
	void clear();
	VkDescriptorSetLayout build(VkDevice device, VkShaderStageFlags shaderStages, void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);
};

class DescriptorAllocatorDynamic
{
public:
	struct PoolSizeRatio
	{
		VkDescriptorType type;
		float ratio;
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
	std::deque<VkDescriptorImageInfo> imageInfos;
	std::deque<VkDescriptorBufferInfo> bufferInfos;
	std::vector<VkWriteDescriptorSet> writes;

	void writeImage(int binding, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type);
	void writeBuffer(int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type);
	void clear();
	void updateSet(VkDevice device, VkDescriptorSet set);
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
	DescriptorAllocatorDynamic frameDescriptors;
};


struct AllocatedImage
{
	VkImage image;
	VkImageView imageView;
	VmaAllocation allocation;
	VkExtent3D imageExtent;
	VkFormat imageFormat;
};

struct AllocatedBuffer
{
	VkBuffer buffer;
	VmaAllocation allocation;
	VmaAllocationInfo info;
};

struct Vertex
{
	glm::vec3 position;
	float uvX;
	glm::vec3 normal;
	float uvY;
	glm::vec4 color;
};

struct GPUMeshBuffers
{
	AllocatedBuffer indexBuffer;
	AllocatedBuffer vertexBuffer;
	VkDeviceAddress vertexDeviceAddress;
};

struct GPUDrawPushConstants
{
	glm::mat4 worldMatrix;
	VkDeviceAddress vertexBufferAddress;
};

struct GPUSceneData
{
	glm::mat4 view;
	glm::mat4 proj;
	glm::mat4 viewproj;
	glm::vec4 ambientColor;
	glm::vec4 sunlightDirection; // w for sun power
	glm::vec4 sunlightColor;
};

enum class MaterialPass : uint8_t
{
	MainColor,
	Transparent,
	Other
};

struct MaterialPipeline
{
	VkPipeline pipeline;
	VkPipelineLayout layout;
};

struct MaterialInstance
{
	MaterialPipeline* pipeline;
	VkDescriptorSet materialSet;
	MaterialPass passType;
};

struct GLTFMetallic_Roughness
{
	MaterialPipeline opaquePipeline;
	MaterialPipeline transparentPipeline;

	VkDescriptorSetLayout materialLayout;

	// 256 bytes in total because good default allignment
	struct MaterialConstants
	{
		glm::vec4 colorFactors;
		glm::vec4 metalRoughFactors;
		// padding, may need for uniform buffer
		glm::vec4 extra[14];
	};


	struct MaterialResources
	{
		AllocatedImage colorImage;
		VkSampler colorSampler;
		AllocatedImage metalRoughImage;
		VkSampler	metalRoughSampler;
		VkBuffer dataBuffer;
		uint32_t dataBufferOffset;
	};

	DescriptorWriter writer;

	void buildPipelines(class VulkanEngine* engine);
	void clearResources(VkDevice device);
	MaterialInstance writeMaterial(VkDevice device, MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorDynamic& descriptorAllocator);
};