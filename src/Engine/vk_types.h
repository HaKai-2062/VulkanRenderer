#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vk_mem_alloc.h>

#include <vector>
#include <memory>
#include <format>
#include <functional>

#include <glm/glm.hpp>

#include "vk_descriptors.h"
#include "Core/Log.h"

constexpr unsigned int MAX_FRAMES_IN_FLIGHT = 2;
constexpr unsigned int MAX_POINT_LIGHTS = 8;
constexpr unsigned int MAX_SPOT_LIGHTS = 8;

#define VK_CHECK(x)																				   \
	do																							   \
	{																							   \
		VkResult err = x;																		   \
		if (err)																				   \
		{																						   \
			Log::Write(LogLevel::FATAL, std::format("VK_CHECK {}", string_VkResult(err)).c_str()); \
			abort();																			   \
		}																						   \
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

struct alignas(16) Light
{
	glm::vec3 Position = { 0.0f, -1.0, 0.0f };
	// 0 is PointLight, 1 is SpotLight, 2 is DirectionalLight
	int Type;
	glm::vec3 Direction = { 0.0f, -1.0, 0.0f };
	float Intensity = 1.0f;
	glm::vec4 Color = glm::vec4(1.0f);
	glm::mat4 LightProj;
};

struct alignas(16) LightData
{
	int Count = 0;
	int TotalPointLights = 0;
	int TotalSpotLights = 0;
	int TotalDirectionalLights = 0;
	std::vector<Light> Lights;
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