module;

#include <cstdint>

export module PushConstants;

// All actual data (transform, material) lives in ObjectData/MaterialData SSBOs
export struct PushConstantData
{
    uint32_t ObjectIndex = 0;
};

export struct NormalMipPushConstants
{
	uint32_t SourceWidth;
	uint32_t SourceHeight;
	uint32_t DestinationWidth;
	uint32_t DestinationHeight;
	uint32_t AddressModeU;
	uint32_t AddressModeV;
};