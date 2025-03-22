#include <iostream>
#include <fmt/core.h>
#include <fmt/os.h>
#include <fmt/color.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
//#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>

#include "vk_loader.h"
#include "vk_engine.h"

std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGltfMeshes(VulkanEngine* engine, std::filesystem::path filePath)
{
	fmt::print(fmt::fg(fmt::color::white), "Loading GLTF: ");
	fmt::print(fmt::fg(fmt::color::yellow), "{}", filePath.string());
	fmt::print(fmt::fg(fmt::color::white), " | ");

	auto data = fastgltf::GltfDataBuffer::FromPath(filePath);
	
	constexpr auto gltfOptions = fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers;
	fastgltf::Asset gltf;
	fastgltf::Parser parser{};

	auto load = parser.loadGltf(data.get(), filePath.parent_path(), gltfOptions);
	if (load)
	{
		gltf = std::move(load.get());
		fmt::print(fmt::fg(fmt::color::green), "SUCCESS\n");
	}
	else
	{
		fmt::print(fmt::fg(fmt::color::red), "FAILED\n");
		return {};
	}

	std::vector<std::shared_ptr<MeshAsset>> meshes;

	// Use same vector for all meshes so memory doesnot get reallocated
	std::vector<uint32_t> indices;
	std::vector<Vertex> vertices;

	for (fastgltf::Mesh& mesh : gltf.meshes)
	{
		indices.clear();
		vertices.clear();
		
		MeshAsset newMesh;
		newMesh.name = mesh.name;

		for (auto&& p : mesh.primitives)
		{
			GeoSurface newSurface;
			newSurface.startIndex = (uint32_t)indices.size();
			newSurface.count = (uint32_t)gltf.accessors[p.indicesAccessor.value()].count;
			size_t initialVtx = vertices.size();

			// Load indices
			{
				fastgltf::Accessor& indexAccessor = gltf.accessors[p.indicesAccessor.value()];
				indices.reserve(indices.size() + indexAccessor.count);
				
				fastgltf::iterateAccessor<uint32_t>(gltf, indexAccessor,
					[&](uint32_t index) {
						indices.push_back(initialVtx + index);
					});
			}

			// Load vertex position
			{
				fastgltf::Accessor& posAccessor = gltf.accessors[p.findAttribute("POSITION")->accessorIndex];
				vertices.resize(vertices.size() + posAccessor.count);

				fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
					[&](glm::vec3 v, size_t index) {
						Vertex newVtx;
						newVtx.position = v;
						newVtx.normal = { 1.0f, 0.0f, 0.0f };
						newVtx.color = glm::vec4(1.0f);
						newVtx.uvX = 0.0f;
						newVtx.uvY = 0.0f;
						vertices[initialVtx + index] = newVtx;
					});
			}

			// Load vertex normals
			auto normals = p.findAttribute("NORMAL");
			if (normals != p.attributes.end())
			{
				fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[(*normals).accessorIndex],
					[&](glm::vec3 v, size_t index) {
						vertices[initialVtx + index].normal = v;
					});
			}

			// Load UVs
			auto uvs = p.findAttribute("TEXCOORD_0");
			if (uvs != p.attributes.end())
			{
				fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[(*uvs).accessorIndex],
					[&](glm::vec2 v, size_t index) {
						vertices[initialVtx + index].uvX = v.x;
						vertices[initialVtx + index].uvY = v.y;
					});
			}

			// Load vertex colors
			auto colors = p.findAttribute("COLOR_0");
			if (colors != p.attributes.end())
			{
				fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[(*colors).accessorIndex],
					[&](glm::vec4 v, size_t index) {
						vertices[initialVtx + index].color = v;
					});
			}

			newMesh.surfaces.push_back(newSurface);
		}

		// Display vertex normals
		constexpr bool overrideColors = true;
		if (overrideColors)
		{
			for (Vertex& vertex : vertices)
			{
				vertex.color = glm::vec4(vertex.normal, 1.0f);
			}
		}

		newMesh.meshBuffers = engine->UploadMesh(indices, vertices);
		meshes.push_back(std::make_shared<MeshAsset>(std::move(newMesh)));
	}

	return meshes;
}
