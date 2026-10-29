/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Util/TextureCompression.hpp>

#include <Core/Memory/Memory.hpp>
#include <Core/Math/MathUtil.hpp>

#include <Rendering/Texture.hpp>

#define BCDEC_IMPLEMENTATION
#include <bcdec.h>

#ifdef HYP_BC7ENC
#include <bc7enc.h>
#include <rgbcx.h>
#endif

namespace Hyperion {

namespace TextureCompression {

static void DecodeBlock(TextureFormat format, const ubyte* block, ubyte (&outRgba)[4 * 4 * 4])
{
    switch (format)
    {
    case TextureFormat::BC1:
    case TextureFormat::BC1_SRGB:
        bcdec_bc1(block, outRgba, 4 * 4);
        break;
    case TextureFormat::BC7:
    case TextureFormat::BC7_SRGB:
        bcdec_bc7(block, outRgba, 4 * 4);
        break;
    case TextureFormat::BC4:
    {
        ubyte r[4 * 4];
        bcdec_bc4(block, r, 4);

        for (uint32 i = 0; i < 16; i++)
        {
            outRgba[i * 4 + 0] = r[i];
            outRgba[i * 4 + 1] = 0;
            outRgba[i * 4 + 2] = 0;
            outRgba[i * 4 + 3] = 255;
        }

        break;
    }
    case TextureFormat::BC5:
    {
        ubyte rg[4 * 4 * 2];
        bcdec_bc5(block, rg, 4 * 2);

        for (uint32 i = 0; i < 16; i++)
        {
            outRgba[i * 4 + 0] = rg[i * 2 + 0];
            outRgba[i * 4 + 1] = rg[i * 2 + 1];
            outRgba[i * 4 + 2] = 0;
            outRgba[i * 4 + 3] = 255;
        }

        break;
    }
    default:
        Memory::Zero(outRgba, sizeof(outRgba));
        break;
    }
}

size_t GetMipDataOffset(const TextureDesc& desc, uint8 mipIndex)
{
    size_t offset = 0;

    for (uint8 i = 0; i < mipIndex; i++)
    {
        offset += desc.GetMipByteSize(i, /* includeArrayLayers */ true);
    }

    return offset;
}

bool DecodeMip(const TextureDesc& desc, ConstByteView imageData, uint8 mipIndex, ByteBuffer& outRgba8)
{
    if (!TextureUtils::IsBlockCompressed(desc.format) || mipIndex >= desc.NumMips())
    {
        return false;
    }

    const Vec3u mipExtent = desc.GetMipExtent(mipIndex);
    const uint32 numSlices = mipExtent.z * desc.NumArrayLayers();
    const uint32 blocksX = (mipExtent.x + 3) / 4;
    const uint32 blocksY = (mipExtent.y + 3) / 4;
    const uint32 bytesPerBlock = TextureUtils::GetBytesPerBlock(desc.format);

    const size_t srcOffset = GetMipDataOffset(desc, mipIndex);
    const size_t srcSize = desc.GetMipByteSize(mipIndex, /* includeArrayLayers */ true);

    if (imageData.Data() == nullptr || srcOffset + srcSize > imageData.Size())
    {
        return false;
    }

    outRgba8.SetSize(size_t(mipExtent.x) * size_t(mipExtent.y) * numSlices * 4);

    const ubyte* src = imageData.Data() + srcOffset;
    ubyte* dst = outRgba8.Data();

    ubyte decoded[4 * 4 * 4];

    for (uint32 slice = 0; slice < numSlices; slice++)
    {
        ubyte* dstSlice = dst + size_t(slice) * mipExtent.x * mipExtent.y * 4;

        for (uint32 by = 0; by < blocksY; by++)
        {
            for (uint32 bx = 0; bx < blocksX; bx++)
            {
                DecodeBlock(desc.format, src, decoded);
                src += bytesPerBlock;

                const uint32 copyWidth = MathUtil::Min(4u, mipExtent.x - bx * 4);
                const uint32 copyHeight = MathUtil::Min(4u, mipExtent.y - by * 4);

                for (uint32 row = 0; row < copyHeight; row++)
                {
                    Memory::Copy(
                        dstSlice + (size_t(by * 4 + row) * mipExtent.x + bx * 4) * 4,
                        decoded + row * 4 * 4,
                        copyWidth * 4);
                }
            }
        }
    }

    return true;
}

bool DecodeTexel(const TextureDesc& desc, ConstByteView imageData, Vec3u coord, uint32 slice, ubyte (&outRgba)[4])
{
    if (!TextureUtils::IsBlockCompressed(desc.format) || coord.x >= desc.extent.x || coord.y >= desc.extent.y)
    {
        return false;
    }

    const uint32 blocksX = (desc.extent.x + 3) / 4;
    const uint32 blocksY = (desc.extent.y + 3) / 4;
    const uint32 bytesPerBlock = TextureUtils::GetBytesPerBlock(desc.format);

    const size_t blockIndex = (size_t(slice) * blocksY + coord.y / 4) * blocksX + coord.x / 4;
    const size_t blockOffset = blockIndex * bytesPerBlock;

    if (imageData.Data() == nullptr || blockOffset + bytesPerBlock > imageData.Size())
    {
        return false;
    }

    ubyte decoded[4 * 4 * 4];
    DecodeBlock(desc.format, imageData.Data() + blockOffset, decoded);

    const uint32 texelIndex = (coord.y % 4) * 4 + (coord.x % 4);
    Memory::Copy(outRgba, decoded + texelIndex * 4, 4);

    return true;
}

bool Decode(const TextureDesc& desc, ConstByteView imageData, TextureDesc& outDesc, ByteBuffer& outImageData)
{
    if (!TextureUtils::IsBlockCompressed(desc.format))
    {
        return false;
    }

    const uint8 numMips = desc.HasStoredMips() ? desc.NumMips() : 1;

    outDesc = desc;
    outDesc.format = TextureUtils::IsSRGB(desc.format) ? TextureFormat::RGBA8_SRGB : TextureFormat::RGBA8;

    for (uint32 i = 0; i < TextureDesc::MaxMips; i++)
    {
        outDesc.mipOffsets[i] = 0;
    }

    size_t totalSize = 0;

    for (uint8 mipIndex = 0; mipIndex < numMips; mipIndex++)
    {
        totalSize += outDesc.GetMipByteSize(mipIndex, /* includeArrayLayers */ true);
    }

    outImageData.SetSize(totalSize);

    size_t dstOffset = 0;
    ByteBuffer mipData;

    for (uint8 mipIndex = 0; mipIndex < numMips; mipIndex++)
    {
        if (!DecodeMip(desc, imageData, mipIndex, mipData))
        {
            return false;
        }

        if (mipIndex != 0)
        {
            outDesc.mipOffsets[mipIndex - 1] = uint32(dstOffset);
        }

        Memory::Copy(outImageData.Data() + dstOffset, mipData.Data(), mipData.Size());
        dstOffset += mipData.Size();
    }

    return true;
}

static bool HasOpaqueAlpha(const TextureDesc& desc, ConstByteView imageData)
{
    if (TextureUtils::NumComponents(desc.format) != 4)
    {
        return true;
    }

    const size_t baseSize = desc.GetByteSize();

    if (imageData.Size() < baseSize)
    {
        return false;
    }

    for (size_t i = 3; i < baseSize; i += 4)
    {
        if (imageData.Data()[i] != 255)
        {
            return false;
        }
    }

    return true;
}

TextureFormat ChooseFormat(TextureCompressionMode mode, const TextureDesc& desc, ConstByteView imageData)
{
    const TextureFormat linearFormat = TextureUtils::ChangeFormatSRGB(desc.format, false);

    const bool isEightBit = linearFormat == TextureFormat::R8
        || linearFormat == TextureFormat::RG8
        || linearFormat == TextureFormat::RGB8
        || linearFormat == TextureFormat::RGBA8;

    if (mode == TextureCompressionMode::None
        || !isEightBit
        || (desc.type != TextureType::Texture2D && desc.type != TextureType::Texture2DArray)
        || desc.extent.x % 4 != 0
        || desc.extent.y % 4 != 0)
    {
        return InvalidTextureFormat;
    }

    switch (mode)
    {
    case TextureCompressionMode::Color:
        return TextureFormat::BC1;
    case TextureCompressionMode::ColorAlpha:
        return TextureFormat::BC7;
    case TextureCompressionMode::Normal:
        return TextureFormat::BC5;
    case TextureCompressionMode::Mask:
        return TextureFormat::BC4;
    default:
        break;
    }

    // BC4 and BC5 have no sRGB variants, so sRGB images of 1 or 2 channels go to BC7 like the rest
    if (!desc.IsSrgb())
    {
        switch (TextureUtils::NumComponents(desc.format))
        {
        case 1:
            return TextureFormat::BC4;
        case 2:
            return TextureFormat::BC5;
        default:
            break;
        }
    }

    if (desc.IsSrgb() && HasOpaqueAlpha(desc, imageData))
    {
        return TextureFormat::BC1;
    }

    return TextureFormat::BC7;
}

bool CanEncode()
{
#ifdef HYP_BC7ENC
    return true;
#else
    return false;
#endif
}

#ifdef HYP_BC7ENC

static void InitEncoders()
{
    static const bool s_initialized = []()
    {
        rgbcx::init();
        bc7enc_compress_block_init();

        return true;
    }();

    (void)s_initialized;
}

static void EncodeBlock(TextureFormat format, const ubyte (&rgba)[4 * 4 * 4], const bc7enc_compress_block_params& bc7Params, ubyte* outBlock)
{
    switch (format)
    {
    case TextureFormat::BC1:
    case TextureFormat::BC1_SRGB:
        rgbcx::encode_bc1(10, outBlock, rgba, /* allow_3color */ false, /* use_transparent_texels_for_black */ false);
        break;
    case TextureFormat::BC4:
        rgbcx::encode_bc4(outBlock, rgba, 4);
        break;
    case TextureFormat::BC5:
        rgbcx::encode_bc5(outBlock, rgba, 0, 1, 4);
        break;
    case TextureFormat::BC7:
    case TextureFormat::BC7_SRGB:
        bc7enc_compress_block(outBlock, rgba, &bc7Params);
        break;
    default:
        break;
    }
}

#endif // HYP_BC7ENC

bool Encode(const TextureDesc& desc, ConstByteView imageData, TextureFormat targetFormat, TextureDesc& outDesc, ByteBuffer& outImageData)
{
#ifdef HYP_BC7ENC
    const TextureFormat linearSourceFormat = TextureUtils::ChangeFormatSRGB(desc.format, false);

    const bool isEightBitSource = linearSourceFormat == TextureFormat::R8
        || linearSourceFormat == TextureFormat::RG8
        || linearSourceFormat == TextureFormat::RGB8
        || linearSourceFormat == TextureFormat::RGBA8;

    if (!isEightBitSource || !TextureUtils::IsBlockCompressed(targetFormat) || desc.extent.x % 4 != 0 || desc.extent.y % 4 != 0)
    {
        return false;
    }

    InitEncoders();

    TextureDesc sourceDesc = desc;
    ByteBuffer generatedMips;

    if (sourceDesc.HasMipMaps() && !sourceDesc.HasStoredMips())
    {
        const size_t baseSize = sourceDesc.GetByteSize();

        if (imageData.Size() < baseSize)
        {
            return false;
        }

        generatedMips = ByteBuffer(imageData.Slice(0, baseSize));
        Texture::GenerateMipmaps(sourceDesc, generatedMips);
        imageData = generatedMips.ToByteView();
    }

    if (imageData.Size() < sourceDesc.GetByteSize(/* includeAllMips */ sourceDesc.HasStoredMips()))
    {
        return false;
    }

    const uint32 numComponents = TextureUtils::NumComponents(sourceDesc.format);
    const uint8 numMips = sourceDesc.HasStoredMips() ? sourceDesc.NumMips() : 1;

    outDesc = sourceDesc;
    outDesc.format = TextureUtils::ChangeFormatSRGB(targetFormat, sourceDesc.IsSrgb());

    for (uint32 i = 0; i < TextureDesc::MaxMips; i++)
    {
        outDesc.mipOffsets[i] = 0;
    }

    size_t totalSize = 0;

    for (uint8 mipIndex = 0; mipIndex < numMips; mipIndex++)
    {
        if (mipIndex != 0)
        {
            outDesc.mipOffsets[mipIndex - 1] = uint32(totalSize);
        }

        totalSize += outDesc.GetMipByteSize(mipIndex, /* includeArrayLayers */ true);
    }

    outImageData.SetSize(totalSize);

    bc7enc_compress_block_params bc7Params;
    bc7enc_compress_block_params_init(&bc7Params);

    if (!sourceDesc.IsSrgb())
    {
        bc7enc_compress_block_params_init_linear_weights(&bc7Params);
    }

    const uint32 bytesPerBlock = TextureUtils::GetBytesPerBlock(outDesc.format);

    ubyte* dst = outImageData.Data();

    for (uint8 mipIndex = 0; mipIndex < numMips; mipIndex++)
    {
        const Vec3u mipExtent = sourceDesc.GetMipExtent(mipIndex);
        const uint32 numSlices = mipExtent.z * sourceDesc.NumArrayLayers();
        const uint32 blocksX = (mipExtent.x + 3) / 4;
        const uint32 blocksY = (mipExtent.y + 3) / 4;

        const ubyte* srcMip = imageData.Data() + GetMipDataOffset(sourceDesc, mipIndex);
        const size_t srcSliceSize = size_t(mipExtent.x) * mipExtent.y * numComponents;

        for (uint32 slice = 0; slice < numSlices; slice++)
        {
            const ubyte* srcSlice = srcMip + slice * srcSliceSize;

            for (uint32 by = 0; by < blocksY; by++)
            {
                for (uint32 bx = 0; bx < blocksX; bx++)
                {
                    ubyte rgba[4 * 4 * 4];

                    for (uint32 row = 0; row < 4; row++)
                    {
                        // blocks past the edge of a small mip repeat its last row / column
                        const uint32 y = MathUtil::Min(by * 4 + row, mipExtent.y - 1);

                        for (uint32 col = 0; col < 4; col++)
                        {
                            const uint32 x = MathUtil::Min(bx * 4 + col, mipExtent.x - 1);

                            const ubyte* texel = srcSlice + (size_t(y) * mipExtent.x + x) * numComponents;
                            ubyte* out = rgba + (row * 4 + col) * 4;

                            out[0] = texel[0];
                            out[1] = numComponents > 1 ? texel[1] : 0;
                            out[2] = numComponents > 2 ? texel[2] : 0;
                            out[3] = numComponents > 3 ? texel[3] : 255;
                        }
                    }

                    EncodeBlock(outDesc.format, rgba, bc7Params, dst);
                    dst += bytesPerBlock;
                }
            }
        }
    }

    return true;
#else
    (void)desc;
    (void)imageData;
    (void)targetFormat;
    (void)outDesc;
    (void)outImageData;

    return false;
#endif
}

} // namespace TextureCompression

} // namespace Hyperion
