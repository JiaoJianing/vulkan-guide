#include <vk_loader.h>
#include "stb_image.h"
#include <iostream>

#include "vk_engine.h"
#include "vk_initializers.h"
#include "vk_types.h"
#include "vk_images.h"
#include <glm/gtx/quaternion.hpp>

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>

VkFilter extract_filter(fastgltf::Filter filter)
{
	switch (filter)
	{
	case fastgltf::Filter::Nearest:
	case fastgltf::Filter::NearestMipMapNearest:
	case fastgltf::Filter::NearestMipMapLinear:
		return VK_FILTER_NEAREST;
	case fastgltf::Filter::Linear:
	case fastgltf::Filter::LinearMipMapNearest:
	case fastgltf::Filter::LinearMipMapLinear:
		return VK_FILTER_LINEAR;
	default:
		return VK_FILTER_LINEAR;
	}
}

VkSamplerMipmapMode extract_mipmap_mode(fastgltf::Filter filter)
{
	switch (filter)
	{
	case fastgltf::Filter::NearestMipMapNearest:
	case fastgltf::Filter::LinearMipMapNearest:
		return VK_SAMPLER_MIPMAP_MODE_NEAREST;
	case fastgltf::Filter::NearestMipMapLinear:
	case fastgltf::Filter::LinearMipMapLinear:
		return VK_SAMPLER_MIPMAP_MODE_LINEAR;
	default:
		return VK_SAMPLER_MIPMAP_MODE_LINEAR;
	}
}

std::optional<std::shared_ptr<LoadedGLTF>> loadGltf(VulkanEngine* engine, std::string_view filePath)
{
	fmt::print("Loading GLTF: {}", filePath);

	std::shared_ptr<LoadedGLTF> scene = std::make_shared<LoadedGLTF>();
	scene->creator = engine;
	LoadedGLTF& file = *scene.get();

	fastgltf::Parser parser{};
	constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble |
		fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers;

	fastgltf::GltfDataBuffer data;
	data.loadFromFile(filePath);
	fastgltf::Asset gltf;
	std::filesystem::path path = filePath;
	// 区分是gltf还是glb
	auto type = fastgltf::determineGltfFileType(&data);
	if (type == fastgltf::GltfType::glTF)
	{
		auto load = parser.loadGLTF(&data, path.parent_path(), gltfOptions);
		if (load)
		{
			gltf = std::move(load.get());
		}
		else
		{
			std::cerr << "Failed to load gltf: " << fastgltf::to_underlying(load.error()) << std::endl;
			return {};
		}
	}
	else if (type == fastgltf::GltfType::GLB)
	{
		auto load = parser.loadBinaryGLTF(&data, path.parent_path(), gltfOptions);
		if (load)
		{
			gltf = std::move(load.get());
		}
		else
		{
			std::cerr << "Failed to load gltf: " << fastgltf::to_underlying(load.error()) << std::endl;
			return {};
		}
	}
	else
	{
		std::cerr << "Failed to determine gltf container" << std::endl;
		return {};
	}

	// 初始化描述符池
	std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes =
	{
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}
	};
	file.descriptorPool.init(engine->_device, gltf.materials.size(), sizes);

	// 加载采样器
	for (fastgltf::Sampler& sampler : gltf.samplers)
	{
		VkSamplerCreateInfo samplerInfo = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,.pNext = nullptr };
		samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
		samplerInfo.minLod = 0;
		samplerInfo.magFilter = extract_filter(sampler.magFilter.value_or(fastgltf::Filter::Nearest));
		samplerInfo.minFilter = extract_filter(sampler.minFilter.value_or(fastgltf::Filter::Nearest));
		samplerInfo.mipmapMode = extract_mipmap_mode(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

		VkSampler newSampler;
		vkCreateSampler(engine->_device, &samplerInfo, nullptr, &newSampler);

		file.samplers.push_back(newSampler);
	}

	// 中间结构 因为gltf中引用使用的是目标的索引
	std::vector<std::shared_ptr<MeshAsset>> meshes;
	std::vector<std::shared_ptr<Node>> nodes;
	std::vector<AllocatedImage> images;
	std::vector<std::shared_ptr<GLTFMaterial>> materials;

	// 加载纹理
	for (fastgltf::Image& image : gltf.images)
	{
		std::optional<AllocatedImage> img = vkutil::load_image(engine, gltf, image);
		if (img.has_value())
		{
			images.push_back(*img);
			file.images[image.name.c_str()] = *img;
		}
		else
		{
			images.push_back(engine->_errorCheckerboardImage);
			std::cout << "gltf failed to load texture: " << image.name << std::endl;
		}
	}

	// 创建并填充一个大的缓冲区 存储所有的材质数据
	file.materialDataBuffer = engine->create_buffer(sizeof(GLTFMetallic_Roughness::MaterialConstants) * gltf.materials.size(),
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
	int data_index = 0;
	GLTFMetallic_Roughness::MaterialConstants* sceneMaterialConstants = (GLTFMetallic_Roughness::MaterialConstants*)file.materialDataBuffer.info.pMappedData;
	for (fastgltf::Material& mat : gltf.materials)
	{
		std::shared_ptr<GLTFMaterial> newMat = std::make_shared<GLTFMaterial>();
		materials.push_back(newMat);
		file.materials[mat.name.c_str()] = newMat;

		GLTFMetallic_Roughness::MaterialConstants constants;
		constants.colorFactors.x = mat.pbrData.baseColorFactor[0];
		constants.colorFactors.y = mat.pbrData.baseColorFactor[1];
		constants.colorFactors.z = mat.pbrData.baseColorFactor[2];
		constants.colorFactors.w = mat.pbrData.baseColorFactor[3];
		constants.metal_rough_factors.x = mat.pbrData.metallicFactor;
		constants.metal_rough_factors.y = mat.pbrData.roughnessFactor;
		sceneMaterialConstants[data_index] = constants;

		MaterialPass passType = MaterialPass::MainColor;
		if (mat.alphaMode == fastgltf::AlphaMode::Blend)
		{
			passType = MaterialPass::Transparent;
		}

		GLTFMetallic_Roughness::MaterialResources materialResources;
		materialResources.colorImage = engine->_whiteImage;
		materialResources.colorSampler = engine->_defaultSamplerLinear;
		materialResources.metalRoughImage = engine->_whiteImage;
		materialResources.metalRoughSampler = engine->_defaultSamplerLinear;
		if (mat.pbrData.baseColorTexture.has_value())
		{
			size_t img = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].imageIndex.value();
			size_t sampler = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].samplerIndex.value();

			materialResources.colorImage = images[img];
			materialResources.colorSampler = file.samplers[sampler];
		}

		// 注意设置合适的offset
		materialResources.dataBuffer = file.materialDataBuffer.buffer;
		materialResources.dataBufferOffset = data_index * sizeof(GLTFMetallic_Roughness::MaterialConstants);

		newMat->data = engine->_metalRoughMaterial.write_material(engine->_device, passType, materialResources, file.descriptorPool);

		data_index++;
	}

	// 加载网格信息
	std::vector<uint32_t> indices;
	std::vector<Vertex> vertices;
	for (fastgltf::Mesh& mesh : gltf.meshes)
	{
		std::shared_ptr<MeshAsset> newMesh = std::make_shared<MeshAsset>();
		meshes.push_back(newMesh);
		file.meshes[mesh.name.c_str()] = newMesh;
		newMesh->name = mesh.name;

		indices.clear();
		vertices.clear();

		for (auto&& p : mesh.primitives)
		{
			GeoSurface newSurface;
			newSurface.startIndex = (uint32_t)indices.size();
			newSurface.count = (uint32_t)gltf.accessors[p.indicesAccessor.value()].count;

			size_t initial_vtx = vertices.size();

			// load indices
			{
				fastgltf::Accessor& indexAccessor = gltf.accessors[p.indicesAccessor.value()];
				indices.reserve(indices.size() + indexAccessor.count);
				fastgltf::iterateAccessor<std::uint32_t>(gltf, indexAccessor, [&](std::uint32_t idx)
					{
						indices.push_back(idx + initial_vtx);
					});
			}
			// load vertices
			{
				fastgltf::Accessor& posAccessor = gltf.accessors[p.findAttribute("POSITION")->second];
				vertices.resize(vertices.size() + posAccessor.count);
				fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor, [&](glm::vec3 v, size_t index)
					{
						Vertex newVtx;
						newVtx.position = v;
						newVtx.normal = { 1,0,0 };
						newVtx.color = glm::vec4{ 1.0f };
						newVtx.uv_x = 0;
						newVtx.uv_y = 0;
						vertices[initial_vtx + index] = newVtx;
					});
			}
			// load vertex normals
			auto normals = p.findAttribute("NORMAL");
			if (normals != p.attributes.end())
			{
				fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[(*normals).second], [&](glm::vec3 n, size_t index)
					{
						vertices[initial_vtx + index].normal = n;
					});
			}
			// load UVS
			auto uvs = p.findAttribute("TEXCOORD_0");
			if (uvs != p.attributes.end())
			{
				fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[(*uvs).second], [&](glm::vec2 uv, size_t index)
					{
						vertices[initial_vtx + index].uv_x = uv.x;
						vertices[initial_vtx + index].uv_y = uv.y;
					});
			}
			// load vertex colors
			auto colors = p.findAttribute("COLOR_0");
			if (colors != p.attributes.end())
			{
				fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[(*colors).second], [&](glm::vec4 c, size_t index)
					{
						vertices[initial_vtx + index].color = c;
					});
			}

			if (p.materialIndex.has_value())
			{
				newSurface.material = materials[p.materialIndex.value()];
			}
			else
			{
				newSurface.material = materials[0];
			}

			// 计算newSurface的包围信息 用于后续的视锥体剔除
			glm::vec3 minPos = vertices[initial_vtx].position;
			glm::vec3 maxPos = vertices[initial_vtx].position;
			for (int i = initial_vtx; i < vertices.size(); i++)
			{
				minPos = glm::min(minPos, vertices[i].position);
				maxPos = glm::max(maxPos, vertices[i].position);
			}
			newSurface.bounds.origin = (maxPos + minPos) / 2.0f;
			newSurface.bounds.extents = (maxPos - minPos) / 2.0f;
			newSurface.bounds.sphereRadius = glm::length(newSurface.bounds.extents);

			newMesh->surfaces.push_back(newSurface);
		}

		newMesh->meshBuffers = engine->uploadMesh(indices, vertices);
	}

	// 加载节点
	for (fastgltf::Node& node : gltf.nodes)
	{
		std::shared_ptr<Node> newNode;
		if (node.meshIndex.has_value())
		{
			newNode = std::make_shared<MeshNode>();
			static_cast<MeshNode*>(newNode.get())->mesh = meshes[*node.meshIndex];
		}
		else
		{
			newNode = std::make_shared<Node>();
		}

		nodes.push_back(newNode);
		file.nodes[node.name.c_str()] = newNode;

		std::visit(fastgltf::visitor
			{ 
				[&](fastgltf::Node::TransformMatrix matrix)
					{
						memcpy(&newNode->localTransform, matrix.data(), sizeof(matrix));
					}
				,[&](fastgltf::Node::TRS transform)
					{
						glm::vec3 tl(transform.translation[0], transform.translation[1], transform.translation[2]);
						glm::quat rot(transform.rotation[3], transform.rotation[0], transform.rotation[1], transform.rotation[2]);
						glm::vec3 sc(transform.scale[0], transform.scale[1], transform.scale[2]);

						glm::mat4 tm = glm::translate(glm::mat4(1.0f), tl);
						glm::mat4 rm = glm::toMat4(rot);
						glm::mat4 sm = glm::scale(glm::mat4(1.0f), sc);

						newNode->localTransform = tm * rm * sm;
					}
			}, node.transform);
	}

	// 建立节点父子关系
	for (int i = 0; i < gltf.nodes.size(); i++)
	{
		fastgltf::Node& node = gltf.nodes[i];
		std::shared_ptr<Node>& sceneNode = nodes[i];
		for (auto& c : node.children)
		{
			sceneNode->children.push_back(nodes[c]);
			nodes[c]->parent = sceneNode;
		}
	}

	// 找到所有top节点
	for (auto& node : nodes)
	{
		if (node->parent.lock() == nullptr)
		{
			file.topNodes.push_back(node);
			node->refreshTransform(glm::mat4(1.0f));
		}
	}

	return scene;
}

void LoadedGLTF::Draw(const glm::mat4& topMatrix, DrawContext& ctx)
{
	for (auto& n : topNodes)
	{
		n->Draw(topMatrix, ctx);
	}
}

void LoadedGLTF::ClearAll()
{
	VkDevice dv = creator->_device;

	descriptorPool.destroy_pools(dv);
	creator->destroy_buffer(materialDataBuffer);

	for (auto& [k, v] : meshes)
	{
		creator->destroy_buffer(v->meshBuffers.indexBuffer);
		creator->destroy_buffer(v->meshBuffers.vertexBuffer);
	}

	for (auto& [k, v] : images)
	{
		if (v.image == creator->_errorCheckerboardImage.image)
		{
			continue;
		}
		creator->destroy_image(v);
	}

	for (auto& sampler : samplers)
	{
		vkDestroySampler(dv, sampler, nullptr);
	}
}
