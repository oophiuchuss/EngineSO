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
	fastgltf::Parser Parser{ fastgltf::Extensions::KHR_lights_punctual };
	
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

	Asset.extensionsUsed;

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

	// Parse lights (KHR_lights_punctual extension)
	RawLights.reserve(Asset.lights.size());

	for (auto& Light : Asset.lights)
	{
		RawLight R;
		R.Name = std::string(Light.name);

		R.Color = glm::vec3(Light.color[0], Light.color[1], Light.color[2]);

		R.Intensity = Light.intensity;

		if (Light.range.has_value())
		{
			R.Range = Light.range.value();
		}

		R.Type = Light.type;

		if (Light.type == fastgltf::LightType::Spot)
		{
			R.InnerConeAngle = Light.innerConeAngle.has_value() ? Light.innerConeAngle.value() : 0.0f;
			R.OuterConeAngle = Light.outerConeAngle.has_value() ? Light.outerConeAngle.value() : glm::radians(45.0f);
		}

		RawLights.push_back(std::move(R));
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

		if (Node.lightIndex.has_value())
		{
			RawNode.LightIndex = static_cast<int>(Node.lightIndex.value());
		}

		for (auto& ChildIdx : Node.children)
		{
			RawNode.ChildIndices.push_back(static_cast<int>(ChildIdx));
			RawNodes[ChildIdx].ParentIndex = static_cast<int>(i);
		}
		
		// Transform — handle both TRS and matrix
		RawNode.LocalTransform = ResolveNodeTransform(Node);
	}
	
	std::cout << "[GltfSceneData] Parsed: "
		<< RawMeshes.size() << " meshes, "
		<< RawMaterials.size() << " materials, "
		<< RawTextures.size() << " textures, "
		<< RawLights.size() << " lights, "
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
	std::cout << "[GltfSceneData] Registering " << RawTextures.size() << " textures (parallel decode)......\n";

	std::vector<std::string> TextureIDs;
	TextureIDs.reserve(RawTextures.size());

	for (size_t i = 0; i < RawTextures.size(); i++)
	{
		auto& Raw = RawTextures[i];

		if (Raw.bIsEmbedded)
		{
			std::string TexID = GetResourceID() + "_tex_" + std::to_string(i);
			ResourceManagerRef.PrepareAsyncFromMemory<TextureData>(TexID, Raw.EmbeddedData);
			TextureIDs.push_back(TexID);
		}
		else
		{
			ResourceManagerRef.PrepareAsync<TextureData>(Raw.FilePath);
			TextureIDs.push_back(Raw.FilePath);
		}
	}

	std::cout << "[GltfSceneData] Waiting for texture decodes...\n";
	ResourceManagerRef.WaitForAsyncLoads();
	std::cout << "[GltfSceneData] All textures decoded.\n";

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

	// Helper: create MeshData for a single primitive, returns its MeshID
	auto CreateMeshDataForPrimitive = [&](const RawPrimitive& Prim, int MeshIndex, int PrimIdx) -> std::string
		{
			std::string MeshID = GetResourceID() + "_mesh_" + std::to_string(MeshIndex) + "_prim_" + std::to_string(PrimIdx);

			std::vector<Vertex> Vertices(Prim.Positions.size());
			for (size_t v = 0; v < Prim.Positions.size(); ++v)
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
			return MeshID;
		};

	// Helper: resolve the material ID for a primitive
	auto ResolveMaterial = [&](const RawPrimitive& Prim) -> std::string
		{
			if (Prim.MaterialIndex >= 0 && Prim.MaterialIndex < static_cast<int>(MaterialIDs.size()))
			{
				return MaterialIDs[Prim.MaterialIndex];
			}
			return "default";
		};

	NodeEntries.clear();
	MeshInstances.clear();
	LightInstances.clear();

	// Create MeshData and NodeEntries for every node with a mesh
	for (size_t NodeIdx = 0; NodeIdx < RawNodes.size(); NodeIdx++)
	{
		auto& Node = RawNodes[NodeIdx];

		SceneNodeEntry Entry;
		Entry.Name = Node.Name;
		Entry.LocalTransform = Node.LocalTransform;
		Entry.ParentIndex = std::nullopt; // resolved later

		int FirstPrimEntryIndex = -1;   // index of the first primitive entry (if any)

		if (Node.MeshIndex >= 0)
		{
			auto& RawMesh = RawMeshes[Node.MeshIndex];

			// First primitive – uses the node's local transform
			if (!RawMesh.Primitives.empty())
			{
				const auto& Prim = RawMesh.Primitives[0];
				std::string MeshID = CreateMeshDataForPrimitive(Prim, Node.MeshIndex, 0);
				std::string MaterialID = ResolveMaterial(Prim);

				MeshInstance MI;
				MI.NodeIndex = static_cast<int>(NodeEntries.size());   // current entry index
				MI.MeshID = MeshID;
				MI.MaterialID = MaterialID;
				MeshInstances.push_back(MI);

				Entry.Name = RawMesh.Name;

				// Keep a simple name for multi‑primitive meshes
				if (RawMesh.Primitives.size() > 1)
				{
					Entry.Name += "_prim_0";
				}
			}
		}

		int EntryIndex = static_cast<int>(NodeEntries.size());
		NodeToEntryIndex[NodeIdx] = EntryIndex;
		FirstPrimEntryIndex = EntryIndex;
		NodeEntries.push_back(std::move(Entry));

		// Additional primitives — each gets its own entry as a child of this entry
		if (Node.MeshIndex >= 0)
		{
			auto& RawMesh = RawMeshes[Node.MeshIndex];
			for (size_t PrimIdx = 1; PrimIdx < RawMesh.Primitives.size(); ++PrimIdx)
			{
				const auto& Prim = RawMesh.Primitives[PrimIdx];
				std::string MeshID = CreateMeshDataForPrimitive(Prim, Node.MeshIndex, PrimIdx);
				std::string MaterialID = ResolveMaterial(Prim);

				SceneNodeEntry PrimEntry;
				PrimEntry.Name = RawMesh.Name + "_prim_" + std::to_string(PrimIdx);
				PrimEntry.LocalTransform = glm::mat4(1.0f);          // inherited from parent
				PrimEntry.ParentIndex = FirstPrimEntryIndex;

				int PrimEntryIndex = static_cast<int>(NodeEntries.size());
				NodeEntries[FirstPrimEntryIndex].ChildIndices.push_back(PrimEntryIndex);
				NodeEntries.push_back(std::move(PrimEntry));

				// Mesh instance for this additional primitive
				MeshInstance MI;
				MI.NodeIndex = PrimEntryIndex;
				MI.MeshID = MeshID;
				MI.MaterialID = MaterialID;
				MeshInstances.push_back(MI);
			}
		}

		// Light attachment
		if (Node.LightIndex >= 0 && Node.LightIndex < static_cast<int>(RawLights.size()))
		{
			const auto& L = RawLights[Node.LightIndex];
			LightInstance LI;
			LI.NodeIndex = FirstPrimEntryIndex;   // attach to the first entry of this node
			LI.Color = L.Color;
			LI.Intensity = L.Intensity;
			LI.Range = L.Range;
			LI.InnerConeAngle = L.InnerConeAngle;
			LI.OuterConeAngle = L.OuterConeAngle;

			switch (L.Type)
			{
			case fastgltf::LightType::Directional: LI.Type = LightType::Directional; break;
			case fastgltf::LightType::Point:       LI.Type = LightType::Point;       break;
			case fastgltf::LightType::Spot:        LI.Type = LightType::Spot;        break;
			default: continue; // skip unknown types
			}

			LightInstances.push_back(LI);
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

	int MeshCount = static_cast<int>(MeshInstances.size());
	int LightCount = static_cast<int>(LightInstances.size());

	std::cout << "[GltfSceneData] Nodes done: "
		<< NodeEntries.size() << " entries, "
		<< MeshCount << " mesh instances, "
		<< LightCount << " light instances.\n";
}

void GltfSceneData::UnloadResource()
{
	RawMeshes.clear();
	RawMaterials.clear();
	RawTextures.clear();
	RawLights.clear();
	RawNodes.clear();

	// Call base to clear NodeEntries and bIsInstantiated
	SceneData::UnloadResource();
}
