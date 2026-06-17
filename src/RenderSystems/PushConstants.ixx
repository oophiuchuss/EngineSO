module;

#include <cstdint>

export module PushConstants;

// All actual data (transform, material) lives in ObjectData/MaterialData SSBOs
export struct PushConstantData
{
    uint32_t ObjectIndex = 0;
};