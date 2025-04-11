#include <iostream>
#include <fmt/core.h>
#include <fmt/os.h>
#include <fmt/color.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>

#include "vk_loader.h"
#include "vk_engine.h"

std::optional<std::vector<std::shared_ptr<MeshAsset>>> LoadGltfMeshes(VulkanEngine* engine, std::filesystem::path filePath)
{
	fmt::print(fmt::fg(fmt::color::white), "Loading GLTF: ");
	fmt::print(fmt::fg(fmt::color::yellow), "{}", filePath.string());
	fmt::print(fmt::fg(fmt::color::white), " | ");

	auto data = fastgltf::GltfDataBuffer::FromPath(filePath);

	constexpr auto gltfOptions = fastgltf::Options::LoadGLBBuffers
		| fastgltf::Options::LoadExternalBuffers;

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
		newMesh.Name = mesh.name;

		for (auto&& p : mesh.primitives)
		{
			GeoSurface newSurface;
			newSurface.StartIndex = (uint32_t)indices.size();
			newSurface.Count = (uint32_t)gltf.accessors[p.indicesAccessor.value()].count;
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
						newVtx.Position = v;
						newVtx.Normal = { 1.0f, 0.0f, 0.0f };
						newVtx.Color = glm::vec4(1.0f);
						newVtx.UVX = 0.0f;
						newVtx.UVY = 0.0f;
						vertices[initialVtx + index] = newVtx;
					});
			}

			// Load vertex normals
			auto normals = p.findAttribute("NORMAL");
			if (normals != p.attributes.end())
			{
				fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[(*normals).accessorIndex],
					[&](glm::vec3 v, size_t index) {
						vertices[initialVtx + index].Normal = v;
					});
			}

			// Load UVs
			auto uvs = p.findAttribute("TEXCOORD_0");
			if (uvs != p.attributes.end())
			{
				fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[(*uvs).accessorIndex],
					[&](glm::vec2 v, size_t index) {
						vertices[initialVtx + index].UVX = v.x;
						vertices[initialVtx + index].UVY = v.y;
					});
			}

			// Load vertex colors
			auto colors = p.findAttribute("COLOR_0");
			if (colors != p.attributes.end())
			{
				fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[(*colors).accessorIndex],
					[&](glm::vec4 v, size_t index) {
						vertices[initialVtx + index].Color = v;
					});
			}

			newMesh.Surfaces.push_back(newSurface);
		}

		// Display vertex normals
		constexpr bool overrideColors = true;
		if (overrideColors)
		{
			for (Vertex& vertex : vertices)
			{
				vertex.Color = glm::vec4(vertex.Normal, 1.0f);
			}
		}

		newMesh.MeshBuffers = engine->UploadMesh(indices, vertices);
		meshes.emplace_back(std::make_shared<MeshAsset>(std::move(newMesh)));
	}

	return meshes;
}
