#include "vk_types.h"

void DescriptorLayoutBuilder::addBinding(uint32_t binding, VkDescriptorType type)
{
    VkDescriptorSetLayoutBinding newbind{};
    newbind.binding = binding;
    newbind.descriptorCount = 1;
    newbind.descriptorType = type;

    bindings.push_back(newbind);
}

void DescriptorLayoutBuilder::clear()
{
    bindings.clear();
}

VkDescriptorSetLayout DescriptorLayoutBuilder::build(VkDevice device, VkShaderStageFlags shaderStages, void* pNext, VkDescriptorSetLayoutCreateFlags flags)
{
    for (auto& b : bindings)
    {
        b.stageFlags |= shaderStages;
    }

    VkDescriptorSetLayoutCreateInfo info = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    info.pNext = pNext;

    info.pBindings = bindings.data();
    info.bindingCount = (uint32_t)bindings.size();
    info.flags = flags;

    VkDescriptorSetLayout set;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &set));

    return set;
}

void DescriptorAllocatorDynamic::Init(VkDevice device, uint32_t initialSets, std::span<PoolSizeRatio> poolRatios)
{
    m_Ratios.clear();
    
    // ratio is a multiplier that determines how many descriptors of that type should be allocated based on initialSets
    for (PoolSizeRatio ratio : poolRatios)
    {
        m_Ratios.push_back(ratio);
    }

    VkDescriptorPool newPool = CreatePool(device, initialSets, poolRatios);
    m_SetsPerPool = initialSets * 1.5f;
    m_ReadyPools.push_back(newPool);
}

void DescriptorAllocatorDynamic::ClearPools(VkDevice device)
{
    for (auto pool : m_ReadyPools)
    {
        vkResetDescriptorPool(device, pool, 0);
    }

    for (auto pool : m_FullPools)
    {
        vkResetDescriptorPool(device, pool, 0);
        m_ReadyPools.push_back(pool);
    }

    m_FullPools.clear();
}

void DescriptorAllocatorDynamic::DestroyPools(VkDevice device)
{
    for (auto pool : m_ReadyPools)
    {
        vkDestroyDescriptorPool(device, pool, nullptr);
    }
    m_ReadyPools.clear();

    for (auto pool : m_FullPools)
    {
        vkDestroyDescriptorPool(device, pool, nullptr);
    }
    m_FullPools.clear();
}

VkDescriptorSet DescriptorAllocatorDynamic::Allocate(VkDevice device, VkDescriptorSetLayout layout, void* pNext)
{
    VkDescriptorPool poolToUse = GetPool(device);

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.pNext = nullptr;
    allocInfo.descriptorPool = poolToUse;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet ds;
    VkResult result = vkAllocateDescriptorSets(device, &allocInfo, &ds);
    
    if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL)
    {
        m_FullPools.push_back(poolToUse);
        poolToUse = GetPool(device);
        allocInfo.descriptorPool = poolToUse;

        VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &ds));
    }
    
    m_ReadyPools.push_back(poolToUse);
    return ds;
}

// Get pool from m_ReadyPools, remove it from the array and push it to m_FullPools if filled up otherwise into m_ReadyPools
// If no pool in m_ReadyPools exist, create with linear increasing capactiy of 1.5

VkDescriptorPool DescriptorAllocatorDynamic::GetPool(VkDevice device)
{
    VkDescriptorPool newPool;
    if (m_ReadyPools.size() != 0)
    {
        newPool = m_ReadyPools.back();
        m_ReadyPools.pop_back();
    }
    else
    {
        newPool = CreatePool(device, m_SetsPerPool, m_Ratios);
        m_SetsPerPool *= 1.5f;
        if (m_SetsPerPool > 4092)
        {
            m_SetsPerPool = 4092;
        }
    }

    return newPool;
}

VkDescriptorPool DescriptorAllocatorDynamic::CreatePool(VkDevice device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios)
{
    std::vector<VkDescriptorPoolSize> poolSizes;
    for (PoolSizeRatio ratio : poolRatios)
    {
        poolSizes.push_back(VkDescriptorPoolSize{
            .type = ratio.type,
            .descriptorCount = uint32_t(ratio.ratio * setCount)
        });
    }

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = 0;
    poolInfo.maxSets = setCount;
    poolInfo.poolSizeCount = (uint32_t)poolSizes.size();
    poolInfo.pPoolSizes = poolSizes.data();

    VkDescriptorPool newPool;
    vkCreateDescriptorPool(device, &poolInfo, nullptr, &newPool);
    return newPool;
}

void DescriptorWriter::writeImage(int binding, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type)
{
    VkDescriptorImageInfo& info = imageInfos.emplace_back(VkDescriptorImageInfo{
    .sampler = sampler,
    .imageView = image,
    .imageLayout = layout
        });

    VkWriteDescriptorSet write = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };

    write.dstBinding = binding;
    write.dstSet = VK_NULL_HANDLE; //left empty for now until we need to write it
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pImageInfo = &info;

    writes.push_back(write);
}

void DescriptorWriter::writeBuffer(int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type)
{
    VkDescriptorBufferInfo& info = bufferInfos.emplace_back(VkDescriptorBufferInfo{
        .buffer = buffer,
        .offset = offset,
        .range = size
        });

    VkWriteDescriptorSet write = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };

    write.dstBinding = binding;
    write.dstSet = VK_NULL_HANDLE; //left empty for now until we need to write it
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pBufferInfo = &info;

    writes.push_back(write);
}

void DescriptorWriter::clear()
{
    imageInfos.clear();
    writes.clear();
    bufferInfos.clear();
}

void DescriptorWriter::updateSet(VkDevice device, VkDescriptorSet set)
{
    for (VkWriteDescriptorSet& write : writes)
    {
        write.dstSet = set;
    }

    vkUpdateDescriptorSets(device, (uint32_t)writes.size(), writes.data(), 0, nullptr);
}
