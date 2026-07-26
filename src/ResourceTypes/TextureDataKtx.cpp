module;

#include <ktx.h>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>

module TextureData;

namespace
{
	struct KtxTextureDeleter
	{
		void operator()(ktxTexture2* Texture) const noexcept
		{
			if (Texture)
			{
				ktxTexture_Destroy(ktxTexture(Texture));
			}
		}
	};

	using UniqueKtxTexture = std::unique_ptr<ktxTexture2, KtxTextureDeleter>;

	bool RejectKtx(const char* Message)
	{
		std::cerr << "[TextureData] Unsupported KTX2 texture: " << Message << '\n';

		return false;
	}
}

bool TextureData::DecodeKtx2(const uint8_t* Data, std::size_t DataSize)
{
	ktxTexture2* RawTexture = nullptr;

	const KTX_error_code CreateResult = ktxTexture2_CreateFromMemory(
		reinterpret_cast<const ktx_uint8_t*>(Data),
		static_cast<ktx_size_t>(DataSize),
		KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
		&RawTexture);

	if (CreateResult != KTX_SUCCESS)
	{
		std::cerr << "[TextureData] Failed to decode KTX2: " << ktxErrorString(CreateResult) << '\n';

		return false;
	}

	UniqueKtxTexture Texture(RawTexture);
	ktxTexture* BaseTexture = ktxTexture(Texture.get());

	if (Texture->numDimensions != 2 || Texture->baseDepth != 1)
	{
		return RejectKtx("only 2D textures are currently supported");
	}

	if (Texture->numFaces != 1 && Texture->numFaces != 6)
	{
		return RejectKtx("face count must be either one or six");
	}

	if (Texture->numFaces == 6 && Texture->baseWidth != Texture->baseHeight)
	{
		return RejectKtx("cubemap faces must be square");
	}

	if ((Texture->numFaces == 6) != (Texture->isCubemap == KTX_TRUE))
	{
		return RejectKtx("cubemap flag and face count disagree");
	}

	if (!Texture->isArray && Texture->numLayers != 1)
	{
		return RejectKtx("non-array texture contains multiple layers");
	}

	if (Texture->numLevels == 0 || Texture->numLayers == 0)
	{
		return RejectKtx("texture contains no mip levels or layers");
	}

	if (Texture->generateMipmaps == KTX_TRUE)
	{
		return RejectKtx("runtime mip generation requests are unsupported");
	}

	if (Texture->isVideo == KTX_TRUE)
	{
		return RejectKtx("video textures are unsupported");
	}

	if (Texture->supercompressionScheme != KTX_SS_NONE)
	{
		return RejectKtx("supercompression and Basis transcoding are not enabled yet");
	}

	if (Texture->vkFormat == static_cast<uint32_t>(vk::Format::eUndefined))
	{
		return RejectKtx("texture has no directly uploadable Vulkan format");
	}

	ktx_uint8_t* SourceData = ktxTexture_GetData(BaseTexture);

	const ktx_size_t SourceDataSize = ktxTexture_GetDataSize(BaseTexture);

	if (!SourceData || SourceDataSize == 0)
	{
		return RejectKtx("texture contains no loaded image data");
	}

	TextureInfo NewInfo = Info;

	NewInfo.Width = Texture->baseWidth;
	NewInfo.Height = Texture->baseHeight;
	NewInfo.Depth = 1;

	NewInfo.MipLevels = Texture->numLevels;
	NewInfo.LayerCount = Texture->numLayers;
	NewInfo.FaceCount = Texture->numFaces;

	NewInfo.Format = static_cast<vk::Format>(Texture->vkFormat);

	NewInfo.MipMode = TextureMipMode::Provided;

	std::vector<uint8_t> NewBytes(SourceData, SourceData + SourceDataSize);

	std::vector<TextureSubresource> NewSubresources;

	const size_t SubresourceCount =
		static_cast<size_t>(Texture->numLevels) *
		static_cast<size_t>(Texture->numLayers) *
		static_cast<size_t>(Texture->numFaces);

	NewSubresources.reserve(SubresourceCount);

	for (uint32_t MipLevel = 0; MipLevel < Texture->numLevels; MipLevel++)
	{
		const uint32_t MipWidth = (std::max)(Texture->baseWidth >> MipLevel, 1u);

		const uint32_t MipHeight = (std::max)(Texture->baseHeight >> MipLevel, 1u);

		const ktx_size_t ImageSize = ktxTexture_GetImageSize(BaseTexture, MipLevel);

		if (ImageSize == 0)
		{
			return RejectKtx("mip level has an invalid image size");
		}

		for (uint32_t Layer = 0; Layer < Texture->numLayers; Layer++)
		{
			for (uint32_t Face = 0; Face < Texture->numFaces; Face++)
			{
				ktx_size_t ImageOffset = 0;

				const KTX_error_code OffsetResult =
					ktxTexture_GetImageOffset(
						BaseTexture,
						MipLevel,
						Layer,
						Face,
						&ImageOffset);

				if (OffsetResult != KTX_SUCCESS)
				{
					std::cerr << "[TextureData] Failed to obtain KTX2 image offset: " << ktxErrorString(OffsetResult) << '\n';

					return false;
				}

				if (ImageOffset > SourceDataSize || ImageSize > SourceDataSize - ImageOffset)
				{
					return RejectKtx("subresource byte range is invalid");
				}

				TextureSubresource Subresource;

				Subresource.MipLevel = MipLevel;
				Subresource.Layer = Layer;
				Subresource.Face = Face;

				Subresource.Width = MipWidth;
				Subresource.Height = MipHeight;
				Subresource.Depth = 1;

				Subresource.ByteOffset = static_cast<uint64_t>(ImageOffset);

				Subresource.ByteSize = static_cast<uint64_t>(ImageSize);

				NewSubresources.push_back(Subresource);
			}
		}
	}

	if (NewSubresources.size() != SubresourceCount)
	{
		return RejectKtx("texture produced an incomplete subresource set");
	}

	Info = NewInfo;
	Bytes = std::move(NewBytes);
	Subresources = std::move(NewSubresources);

	return true;
}