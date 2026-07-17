module;

#include <string>
#include <vector>
#include <fstream>
#include <filesystem>

module ShaderData;

bool ShaderData::LoadResource(const std::string& BasePath)
{
	const bool bHasVertex = std::filesystem::exists(BasePath + ".vert.spv");
	const bool bHasFragment = std::filesystem::exists(BasePath + ".frag.spv");
	const bool bHasCompute = std::filesystem::exists(BasePath + ".comp.spv");

	const bool bHasAnyGraphicsFile = bHasVertex || bHasFragment;

	// Mixing compute and graphics files under the same base name
	// would make automatic detection ambiguous.
	if (bHasCompute && bHasAnyGraphicsFile)
	{
		return false;
	}

	if (bHasVertex && bHasFragment)
	{
		ProgramType = ShaderProgramType::Graphics;
		if (!ReadSpirV(BasePath + ".vert.spv", VertexBytecode))
		{
			return false;
		}
		
		if (!ReadSpirV(BasePath + ".frag.spv", FragmentBytecode)) 
		{
			return false;
		}
	}
	else if (bHasCompute)
	{
		ProgramType = ShaderProgramType::Compute;
		if (!ReadSpirV(BasePath + ".comp.spv", ComputeBytecode)) 
		{
			return false;
		}
	}
	else
	{
		return false;
	}

    return true;
}

void ShaderData::UnloadResource()
{
    VertexBytecode.clear();
    FragmentBytecode.clear();
    ComputeBytecode.clear();
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