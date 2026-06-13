module;

#include <glm/glm.hpp>
#include <string>

export module Material;

import ResourceBase;
import MaterialProperties;
import TextureData;
import ResourceHandle;

// Forward declaration
export template<typename T> class ResourceHandle;

export enum MaterialType
{
	PBR,		// Physically Based Rendering
	Emissive,	// Glowing materials, no lighting applied
	Unlit,		// Debug, UI, skybox
};

export class Material : public ResourceBase
{
public:
	explicit Material(const std::string& InResourceID) : ResourceBase(InResourceID) {}

	// Programmatic constructor — fully immutable after construction
	Material(
		const std::string& ID,
		MaterialType                InType,
		const MaterialProperties&	InProperties,
		ResourceHandle<TextureData> InAlbedo = {},
		ResourceHandle<TextureData> InNormal = {},
		ResourceHandle<TextureData> InMetallicRoughness = {},
		ResourceHandle<TextureData> InOcclusion = {},
		ResourceHandle<TextureData> InEmissive = {})
		: ResourceBase(ID)
		, Type(InType)
		, Properties(InProperties)
		, AlbedoTexture(std::move(InAlbedo))
		, NormalTexture(std::move(InNormal))
		, MetallicRoughnessTexture(std::move(InMetallicRoughness))
		, OcclusionTexture(std::move(InOcclusion))
		, EmissiveTexture(std::move(InEmissive))
	{
	}

	// Getters only
	inline MaterialType GetType() const { return Type; }
	inline const MaterialProperties& GetMaterialProperties() const { return Properties; }

	// Texture handles — public read access
	inline const ResourceHandle<TextureData>& GetAlbedoTexture() const { return AlbedoTexture; }
	inline const ResourceHandle<TextureData>& GetNormalTexture() const { return NormalTexture; }
	inline const ResourceHandle<TextureData>& GetMetallicRoughnessTexture() const { return MetallicRoughnessTexture; }
	inline const ResourceHandle<TextureData>& GetOcclusionTexture() const { return OcclusionTexture; }
	inline const ResourceHandle<TextureData>& GetEmissiveTexture() const { return EmissiveTexture; }

	static std::string_view AssetFolder() { return "materials"; }
	static std::string_view FileExtension() { return ".mat"; }

protected:
	virtual bool LoadResource(const std::string& FilePath);
	virtual void UnloadResource();

private:
	MaterialType Type = MaterialType::PBR;
	MaterialProperties Properties;

	ResourceHandle<TextureData> AlbedoTexture;
	ResourceHandle<TextureData> NormalTexture;
	ResourceHandle<TextureData> MetallicRoughnessTexture;
	ResourceHandle<TextureData> OcclusionTexture;
	ResourceHandle<TextureData> EmissiveTexture;
};