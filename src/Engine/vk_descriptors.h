#pragma once

#include <vector>
#include <deque>
#include <span>

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