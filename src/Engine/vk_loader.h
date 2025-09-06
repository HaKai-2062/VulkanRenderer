#pragma once

#include <unordered_map>
#include <filesystem>
#include <fastgltf/core.hpp>
#include "vk_types.h"

class VulkanEngine;

struct GLTFMaterial
{
	MaterialInstance Data;
};

struct Bounds
{
    glm::vec3 Origin;
    float SphereRadius;
    glm::vec3 Extents;
};

struct GeoSurface
{
	uint32_t StartIndex;
	uint32_t Count;
    Bounds BoundingBox;
	std::shared_ptr<GLTFMaterial> Material;
};

struct MeshAsset
{
	std::string Name;
	std::vector<GeoSurface> Surfaces;
	GPUMeshBuffers MeshBuffers;
};

struct LoadedGLTF : public IRenderable
{
    // Storage for all the data on a given glTF file
    std::unordered_map<std::string, std::shared_ptr<MeshAsset>> Meshes;
    std::unordered_map<std::string, std::shared_ptr<Node>> Nodes;
    std::unordered_map<std::string, std::shared_ptr<GLTFMaterial>> Materials;
    // Using map with name can cause name collisions if 2 images have same name
    std::vector <AllocatedImage> Images;

    // Nodes without a parent, for iterating through the file in tree order
    // Could be useful for displaying nodes in editor
    std::vector<std::shared_ptr<Node>> TopNodes;

    std::vector<VkSampler> Samplers;
    DescriptorAllocatorDynamic DescriptorPool;
    AllocatedBuffer MaterialDataBuffer;
    VulkanEngine* Engine;

    ~LoadedGLTF() { ClearAll(); };
    virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx);

private:

    void ClearAll();
};

std::optional<std::shared_ptr<LoadedGLTF>> loadGltfScene(VulkanEngine* engine, std::string_view filePath);
std::optional<AllocatedImage> loadImage(VulkanEngine* engine, fastgltf::Asset& asset, fastgltf::Image& image);