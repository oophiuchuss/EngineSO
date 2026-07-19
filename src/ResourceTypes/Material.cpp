module;

#include <glm/glm.hpp>
#include <string>

module Material;

bool Material::Reprocess(const ReprocessOptions& Options)
{
    const auto* MaterialOptions = dynamic_cast<const MaterialReprocessOptions*>(&Options);

    if (!MaterialOptions || MaterialOptions->bFullReconstruct)
    {
        return false;
    }

    Properties = MaterialOptions->Properties;
    return true;
}

bool Material::LoadResource(const std::string& FilePath)
{
    // Already populated via programmatic constructor
    if (Type != MaterialType::PBR || Properties.AlbedoColor != glm::vec4(0.0f))
        return true;


    // TODO: implement proper .mat file parsing
    // For now just succeeds with default values so materials
    // can be created programmatically without a file
    return true;
}

void Material::UnloadResource()
{
    Type = MaterialType::PBR;
    Properties = MaterialProperties{};
    AlbedoTexture = {};
    NormalTexture = {};
    MetallicRoughnessTexture = {};
    OcclusionTexture = {};
    EmissiveTexture = {};
}
