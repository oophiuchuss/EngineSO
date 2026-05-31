module;

#include <string>
#include <vector>

export module ShaderData;

import ResourceBase;

export struct ShaderData : public ResourceBase
{
    explicit ShaderData(const std::string& ID) : ResourceBase(ID) {}

    // SPIR-V bytecode loaded from <ID>.vert.spv and <ID>.frag.spv
    std::vector<uint32_t> VertexBytecode;
    std::vector<uint32_t> FragmentBytecode;

protected:
    bool LoadResource(const std::string& BasePath) override;
    void UnloadResource() override;

private:
    static bool ReadSpirV(const std::string& FilePath, std::vector<uint32_t>& OutBytecode);
};