#pragma once

#include <unordered_map>
#include <filesystem>

#include "vk_types.h"

struct GeoSurface
{
	uint32_t StartIndex;
	uint32_t Count;
};

struct MeshAsset
{
	std::string Name;
	std::vector<GeoSurface> Surfaces;
	GPUMeshBuffers MeshBuffers;
};

class VulkanEngine;

std::optional<std::vector<std::shared_ptr<MeshAsset>>> LoadGltfMeshes(VulkanEngine* engine, std::filesystem::path filePath);