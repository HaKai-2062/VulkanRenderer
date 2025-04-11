#pragma once

#include <unordered_map>
#include <filesystem>

#include "vk_types.h"

class VulkanEngine;

struct GLTFMaterial
{
	MaterialInstance Data;
};

struct GeoSurface
{
	uint32_t StartIndex;
	uint32_t Count;
	std::shared_ptr<GLTFMaterial> Material;
};

struct MeshAsset
{
	std::string Name;
	std::vector<GeoSurface> Surfaces;
	GPUMeshBuffers MeshBuffers;
};

std::optional<std::vector<std::shared_ptr<MeshAsset>>> LoadGltfMeshes(VulkanEngine* engine, std::filesystem::path filePath);