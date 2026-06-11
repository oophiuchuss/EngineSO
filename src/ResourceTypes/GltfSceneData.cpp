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


import Geometry;
import Material;
import TextureData;
import MeshData;
import MaterialProperties;

bool GltfSceneData::LoadResource(const std::string& FilePath)
{
	// fastgltf handles both .gltf and .glb  transparently
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
	std::vector<std::string> TextureIDs;
	TextureIDs.reserve(RawTextures.size());

	for (size_t i = 0; i < RawTextures.size(); ++i)
	{
		auto& Raw = RawTextures[i];

		if (Raw.bIsEmbedded)
		{
			// Embedded — load from memory, ID is scene-scoped
			std::string TexID = GetResourceID() + "_tex_" + std::to_string(i);
			ResourceManagerRef.LoadFromMemory<TextureData>(TexID, Raw.EmbeddedData);
			TextureIDs.push_back(TexID);
		}
		else
		{
			// External — use file path as ID, consistent with direct loads
			ResourceManagerRef.Load<TextureData>(Raw.FilePath);
			TextureIDs.push_back(Raw.FilePath);
		}
	}

	return TextureIDs;
}

std::vector<std::string> GltfSceneData::RegisterAllMaterials(const std::vector<std::string>& TextureIDs)
{
	std::vector<std::string> MaterialIDs;
	MaterialIDs.reserve(RawMaterials.size());

	auto GetTexHandle = [&](int Idx) -> ResourceHandle<TextureData>
		{
			if (Idx >= 0 && Idx < static_cast<int>(TextureIDs.size()))
			{
				return ResourceManagerRef.GetHandle<TextureData>(TextureIDs[Idx]);
			}
			
			return {};
		};

	for (size_t i = 0; i < RawMaterials.size(); ++i)
	{
		auto& Raw = RawMaterials[i];
		std::string MatID = GetResourceID() + "_mat_" + std::to_string(i);

		MaterialProperties MatProperties;
		MatProperties.AlbedoColor = Raw.AlbedoFactor;
		MatProperties.Metallic = Raw.MetallicFactor;
		MatProperties.Roughness = Raw.RoughnessFactor;
		MatProperties.EmissiveStrength = glm::length(Raw.EmissiveFactor);

		// All data including textures passed to constructor — immutable after
		ResourceManagerRef.Load<Material>(
			MatID,
			MaterialType::PBR,
			MatProperties,
			GetTexHandle(Raw.AlbedoTextureIndex),
			GetTexHandle(Raw.NormalTextureIndex),
			GetTexHandle(Raw.MetallicRoughnessTextureIndex),
			GetTexHandle(Raw.OcclusionTextureIndex),
			GetTexHandle(Raw.EmissiveTextureIndex));

		MaterialIDs.push_back(MatID);
	}

	return MaterialIDs;
}

void GltfSceneData::RegisterAllMeshes(const std::vector<std::string>& MaterialIDs)
{
	// Maps node index -> list of MeshEntry indices it produced
	// Needed to wire parent/child relationships after all entries are built
	std::vector<std::vector<int>> NodeToEntriesIndices(RawNodes.size());

	// Create MeshData and MeshEntries for every node with a mesh
	for (size_t NodeIdx = 0; NodeIdx < RawNodes.size(); NodeIdx++)
	{
		auto& Node = RawNodes[NodeIdx];
		if (Node.MeshIndex < 0)
		{
			continue; // Skip nodes without meshes
		}

		auto& RawMesh = RawMeshes[Node.MeshIndex];

		for (size_t PrimIdx = 0; PrimIdx < RawMesh.Primitives.size(); PrimIdx++)
		{
			auto& Prim = RawMesh.Primitives[PrimIdx];

			// Unique ID per primitive
			std::string MeshID = GetResourceID() + "_mesh_" + std::to_string(Node.MeshIndex) + "_prim_" + std::to_string(PrimIdx);

			// Interleave positions/UVs/normals into Vertex array
			std::vector<Vertex> Vertices(Prim.Positions.size());
			for (size_t v = 0; v < Prim.Positions.size(); v++)
			{
				Vertices[v].Position = Prim.Positions[v];

				if (v < Prim.UVs.size())
				{
					Vertices[v].UV = Prim.UVs[v];
				}

				if (v < Prim.Normals.size())
				{
					Vertices[v].Normal = Prim.Normals[v];
				}
			}

			// Register MeshData — bounding box computed in constructor
			ResourceManagerRef.Load<MeshData>(
				MeshID,
				std::move(Vertices),
				Prim.Indices);

			// Resolve material — fall back to "default" if none assigned
			std::string MatID = "default";

			if (Prim.MaterialIndex >= 0 && Prim.MaterialIndex < static_cast<int>(MaterialIDs.size()))
			{
				MatID = MaterialIDs[Prim.MaterialIndex];
			}
					
			// Build entry, hierarchy resolved later

			SceneMeshEntry Entry;
			Entry.MeshID = MeshID;
			Entry.MaterialID = MatID;
			Entry.LocalTransform = Node.LocalTransform;
			Entry.Name = RawMesh.Name;

			if (RawMesh.Primitives.size() > 1)
			{
				Entry.Name = RawMesh.Name + "_prim_" + std::to_string(PrimIdx);
			}

			int EntryIndex = static_cast<int>(MeshEntries.size());
			NodeToEntriesIndices[NodeIdx].push_back(EntryIndex);
			MeshEntries.push_back(std::move(Entry));
		}
	}

	// Resolve parent / child links between MeshEntries
	for (size_t NodeIdx = 0; NodeIdx < RawNodes.size(); NodeIdx++)
	{
		auto& Node = RawNodes[NodeIdx];

		for (int EntryIdx : NodeToEntriesIndices[NodeIdx])
		{
			// Parent — find closest ancestor node that produced entries
			if (Node.ParentIndex.has_value())
			{
				int ParentNodeIdx = Node.ParentIndex.value();
				if (!NodeToEntriesIndices[ParentNodeIdx].empty())
				{
					MeshEntries[EntryIdx].ParentIndex = NodeToEntriesIndices[ParentNodeIdx].front();
				}
			}

			// Children
			for (int ChildNodeIdx : Node.ChildIndices)
			{
				for (int ChildEntryIdx : NodeToEntriesIndices[ChildNodeIdx])
				{
					MeshEntries[EntryIdx].ChildIndices.push_back(ChildEntryIdx);
				}
			}
		}
	}
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
