#include <fmt/core.h>
#include <fmt/os.h>
#include <fmt/color.h>
#include <variant>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <glm/gtx/quaternion.hpp>
#include <gtc/type_ptr.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/util.hpp>

#include "vk_loader.h"
#include "vk_engine.h"

VkFilter extractFilter(fastgltf::Filter filter)
{
	switch (filter)
	{
	// nearest samplers
	case fastgltf::Filter::Nearest:
	case fastgltf::Filter::NearestMipMapNearest:
	case fastgltf::Filter::NearestMipMapLinear:
		return VK_FILTER_NEAREST;

	// linear samplers
	case fastgltf::Filter::Linear:
	case fastgltf::Filter::LinearMipMapNearest:
	case fastgltf::Filter::LinearMipMapLinear:
	default:
		return VK_FILTER_LINEAR;
	}
}

VkSamplerMipmapMode extractMipmapMode(fastgltf::Filter filter)
{
	switch (filter)
	{
	case fastgltf::Filter::NearestMipMapNearest:
	case fastgltf::Filter::LinearMipMapNearest:
		return VK_SAMPLER_MIPMAP_MODE_NEAREST;

	case fastgltf::Filter::NearestMipMapLinear:
	case fastgltf::Filter::LinearMipMapLinear:
	default:
		return VK_SAMPLER_MIPMAP_MODE_LINEAR;
	}
}

std::optional<AllocatedImage> loadImage(VulkanEngine* engine, fastgltf::Asset& asset, fastgltf::Image& image)
{
	AllocatedImage newImage{};

	int width, height, nrChannels;

	std::visit(fastgltf::visitor{[](auto& arg) {},
		[&](fastgltf::sources::URI& filePath)
		{
			assert(filePath.fileByteOffset == 0);	// We don't support offsets with stbi.
			assert(filePath.uri.isLocalPath());		// We're only capable of loading
			// local files.

			const std::string path(filePath.uri.path().begin(), filePath.uri.path().end());

			unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 4);
			if (data)
			{
				VkExtent3D imagesize;
				imagesize.width = width;
				imagesize.height = height;
				imagesize.depth = 1;

				newImage = engine->UploadImage(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, true);

				stbi_image_free(data);
			}
		},
		[&](fastgltf::sources::Vector& vector)
		{
			unsigned char* data = stbi_load_from_memory((stbi_uc*)vector.bytes.data(), static_cast<int>(vector.bytes.size()),
				&width, &height, &nrChannels, 4);
			if (data)
			{
				VkExtent3D imagesize;
				imagesize.width = width;
				imagesize.height = height;
				imagesize.depth = 1;

				newImage = engine->UploadImage(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, true);

				stbi_image_free(data);
			}
		},
		[&](fastgltf::sources::Array& array)
		{
			unsigned char* data = stbi_load_from_memory((stbi_uc*)array.bytes.data(), static_cast<int>(array.bytes.size()),
				&width, &height, &nrChannels, 4);
			if (data)
			{
				VkExtent3D imagesize;
				imagesize.width = width;
				imagesize.height = height;
				imagesize.depth = 1;

				newImage = engine->UploadImage(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, true);

				stbi_image_free(data);
			}
		},
		[&](fastgltf::sources::BufferView& view)
		{
			auto& bufferView = asset.bufferViews[view.bufferViewIndex];
			auto& buffer = asset.buffers[bufferView.bufferIndex];

			std::visit(fastgltf::visitor
				{
				// We only care about VectorWithMime here, because we
				// specify LoadExternalBuffers, meaning all buffers
				// are already loaded into a vector.
				[](auto& arg) {},
				[&](fastgltf::sources::Array& array)
				{
					unsigned char* data = stbi_load_from_memory(reinterpret_cast<unsigned char*>(array.bytes.data()) + bufferView.byteOffset,
						static_cast<int>(bufferView.byteLength),
						&width, &height, &nrChannels, 4);
					if (data)
					{
						VkExtent3D imagesize;
						imagesize.width = width;
						imagesize.height = height;
						imagesize.depth = 1;

						newImage = engine->UploadImage(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM,
							VK_IMAGE_USAGE_SAMPLED_BIT, true);

						stbi_image_free(data);
					}
				} },
				buffer.data);
		},
		},
		image.data);

	// if any of the attempts to load the data failed, we havent written the image
	// so handle is null
	if (newImage.Image == VK_NULL_HANDLE)
	{
		return {};
	}
	else
	{
		return newImage;
	}
}

std::optional<std::shared_ptr<LoadedGLTF>> loadGltfScene(VulkanEngine* engine, std::string_view filePath)
{
	fmt::print(fmt::fg(fmt::color::white), "Loading GLTF: ");
	fmt::print(fmt::fg(fmt::color::yellow), "{}", filePath);
	fmt::print(fmt::fg(fmt::color::white), " | ");

	std::shared_ptr<LoadedGLTF> scene = std::make_shared<LoadedGLTF>();
	scene->Engine = engine;
	LoadedGLTF& file = *scene.get();

	auto data = fastgltf::GltfDataBuffer::FromPath(filePath);
	constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble | 
		fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers | fastgltf::Options::LoadExternalImages;

	fastgltf::Asset gltf;
	fastgltf::Parser parser{};
	std::filesystem::path path = filePath;

	auto load = parser.loadGltf(data.get(), path.parent_path(), gltfOptions);
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

	// We can stimate the descriptors we will need accurately
	std::vector<DescriptorAllocatorDynamic::PoolSizeRatio> sizes = { { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 } };

	file.DescriptorPool.Init(engine->Device, gltf.materials.size(), sizes);

	// Load samplers
	for (fastgltf::Sampler& sampler : gltf.samplers)
	{
		VkSamplerCreateInfo samplerInfo = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr };
		samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
		samplerInfo.minLod = 0;

		samplerInfo.magFilter = extractFilter(sampler.magFilter.value_or(fastgltf::Filter::Nearest));
		samplerInfo.minFilter = extractFilter(sampler.magFilter.value_or(fastgltf::Filter::Nearest));

		samplerInfo.mipmapMode = extractMipmapMode(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

		VkSampler newSampler;
		vkCreateSampler(engine->Device, &samplerInfo, nullptr, &newSampler);

		file.Samplers.push_back(newSampler);
	}

	// Temporal arrays for all the objects to use while creating the GLTF data
	std::vector<std::shared_ptr<MeshAsset>> meshes;
	std::vector<std::shared_ptr<Node>> nodes;
	std::vector<AllocatedImage> images;
	std::vector<std::shared_ptr<GLTFMaterial>> materials;

	// Load Textures
	for (fastgltf::Image& image : gltf.images)
	{
		std::optional<AllocatedImage> img = loadImage(engine, gltf, image);

		if (img.has_value())
		{
			images.push_back(*img);
			file.Images.push_back(*img);
		}
		else
		{
			// we failed to load, so lets give the slot a default white texture to not
			// completely break loading
			images.push_back(engine->ErrorCheckerboardImage);
			fmt::print(fmt::fg(fmt::color::red), "GLTF failed to load texture {}\n", image.name);
		}
	}

	// Create buffer to hold the material data
	file.MaterialDataBuffer = engine->CreateBuffer(sizeof(GLTFMetallic_Roughness::MaterialConstants) * gltf.materials.size(), 
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
	int dataIndex = 0;
	GLTFMetallic_Roughness::MaterialConstants* sceneMaterialConstants = (GLTFMetallic_Roughness::MaterialConstants*)file.MaterialDataBuffer.Info.pMappedData;

	// Loop to load the materials
	for (fastgltf::Material& mat : gltf.materials)
	{
		std::shared_ptr<GLTFMaterial> newMat = std::make_shared<GLTFMaterial>();
		materials.push_back(newMat);
		file.Materials[mat.name.c_str()] = newMat;

		GLTFMetallic_Roughness::MaterialConstants constants;
		constants.ColorFactors.x = mat.pbrData.baseColorFactor[0];
		constants.ColorFactors.y = mat.pbrData.baseColorFactor[1];
		constants.ColorFactors.z = mat.pbrData.baseColorFactor[2];
		constants.ColorFactors.w = mat.pbrData.baseColorFactor[3];

		constants.MetalRoughFactors.x = mat.pbrData.metallicFactor;
		constants.MetalRoughFactors.y = mat.pbrData.roughnessFactor;

		// Write material parameters to buffer
		sceneMaterialConstants[dataIndex] = constants;

		MaterialPass passType = MaterialPass::MainColor;
		if (mat.alphaMode == fastgltf::AlphaMode::Blend)
		{
			passType = MaterialPass::Transparent;
		}

		// Default the material textures
		GLTFMetallic_Roughness::MaterialResources materialResources;
		materialResources.ColorImage = engine->WhiteImage;
		materialResources.ColorSampler = engine->DefaultSamplerLinear;
		materialResources.MetalRoughImage = engine->WhiteImage;
		materialResources.MetalRoughSampler = engine->DefaultSamplerLinear;
		materialResources.AOImage = engine->WhiteImage;
		materialResources.AOSampler = engine->DefaultSamplerLinear;
		materialResources.NormalMapImage = engine->PurpleImage;
		materialResources.NormalMapSampler = engine->DefaultSamplerLinear;

		// Set the uniform buffer for the material data
		materialResources.DataBuffer = file.MaterialDataBuffer.Buffer;
		materialResources.DataBufferOffset = dataIndex * sizeof(GLTFMetallic_Roughness::MaterialConstants);

		// Grab textures from gltf file
		if (mat.pbrData.baseColorTexture.has_value())
		{
			size_t img = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].imageIndex.value();
			size_t sampler = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].samplerIndex.value();

			materialResources.ColorImage = images[img];
			materialResources.ColorSampler = file.Samplers[sampler];
		}
		if (mat.occlusionTexture.has_value())
		{
			size_t img = gltf.textures[mat.occlusionTexture.value().textureIndex].imageIndex.value();
			size_t sampler = gltf.textures[mat.occlusionTexture.value().textureIndex].samplerIndex.value();

			materialResources.AOImage = images[img];
			materialResources.AOSampler = file.Samplers[sampler];
		}
		if (mat.normalTexture.has_value())
		{
			size_t img = gltf.textures[mat.normalTexture.value().textureIndex].imageIndex.value();
			size_t sampler = gltf.textures[mat.normalTexture.value().textureIndex].samplerIndex.value();

			materialResources.NormalMapImage = images[img];
			materialResources.NormalMapSampler = file.Samplers[sampler];
		}

		// Build material
		newMat->Data = engine->MetalRoughMaterial.WriteMaterial(engine->Device, passType, materialResources, file.DescriptorPool);

		dataIndex++;
	}

	// Use the same vectors for all meshes so that the memory doesnt reallocate as often
	std::vector<uint32_t> indices;
	std::vector<Vertex> vertices;

	for (fastgltf::Mesh& mesh : gltf.meshes)
	{
		std::shared_ptr<MeshAsset> newmesh = std::make_shared<MeshAsset>();
		meshes.push_back(newmesh);
		file.Meshes[mesh.name.c_str()] = newmesh;
		newmesh->Name = mesh.name;

		// Clear the mesh arrays each mesh, we dont want to merge them by error
		indices.clear();
		vertices.clear();

		for (auto&& p : mesh.primitives)
		{
			GeoSurface newSurface;
			newSurface.StartIndex = (uint32_t)indices.size();
			newSurface.Count = (uint32_t)gltf.accessors[p.indicesAccessor.value()].count;

			size_t initialVtx = vertices.size();

			// Load indices
			{
				fastgltf::Accessor& indexaccessor = gltf.accessors[p.indicesAccessor.value()];
				indices.reserve(indices.size() + indexaccessor.count);

				fastgltf::iterateAccessor<std::uint32_t>(gltf, indexaccessor,
					[&](std::uint32_t idx)
					{
						indices.push_back(idx + initialVtx);
					});
			}

			// Load vertex positions
			{
				fastgltf::Accessor& posAccessor = gltf.accessors[p.findAttribute("POSITION")->accessorIndex];
				vertices.resize(vertices.size() + posAccessor.count);

				fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
					[&](glm::vec3 v, size_t index)
					{
						Vertex newvtx;
						newvtx.Position = v;
						newvtx.Normal = { 1, 0, 0 };
						newvtx.Color = glm::vec4{ 1.f };
						newvtx.UVX = 0;
						newvtx.UVY = 0;
						vertices[initialVtx + index] = newvtx;
					});
			}

			// Load vertex normals
			auto normals = p.findAttribute("NORMAL");
			if (normals != p.attributes.end())
			{
				fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[(*normals).accessorIndex],
					[&](glm::vec3 v, size_t index)
					{
						vertices[initialVtx + index].Normal = v;
					});
			}

			// Load UVs
			auto uv = p.findAttribute("TEXCOORD_0");
			if (uv != p.attributes.end())
			{
				fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[(*uv).accessorIndex],
					[&](glm::vec2 v, size_t index)
					{
						vertices[initialVtx + index].UVX = v.x;
						vertices[initialVtx + index].UVY = v.y;
					});
			}

			// Load vertex colors
			auto colors = p.findAttribute("COLOR_0");
			if (colors != p.attributes.end())
			{
				auto& accessor = gltf.accessors[(*colors).accessorIndex];
				if (accessor.type == fastgltf::AccessorType::Vec3)
				{
					fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[(*colors).accessorIndex],
						[&](glm::vec3 v, size_t index)
						{
							vertices[initialVtx + index].Color = glm::vec4(v, 1.0f);
						});
				}
				else
				{
					fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[(*colors).accessorIndex],
						[&](glm::vec4 v, size_t index)
						{
							vertices[initialVtx + index].Color = v;
						});
				}
			}

			if (p.materialIndex.has_value())
			{
				newSurface.Material = materials[p.materialIndex.value()];
			}
			else
			{
				newSurface.Material = materials[0];
			}

			glm::vec3 minpos = vertices[initialVtx].Position;
			glm::vec3 maxpos = vertices[initialVtx].Position;

			for (int i = initialVtx; i < vertices.size(); i++)
			{
				minpos = glm::min(minpos, vertices[i].Position);
				maxpos = glm::max(maxpos, vertices[i].Position);
			}

			newSurface.BoundingBox.Origin = (maxpos + minpos) / 2.0f;
			newSurface.BoundingBox.Extents = (maxpos - minpos) / 2.0f;
			newSurface.BoundingBox.SphereRadius = glm::length(newSurface.BoundingBox.Extents);
			newmesh->Surfaces.push_back(newSurface);
		}

		newmesh->MeshBuffers = engine->UploadMesh(indices, vertices);
	}

	// Load all nodes and their meshes
	for (fastgltf::Node& node : gltf.nodes)
	{
		std::shared_ptr<Node> newNode;

		// Find if the node has a mesh, and if it does hook it to the mesh pointer and allocate it with the meshnode class
		if (node.meshIndex.has_value())
		{
			newNode = std::make_shared<MeshNode>();
			static_cast<MeshNode*>(newNode.get())->Mesh = meshes[*node.meshIndex];
		}
		else
		{
			newNode = std::make_shared<Node>();
		}

		nodes.push_back(newNode);
		file.Nodes[node.name.c_str()];
		std::visit(fastgltf::visitor{ [&](fastgltf::math::fmat4x4 matrix)
			{
				memcpy(&newNode->LocalTransform, matrix.data(), sizeof(matrix));
			},
			[&](fastgltf::TRS transform)
			{
				glm::vec3 tl(transform.translation[0], transform.translation[1],
					transform.translation[2]);
				glm::quat rot(transform.rotation[3], transform.rotation[0], transform.rotation[1],
					transform.rotation[2]);
				glm::vec3 sc(transform.scale[0], transform.scale[1], transform.scale[2]);

				glm::mat4 tm = glm::translate(glm::mat4(1.f), tl);
				glm::mat4 rm = glm::toMat4(rot);
				glm::mat4 sm = glm::scale(glm::mat4(1.f), sc);

				newNode->LocalTransform = tm * rm * sm;
			} },
			node.transform);
	}

	// Run loop again to setup transform hierarchy
	for (int i = 0; i < gltf.nodes.size(); i++)
	{
		fastgltf::Node& node = gltf.nodes[i];
		std::shared_ptr<Node>& sceneNode = nodes[i];

		for (auto& c : node.children)
		{
			sceneNode->Children.push_back(nodes[c]);
			nodes[c]->Parent = sceneNode;
		}
	}

	// Find the top nodes, with no parents
	for (auto& node : nodes)
	{
		if (node->Parent.lock() == nullptr)
		{
			file.TopNodes.push_back(node);
			node->RefreshTransform(glm::mat4{ 1.f });
		}
	}
	return scene;
}

void LoadedGLTF::Draw(const glm::mat4& topMatrix, DrawContext& ctx)
{
	// Create renderables from the scenenodes
	for (auto& n : TopNodes)
	{
		n->Draw(topMatrix, ctx);
	}
}

void LoadedGLTF::ClearAll()
{
	VkDevice dv = Engine->Device;

	DescriptorPool.DestroyPools(dv);
	Engine->DestroyBuffer(MaterialDataBuffer);

	for (auto& [k, v] : Meshes)
	{

		Engine->DestroyBuffer(v->MeshBuffers.IndexBuffer);
		Engine->DestroyBuffer(v->MeshBuffers.VertexBuffer);
	}

	for (auto& image : Images)
	{
		if (image.Image == Engine->ErrorCheckerboardImage.Image)
		{
			// Dont destroy the default images
			continue;
		}
		Engine->DestroyImage(image);
	}

	for (auto& sampler : Samplers)
	{
		vkDestroySampler(dv, sampler, nullptr);
	}
}
