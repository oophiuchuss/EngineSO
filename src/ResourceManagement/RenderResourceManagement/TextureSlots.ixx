module;

export module TextureSlots;

export namespace TextureSlots
{
    // Reserved descriptor array indices (must match DescriptorHeap creation order)
    constexpr int DefaultWhite = 0;     // fallback albedo, metallic/roughness
    constexpr int DefaultNormal = 1;    // flat normal (0.5, 0.5, 1.0)
    constexpr int DefaultBlack = 2;     // fallback occlusion, emissive
}

export namespace CubemapSlots
{
	// Reserved descriptor array indices (must match DescriptorHeap creation order)
    constexpr int DefaultBlack = 0;
}