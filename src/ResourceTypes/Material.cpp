module;

#include <glm/glm.hpp>
#include <string>

module Material;

bool Material::LoadResource(const std::string& FilePath)
{
    // TODO: implement proper .mat file parsing
    // For now just succeeds with default values so materials
    // can be created programmatically without a file
    return true;
}

void Material::UnloadResource()
{
    Type = MaterialType::PBR;
    PushData = MaterialPushData{};
}
