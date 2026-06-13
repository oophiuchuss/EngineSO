module;

#include <string>
#include <vector>
#include <fstream>

module ShaderData;

bool ShaderData::LoadResource(const std::string& BasePath)
{
    // BasePath = "assets/shaders/triangle"
    // Derives both stage paths by convention
    if (!ReadSpirV(BasePath + ".vert.spv", VertexBytecode))   return false;
    if (!ReadSpirV(BasePath + ".frag.spv", FragmentBytecode)) return false;
    return true;
}

void ShaderData::UnloadResource()
{
    VertexBytecode.clear();
    FragmentBytecode.clear();
}

bool ShaderData::ReadSpirV(const std::string& FilePath, std::vector<uint32_t>& OutBytecode)
{
    std::ifstream File(FilePath, std::ios::binary | std::ios::ate);
    if (!File) return false;

    auto FileSize = File.tellg();
    if (FileSize % sizeof(uint32_t) != 0) return false;

    OutBytecode.resize(FileSize / sizeof(uint32_t));
    File.seekg(0);
    File.read(reinterpret_cast<char*>(OutBytecode.data()), FileSize);
    return true;
}