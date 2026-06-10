module;

#include <string>
#include <vector>
#include <optional>
#include <stdexcept>
#include <filesystem>
#include <cstring>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

// fastgltf
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>

module GltfSceneData;

bool GltfSceneData::LoadResource(const std::string& FilePath)
{
	// fastgltf handles both .gltf and .glb  transparently
	// TODO: add figuring out extensions
	fastgltf::Parser Parser;
	
	fastgltf::GltfFileStream FileStream(FilePath.c_str());
	if (!FileStream.isOpen())
	{
		return false;
	}

	// Determine extensions 
	constexpr auto GltfOptions =
		fastgltf::Options::DontRequireValidAssetMember |
		fastgltf::Options::LoadExternalBuffers |
		fastgltf::Options::LoadExternalImages;

	auto LoadResult = Parser.loadGltf(
		FileStream,
		std::filesystem::path(FilePath).parent_path(),
		GltfOptions);

	if (LoadResult.error() != fastgltf::Error::None)
	{
		return false;
	}

	fastgltf::Asset& Asset = LoadResult.get();

	// Parse textures/images

	RawTextures.reserve(Asset.images.size());

	for (auto& Image : Asset.images)
	{
		RawTexture Raw;
		Raw.Name = std::string(Image.name);

		std::visit(fastgltf::visitor
			{
				[&](fastgltf::sources::URI& URI)
				{
					// External file - store path relative to glTF directory
					Raw.FilePath = std::filesystem::path(FilePath)
						.parent_path()
						.append(URI.uri.path())
						.string();

					Raw.bIsEmbedded = false;
				},
				[&](fastgltf::sources::BufferView& BV)
				{
					// Embedded in buffer - copy bytes out
					auto& BufferView = Asset.bufferViews[BV.bufferViewIndex];
					auto& Buffer = Asset.buffers[BufferView.bufferIndex];

					std::visit(fastgltf::visitor
						{
							[&](fastgltf::sources::Array& Arr)
							{
								const uint8_t* Start = reinterpret_cast<uint8_t*>(Arr.bytes.data() + BufferView.byteOffset);
								Raw.EmbeddedData.assign(Start, Start + BufferView.byteLength);
								Raw.bIsEmbedded = true;
							},
							[&](auto& Arr) {} // other buffer sources — ignore for now
						}, Buffer.data);
				},
				[&](auto&) {} // other image sources — ignore for now
			},Image.data);

		RawTextures.push_back(std::move(Raw));
	}

	// Parse materials
	RawMaterials.reserve(Asset.materials.size());

	for (auto& Mat : Asset.materials)
	{
		RawMaterial Raw;
		Raw.Name = std::string(Mat.name);

		// PBR metallic roughness
		auto& PBR = Mat.pbrData;

		Raw.AlbedoFactor = glm::vec4(
			PBR.baseColorFactor[0], PBR.baseColorFactor[1],
			PBR.baseColorFactor[2], PBR.baseColorFactor[3]);
		Raw.MetallicFactor = PBR.metallicFactor;
		Raw.RoughnessFactor = PBR.roughnessFactor;

		if (PBR.baseColorTexture.has_value())
		{
			Raw.AlbedoTextureIndex = static_cast<int>(Asset.textures[PBR.baseColorTexture->textureIndex].imageIndex.value());
		}
		
		if (PBR.metallicRoughnessTexture.has_value())
		{
			Raw.MetallicRoughnessTextureIndex = static_cast<int>(Asset.textures[PBR.metallicRoughnessTexture->textureIndex].imageIndex.value());
		}

		// Normal
		if (Mat.normalTexture.has_value())
		{
			Raw.NormalTextureIndex = static_cast<int>(Asset.textures[Mat.normalTexture->textureIndex].imageIndex.value());
			Raw.NormalScale = Mat.normalTexture->scale;
		}

		// Occlusion
		if (Mat.occlusionTexture.has_value())
		{
			Raw.OcclusionTextureIndex = static_cast<int>(Asset.textures[Mat.occlusionTexture->textureIndex].imageIndex.value());
			Raw.OcclusionStrength = Mat.occlusionTexture->strength;
		}

		// Emissive
		Raw.EmissiveFactor = glm::vec3(
			Mat.emissiveFactor[0],
			Mat.emissiveFactor[1],
			Mat.emissiveFactor[2]);

		if (Mat.emissiveTexture.has_value())
		{
			Raw.EmissiveTextureIndex = static_cast<int>(Asset.textures[Mat.emissiveTexture->textureIndex].imageIndex.value());
			// TODO: what about emissiveStrength?
		}

		RawMaterials.push_back(std::move(Raw));
	}

	// Parse meshes
	RawMeshes.reserve(Asset.meshes.size());

	for (auto& Mesh : Asset.meshes)
	{
		RawMesh Raw;
		Raw.Name = std::string(Mesh.name);

		for (auto& Primitive : Mesh.primitives)
		{
			RawPrimitive RawPrim;
			if (Primitive.materialIndex.has_value())
			{
				RawPrim.MaterialIndex = static_cast<int>(Primitive.materialIndex.value());
			}

			// Positions - required
			auto* Attr = Primitive.findAttribute("POSITION");
			if (Attr != Primitive.attributes.end())
			{
				auto& Accessor = Asset.accessors[Attr->accessorIndex];
				RawPrim.Positions.resize(Accessor.count);
				fastgltf::iterateAccessor<fastgltf::math::fvec3>(
					Asset, Accessor,
					[&, i = 0](fastgltf::math::fvec3 V) mutable
					{
						RawPrim.Positions[i++] = glm::vec3(V.x(), V.y(), V.z());
					});
			}

			// UVs — optional
			if (auto* Attr = Primitive.findAttribute("TEXCOORD_0"); Attr != Primitive.attributes.end())
			{
				auto& Accessor = Asset.accessors[Attr->accessorIndex];
				RawPrim.UVs.resize(Accessor.count);
				fastgltf::iterateAccessor<fastgltf::math::fvec2>(
					Asset, Accessor,
					[&, i = 0](fastgltf::math::fvec2 V) mutable
					{
						RawPrim.UVs[i++] = glm::vec2(V.x(), V.y());
					});
			}

			// Normals — optional
			if (auto* Attr = Primitive.findAttribute("NORMAL"); Attr != Primitive.attributes.end())
			{
				auto& Accessor = Asset.accessors[Attr->accessorIndex];
				RawPrim.Normals.resize(Accessor.count);
				fastgltf::iterateAccessor<fastgltf::math::fvec3>(
					Asset, Accessor,
					[&, i = 0](fastgltf::math::fvec3 V) mutable
					{
						RawPrim.Normals[i++] = glm::vec3(V.x(), V.y(), V.z());
					});
			}

			// Indices — optional (non-indexed geometry is valid)
			if (Primitive.indicesAccessor.has_value())
			{
				auto& Accessor = Asset.accessors[Primitive.indicesAccessor.value()];
				RawPrim.Indices.resize(Accessor.count);
				fastgltf::iterateAccessor<uint32_t>(
					Asset, Accessor,
					[&, i = 0](uint32_t Idx) mutable
					{
						RawPrim.Indices[i++] = Idx;
					});
			}

			Raw.Primitives.push_back(std::move(RawPrim));
		}

		RawMeshes.push_back(std::move(Raw));
	}

	// Parse nodes
	RawNodes.resize(Asset.nodes.size());

	for (size_t i = 0; i < Asset.nodes.size(); i++)
	{
		auto& Node = Asset.nodes[i];
		auto& RawNode = RawNodes[i];

		RawNode.Name = std::string(Node.name);
		if (Node.meshIndex.has_value())
		{
			RawNode.MeshIndex = static_cast<int>(Node.meshIndex.value());
		}

		for (auto& ChildIdx : Node.children)
		{
			RawNode.ChildIndices.push_back(static_cast<int>(ChildIdx));
			RawNodes[ChildIdx].ParentIndex = static_cast<int>(i);
		}
		
		// Transform — handle both TRS and matrix
		RawNode.LocalTransform = ResolveNodeTransform(Node);
	}

	// TODO: parse light
	return true;
}

glm::mat4 GltfSceneData::ResolveNodeTransform(const fastgltf::Node& Node) const
{
	return std::visit(fastgltf::visitor
		{
			[](const fastgltf::TRS& TRS) -> glm::mat4
			{
				glm::vec3 T(TRS.translation[0], TRS.translation[1], TRS.translation[2]);
				glm::quat R(
					TRS.rotation[0], TRS.rotation[1],
					TRS.rotation[2], TRS.rotation[3]);
				glm::vec3 S(TRS.scale[0], TRS.scale[1], TRS.scale[2]);

				return glm::translate(glm::mat4(1.0f), T) * glm::mat4_cast(R) * glm::scale(glm::mat4(1.0f), S);
			},
			[](const fastgltf::math::fmat4x4& M) -> glm::mat4
			{
				// fastgltf matrix is column major — matches GLM directly
				return glm::make_mat4(M.data());
			}
		}, Node.transform);
}

void GltfSceneData::Instantiate()
{
	auto TexutreIDs = RegisterAllTextures();
	auto MaterialIDs = RegisterAllMaterials(TexutreIDs);
	RegisterAllMeshes(MaterialIDs);
	bIsInstantiated = true;
}

std::vector<std::string> GltfSceneData::RegisterAllTextures()
{
	return std::vector<std::string>();
}

std::vector<std::string> GltfSceneData::RegisterAllMaterials(const std::vector<std::string>& TextureIDs)
{
	return std::vector<std::string>();
}

void GltfSceneData::RegisterAllMeshes(const std::vector<std::string>& MaterialIDs)
{
}

void GltfSceneData::UnloadResource()
{
	RawMeshes.clear();
	RawMaterials.clear();
	RawTextures.clear();
	RawNodes.clear();

	// Call base to clear MeshEntries and bIsInstantiated
	SceneData::UnloadResource();
}
