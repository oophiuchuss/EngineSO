module;

#include <glm/glm.hpp>
#include <string>

export module Material;

import ResourceBase;
import PushConstants;

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

	inline MaterialType GetType() const { return Type; }
	inline const MaterialPushData& GetPushData() const { return PushData; }

	inline void SetType(const MaterialType InType) { Type = InType; }
	inline void SetPushData(const MaterialPushData& InPushData) { PushData = InPushData; }

	inline void SetAlbedo(const glm::vec4& Albedo) { PushData.AlbedoColor = Albedo; }
	inline void SetMetallic(float Metallic) { PushData.Metallic = Metallic; }
	inline void SetRoughness(float Roughness) { PushData.Roughness = Roughness; }
	inline void SetEmissiveStrength(float EmissiveStrength) { PushData.EmissiveStrength = EmissiveStrength; }

protected:
	virtual bool LoadResource(const std::string& FilePath);
	virtual void UnloadResource();

private:
	MaterialType Type = MaterialType::PBR;
	MaterialPushData PushData;
};