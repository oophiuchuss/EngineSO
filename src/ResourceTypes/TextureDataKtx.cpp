module;

#include <ktx.h>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>
#include <cstddef>
#include <numeric>

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

	struct KtxUploadPayload
	{
		TextureInfo Info;
		std::vector<uint8_t> Bytes;
		std::vector<TextureSubresource> Subresources;
	};

	ktx_transcode_fmt_e ToKtxTranscodeFormat(BasisTranscodeTarget Target)
	{
		switch (Target)
		{
		case BasisTranscodeTarget::BC7RGBA:
			return KTX_TTF_BC7_RGBA;

		case BasisTranscodeTarget::ASTC4x4RGBA:
			return KTX_TTF_ASTC_4x4_RGBA;

		case BasisTranscodeTarget::ETC2RGBA:
			return KTX_TTF_ETC2_RGBA;

		case BasisTranscodeTarget::BC3RGBA:
			return KTX_TTF_BC3_RGBA;

		case BasisTranscodeTarget::RGBA32:
			return KTX_TTF_RGBA32;

		default:
			return KTX_TTF_NOSELECTION;
		}
	}

	bool ReadKtxInfo(ktxTexture2* Texture, const TextureInfo& BaseInfo, TextureInfo& OutInfo)
	{
		OutInfo = BaseInfo;

		OutInfo.Width = Texture->baseWidth;
		OutInfo.Height = Texture->baseHeight;
		OutInfo.Depth = 1;

		OutInfo.MipLevels = Texture->numLevels;
		OutInfo.LayerCount = Texture->numLayers;
		OutInfo.FaceCount = Texture->numFaces;

		OutInfo.MipMode = TextureMipMode::Provided;
		OutInfo.Format = static_cast<vk::Format>(Texture->vkFormat);

		const khr_df_transfer_e TransferFunction = ktxTexture2_GetTransferFunction_e(Texture);

		if (TransferFunction == KHR_DF_TRANSFER_SRGB)
		{
			OutInfo.ColorSpace = TextureColorSpace::SRGB;
		}
		else if (TransferFunction == KHR_DF_TRANSFER_LINEAR)
		{
			OutInfo.ColorSpace = TextureColorSpace::Linear;
		}
		else
		{
			return RejectKtx("only linear and sRGB transfer functions are supported");
		}

		return true;
	}

	bool BuildKtxUploadPayload(ktxTexture2* Texture, const TextureInfo& NewInfo, KtxUploadPayload& OutPayload)
	{
		ktxTexture* BaseTexture = ktxTexture(Texture);

		ktx_uint8_t* SourceData = ktxTexture_GetData(BaseTexture);

		const ktx_size_t SourceDataSize = ktxTexture_GetDataSize(BaseTexture);

		if (!SourceData || SourceDataSize == 0)
		{
			return RejectKtx("texture contains no loaded image data");
		}

		const ktx_uint32_t ElementSize = ktxTexture_GetElementSize(BaseTexture);

		if (ElementSize == 0)
		{
			return RejectKtx("texture has an invalid element size");
		}

		const bool bBlockCompressed = Texture->isCompressed == KTX_TRUE;

		const std::size_t SubresourceAlignment = std::lcm<std::size_t>(4u, ElementSize);

		std::vector<uint8_t> NewBytes;

		if (SourceDataSize > NewBytes.max_size())
		{
			return RejectKtx("texture data is too large");
		}

		NewBytes.reserve(static_cast<std::size_t>(SourceDataSize));

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

			std::size_t TightRowSize = 0;
			ktx_uint32_t SourceRowPitch = 0;

			if (!bBlockCompressed)
			{
				if (MipWidth > NewBytes.max_size() / ElementSize)
				{
					return RejectKtx("texture row size overflow");
				}

				TightRowSize = static_cast<std::size_t>(MipWidth) * ElementSize;

				SourceRowPitch = ktxTexture_GetRowPitch(BaseTexture, MipLevel);

				if (SourceRowPitch < TightRowSize)
				{
					return RejectKtx("source row pitch is smaller than one tight row");
				}
			}


			for (uint32_t Layer = 0; Layer < Texture->numLayers; Layer++)
			{
				for (uint32_t Face = 0; Face < Texture->numFaces; Face++)
				{
					ktx_size_t ImageOffset = 0;

					const KTX_error_code OffsetResult = ktxTexture_GetImageOffset(
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

					const std::size_t AlignmentRemainder = NewBytes.size() % SubresourceAlignment;

					const std::size_t AlignmentPadding = AlignmentRemainder == 0 ? 0 : SubresourceAlignment - AlignmentRemainder;

					if (AlignmentPadding > NewBytes.max_size() - NewBytes.size())
					{
						return RejectKtx("subresource alignment overflow");
					}

					NewBytes.resize(NewBytes.size() + AlignmentPadding, 0);

					const std::size_t DestinationOffset = NewBytes.size();

					std::size_t DestinationSize = 0;

					if (bBlockCompressed)
					{
						if (ImageSize > NewBytes.max_size() - NewBytes.size())
						{
							return RejectKtx("compressed subresource is too large");
						}

						DestinationSize = static_cast<std::size_t>(ImageSize);

						const uint8_t* SourceImage = SourceData + static_cast<std::size_t>(ImageOffset);

						NewBytes.insert(NewBytes.end(), SourceImage, SourceImage + DestinationSize);
					}
					else
					{
						if (TightRowSize != 0 && MipHeight > NewBytes.max_size() / TightRowSize)
						{
							return RejectKtx("uncompressed subresource size overflow");
						}

						DestinationSize = TightRowSize * static_cast<std::size_t>(MipHeight);

						const uint64_t RequiredSourceSize =
							static_cast<uint64_t>(SourceRowPitch) *
							static_cast<uint64_t>(MipHeight - 1u) +
							static_cast<uint64_t>(TightRowSize);

						if (RequiredSourceSize > ImageSize)
						{
							return RejectKtx("source image is smaller than its row layout");
						}

						NewBytes.resize(DestinationOffset + DestinationSize);

						const uint8_t* SourceImage = SourceData + static_cast<std::size_t>(ImageOffset);

						uint8_t* DestinationImage = NewBytes.data() + DestinationOffset;

						for (uint32_t Row = 0; Row < MipHeight; ++Row)
						{
							const uint8_t* SourceRow = SourceImage + static_cast<std::size_t>(Row) * SourceRowPitch;

							uint8_t* DestinationRow = DestinationImage + static_cast<std::size_t>(Row) * TightRowSize;

							std::copy_n(SourceRow, TightRowSize, DestinationRow);
						}
					}

					TextureSubresource Subresource;

					Subresource.MipLevel = MipLevel;
					Subresource.Layer = Layer;
					Subresource.Face = Face;

					Subresource.Width = MipWidth;
					Subresource.Height = MipHeight;
					Subresource.Depth = 1;

					Subresource.ByteOffset = static_cast<uint64_t>(DestinationOffset);

					Subresource.ByteSize = static_cast<uint64_t>(DestinationSize);

					NewSubresources.push_back(Subresource);
				}
			}
		}

		if (NewSubresources.size() != SubresourceCount)
		{
			return RejectKtx("texture produced an incomplete subresource set");
		}

		OutPayload.Info = NewInfo;
		OutPayload.Bytes = std::move(NewBytes);
		OutPayload.Subresources = std::move(NewSubresources);

		return true;
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

	TextureInfo NewInfo;

	if (!ReadKtxInfo(Texture.get(), Info, NewInfo))
	{
		return false;
	}

	const bool bNeedsBasisTranscoding = ktxTexture2_NeedsTranscoding(Texture.get()) == KTX_TRUE;

	if (bNeedsBasisTranscoding)
	{
		NewInfo.Format = vk::Format::eUndefined;

		std::vector<uint8_t> NewEncodedSource(Data, Data + DataSize);

		Info = NewInfo;

		EncodedSource = std::move(NewEncodedSource);
		Bytes.clear();
		Subresources.clear();

		SourceEncoding = TextureSourceEncoding::BasisUniversal;

		PreparedBasisTarget.reset();

		return true;
	}

	if (Texture->supercompressionScheme != KTX_SS_NONE)
	{
		return RejectKtx("supercompression and Basis transcoding are not enabled yet");
	}

	if (Texture->vkFormat == static_cast<uint32_t>(vk::Format::eUndefined))
	{
		return RejectKtx("texture has no directly uploadable Vulkan format");
	}

	NewInfo.Format = static_cast<vk::Format>(Texture->vkFormat);
	
	KtxUploadPayload Payload;

	if (!BuildKtxUploadPayload(Texture.get(), NewInfo, Payload))
	{
		return false;
	}

	Info = std::move(Payload.Info);
	Bytes = std::move(Payload.Bytes);
	Subresources = std::move(Payload.Subresources);

	EncodedSource.clear();
	SourceEncoding = TextureSourceEncoding::Direct;
	PreparedBasisTarget.reset();

	return true;
}


bool TextureData::PrepareBasisPayload(BasisTranscodeTarget Target)
{
	if (SourceEncoding != TextureSourceEncoding::BasisUniversal)
	{
		return IsUploadReady();
	}

	if (IsPreparedFor(Target))
	{
		return true;
	}

	if (EncodedSource.empty())
	{
		return RejectKtx("Basis source payload is missing");
	}

	const ktx_transcode_fmt_e KtxTarget = ToKtxTranscodeFormat(Target);

	if (KtxTarget == KTX_TTF_NOSELECTION)
	{
		return RejectKtx("invalid Basis transcode target");
	}

	ktxTexture2* RawTexture = nullptr;

	const KTX_error_code CreateResult = ktxTexture2_CreateFromMemory(
		reinterpret_cast<const ktx_uint8_t*>(EncodedSource.data()),
		static_cast<ktx_size_t>(EncodedSource.size()),
		KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
		&RawTexture);

	if (CreateResult != KTX_SUCCESS)
	{
		std::cerr << "[TextureData] Failed to reopen Basis KTX2: " << ktxErrorString(CreateResult) << '\n';
		return false;
	}

	UniqueKtxTexture Texture(RawTexture);

	if (ktxTexture2_NeedsTranscoding(Texture.get()) != KTX_TRUE)
	{
		return RejectKtx("stored Basis source no longer requires transcoding");
	}

	const KTX_error_code TranscodeResult = ktxTexture2_TranscodeBasis(Texture.get(), KtxTarget, 0);

	if (TranscodeResult != KTX_SUCCESS)
	{
		std::cerr << "[TextureData] Basis transcode failed: " << ktxErrorString(TranscodeResult) << '\n';
		return false;
	}

	if (ktxTexture2_NeedsTranscoding(Texture.get()) == KTX_TRUE)
	{
		return RejectKtx("Basis texture still requires transcoding");
	}

	if (Texture->vkFormat == static_cast<uint32_t>(vk::Format::eUndefined))
	{
		return RejectKtx("transcode produced no Vulkan format");
	}

	TextureInfo NewInfo;

	if (!ReadKtxInfo(Texture.get(), Info, NewInfo))
	{
		return false;
	}

	KtxUploadPayload Payload;

	if (!BuildKtxUploadPayload(Texture.get(), NewInfo, Payload))
	{
		return false;
	}

	Info = std::move(Payload.Info);
	Bytes = std::move(Payload.Bytes);
	Subresources = std::move(Payload.Subresources);

	PreparedBasisTarget = Target;

	// Keep EncodedSource and BasisUniversal source encoding.
	return true;
}