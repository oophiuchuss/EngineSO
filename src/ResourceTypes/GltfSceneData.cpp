module;

#include <string>
#include <vector>
#include <optional>
#include <stdexcept>
#include <iostream>
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

import ResourceHandle;
import Geometry;
import Material;
import TextureData;
import MeshData;
import MaterialProperties;

bool GltfSceneData::LoadResource(const std::string& FilePath)
{
	std::cout << "[GltfSceneData] Loading: " << FilePath << "\n";

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

	for (size_t i = 0; i < Asset.images.size(); i++)
	{
		auto& Image = Asset.images[i];
		RawTexture Raw;

		// Use index as fallback name if image has no name
		Raw.Name = Image.name.empty() ? "texture_" + std::to_string(i) : std::string(Image.name);

		std::visit(fastgltf::visitor
			{
				// External file referenced by URI
				[&](fastgltf::sources::URI& URI)
				{
					Raw.FilePath = std::filesystem::path(FilePath)
						.parent_path()
						.append(URI.uri.path())
						.string();
					Raw.bIsEmbedded = false;
				},

			// Embedded via buffer view
			[&](fastgltf::sources::BufferView& BV)
			{
				auto& BufferView = Asset.bufferViews[BV.bufferViewIndex];
				auto& Buffer = Asset.buffers[BufferView.bufferIndex];

				std::visit(fastgltf::visitor
				{
					[&](fastgltf::sources::Array& Arr)
					{
						const uint8_t* Start = reinterpret_cast<const uint8_t*>(
							Arr.bytes.data()) + BufferView.byteOffset;
						Raw.EmbeddedData.assign(Start, Start + BufferView.byteLength);
						Raw.bIsEmbedded = true;
					},
					[&](fastgltf::sources::Vector& Vec)
					{
						const uint8_t* Start = reinterpret_cast<const uint8_t*>(
							Vec.bytes.data()) + BufferView.byteOffset;
						Raw.EmbeddedData.assign(Start, Start + BufferView.byteLength);
						Raw.bIsEmbedded = true;
					},
					[&](fastgltf::sources::ByteView& BView)
					{
						const uint8_t* Start = reinterpret_cast<const uint8_t*>(
							BView.bytes.data()) + BufferView.byteOffset;
						Raw.EmbeddedData.assign(Start, Start + BufferView.byteLength);
						Raw.bIsEmbedded = true;
					},
					[&](auto& Unknown)
					{
						std::cerr << "[GltfSceneData] Unhandled buffer source type for image: "
								  << Raw.Name << "\n";
					}
				}, Buffer.data);
			},

			// Image data directly in array
			[&](fastgltf::sources::Array& Arr)
			{
				const uint8_t* Start = reinterpret_cast<const uint8_t*>(Arr.bytes.data());
				Raw.EmbeddedData.assign(Start, Start + Arr.bytes.size());
				Raw.bIsEmbedded = true;
			},

			// Image data in vector
			[&](fastgltf::sources::Vector& Vec)
			{
				const uint8_t* Start = reinterpret_cast<const uint8_t*>(Vec.bytes.data());
				Raw.EmbeddedData.assign(Start, Start + Vec.bytes.size());
				Raw.bIsEmbedded = true;
			},

			// ByteView — slice into existing memory
			[&](fastgltf::sources::ByteView& BView)
			{
				const uint8_t* Start = reinterpret_cast<const uint8_t*>(BView.bytes.data());
				Raw.EmbeddedData.assign(Start, Start + BView.bytes.size());
				Raw.bIsEmbedded = true;
			},

			// Fallback — log so we know what we're missing
			[&](auto& Unknown)
			{
				std::cerr << "[GltfSceneData] Unhandled image source type for image: "
						  << Raw.Name << " — texture will be missing\n";
			}
			}, Image.data);

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
	
	std::cout << "[GltfSceneData] Parsed: "
		<< RawMeshes.size() << " meshes, "
		<< RawMaterials.size() << " materials, "
		<< RawTextures.size() << " textures, "
		<< RawNodes.size() << " nodes\n";

	return true;
}

glm::mat4 GltfSceneData::ResolveNodeTransform(const fastgltf::Node& Node) const
{
	return std::visit(fastgltf::visitor
		{
			[](const fastgltf::TRS& TRS) -> glm::mat4
			{
				 glm::vec3 T(TRS.translation[0], TRS.translation[1], TRS.translation[2]);
				 // fastgltf quaternion: (x, y, z, w)
				 // GLM quaternion:      (w, x, y, z)
				 glm::quat R(
					 TRS.rotation[3], TRS.rotation[0],
					 TRS.rotation[1], TRS.rotation[2]);
				 glm::vec3 S(TRS.scale[0], TRS.scale[1], TRS.scale[2]);

				 return glm::translate(glm::mat4(1.0f), T) *
						glm::mat4_cast(R) *
						glm::scale(glm::mat4(1.0f), S);
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
	std::cout << "[GltfSceneData] Instantiating: " << GetResourceID() << "\n";

	auto TexutreIDs = RegisterAllTextures();
	auto MaterialIDs = RegisterAllMaterials(TexutreIDs);
	RegisterAllNodes(MaterialIDs);

	bIsInstantiated = true;
	std::cout << "[GltfSceneData] Instantiation complete\n";
}

std::vector<std::string> GltfSceneData::RegisterAllTextures()
{
	std::cout << "[GltfSceneData] Registering " << RawTextures.size() << " textures...\n";

	std::vector<std::string> TextureIDs;
	TextureIDs.reserve(RawTextures.size());

	for (size_t i = 0; i < RawTextures.size(); i++)
	{
		std::cout << "[GltfSceneData] Texture " << (i + 1)
			<< "/" << RawTextures.size()
			<< ": " << RawTextures[i].Name << "\n";

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

	std::cout << "[GltfSceneData] Textures done\n";

	return TextureIDs;
}

std::vector<std::string> GltfSceneData::RegisterAllMaterials(const std::vector<std::string>& TextureIDs)
{
	std::cout << "[GltfSceneData] Registering " << RawMaterials.size() << " materials...\n";

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

	for (size_t i = 0; i < RawMaterials.size(); i++)
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

	std::cout << "[GltfSceneData] Materials done\n";

	return MaterialIDs;
}

void GltfSceneData::RegisterAllNodes(const std::vector<std::string>& MaterialIDs)
{
	std::cout << "[GltfSceneData] Registering meshes from "
		<< RawNodes.size() << " nodes...\n";

	// Maps raw node index -> SceneNodeEntry index
	// Every node gets an entry regardless of whether it has a mesh
	std::vector<int> NodeToEntryIndex(RawNodes.size(), -1);

	// Create MeshData and NodeEntries for every node with a mesh
	for (size_t NodeIdx = 0; NodeIdx < RawNodes.size(); NodeIdx++)
	{
		auto& Node = RawNodes[NodeIdx];

		SceneNodeEntry Entry;
		Entry.Name = Node.Name;
		Entry.LocalTransform = Node.LocalTransform;
		Entry.ParentIndex = std::nullopt; // resolved later
		Entry.bHasMesh = Node.MeshIndex >= 0;

		if (Entry.bHasMesh)
		{
			auto& RawMesh = RawMeshes[Node.MeshIndex];

			// For nodes with multiple primitives we create one entry per primitive
			// but only the first primitive goes into this entry —
			// additional primitives get their own entries added below

			if (!RawMesh.Primitives.empty())
			{
				auto& Prim = RawMesh.Primitives[0];

				// Unique ID per primitive
				std::string MeshID = GetResourceID() + "_mesh_" + std::to_string(Node.MeshIndex) + "_prim_0";

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

				Entry.MeshID = MeshID;

				// Resolve material — fall back to "default" if none assigned
				std::string MatID = "default";

				if (Prim.MaterialIndex >= 0 && Prim.MaterialIndex < static_cast<int>(MaterialIDs.size()))
				{
					MatID = MaterialIDs[Prim.MaterialIndex];
				}

				Entry.Name = RawMesh.Name;

				if (RawMesh.Primitives.size() > 1)
				{
					Entry.Name = RawMesh.Name + "_prim_0";
				}
			}
		}

		int EntryIndex = static_cast<int>(NodeEntries.size());
		NodeToEntryIndex[NodeIdx] = EntryIndex;
		NodeEntries.push_back(std::move(Entry));

		// Additional primitives — each gets its own entry as a child of this entry
		if (Node.MeshIndex >= 0)
		{
			auto& RawMesh = RawMeshes[Node.MeshIndex];
			for (size_t PrimIdx = 1; PrimIdx < RawMesh.Primitives.size(); PrimIdx++)
			{
				auto& Prim = RawMesh.Primitives[PrimIdx];

				std::string MeshID = GetResourceID() + "_mesh_" + std::to_string(Node.MeshIndex) + "_prim_" + std::to_string(PrimIdx);

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

				ResourceManagerRef.Load<MeshData>(MeshID, std::move(Vertices), Prim.Indices);

				std::string MatID = "default";

				if (Prim.MaterialIndex >= 0 && Prim.MaterialIndex < static_cast<int>(MaterialIDs.size()))
				{
					MatID = MaterialIDs[Prim.MaterialIndex];
				}

				// Extra primitive entry — identity local transform,
				SceneNodeEntry PrimEntry;
				PrimEntry.Name = RawMesh.Name + "_prim_" + std::to_string(PrimIdx);
				PrimEntry.LocalTransform = glm::mat4(1.0f); // transform inherited from parent
				PrimEntry.bHasMesh = true;
				PrimEntry.MeshID = MeshID;
				PrimEntry.MaterialID = MatID;
				PrimEntry.ParentIndex = EntryIndex; // parent is the first primitive entry

				int PrimEntryIndex = static_cast<int>(NodeEntries.size());
				NodeEntries[EntryIndex].ChildIndices.push_back(PrimEntryIndex);
				NodeEntries.push_back(std::move(PrimEntry));
			}
		}
	}
			
	// Resolve parent / child links between NodeEntries
	for (size_t NodeIdx = 0; NodeIdx < RawNodes.size(); NodeIdx++)
	{
		auto& Node = RawNodes[NodeIdx];
		int EntryIndex = NodeToEntryIndex[NodeIdx];
		if (EntryIndex < 0)
		{
			continue;
		}

		// Set parent
		if (Node.ParentIndex.has_value())
		{
			int ParentNodeIdx = Node.ParentIndex.value();
			int ParentEntryIdx = NodeToEntryIndex[ParentNodeIdx];

			if (ParentEntryIdx >= 0)
			{
				NodeEntries[EntryIndex].ParentIndex = ParentEntryIdx;
				NodeEntries[ParentEntryIdx].ChildIndices.push_back(EntryIndex);
			}
		}
	}

	int MeshEntriesCount = std::count_if(NodeEntries.begin(), NodeEntries.end(),
		[](const SceneNodeEntry& E) { return E.bHasMesh; });

	std::cout << "[GltfSceneData] Nodes done — "
		<< NodeEntries.size() << " entries ("
		<< MeshEntriesCount
		<< " with meshes)\n";
}

void GltfSceneData::UnloadResource()
{
	RawMeshes.clear();
	RawMaterials.clear();
	RawTextures.clear();
	RawNodes.clear();

	// Call base to clear NodeEntries and bIsInstantiated
	SceneData::UnloadResource();
}
