/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Memory/ByteBuffer.hpp>

#include <Rendering/Shared.hpp>

namespace Hyperion {

enum class TextureCompressionMode : uint8;

namespace TextureCompression {

/*! \brief Byte offset of \p mipIndex in tightly packed image data (mip-major, every array layer of a mip together) */
ENGINE_API size_t GetMipDataOffset(const TextureDesc& desc, uint8 mipIndex);

/*! \brief Decodes every array layer of one mip of a block-compressed image to RGBA8
 *  BC4 decodes to (r, 0, 0, 255) and BC5 decodes to (r, g, 0, 255) */
ENGINE_API bool DecodeMip(const TextureDesc& desc, ConstByteView imageData, uint8 mipIndex, ByteBuffer& outRgba8);

/*! \brief Decodes the RGBA8 texel at \p coord of mip 0, \p slice (array layer, or z for volumes), by decoding only its block */
ENGINE_API bool DecodeTexel(const TextureDesc& desc, ConstByteView imageData, Vec3u coord, uint32 slice, ubyte (&outRgba)[4]);

/*! \brief Decodes a whole block-compressed image (all stored mips and layers) to RGBA8 / RGBA8_SRGB */
ENGINE_API bool Decode(const TextureDesc& desc, ConstByteView imageData, TextureDesc& outDesc, ByteBuffer& outImageData);

/*! \brief The block format corresponding to \p mode for an 8-bit 2D image, or InvalidTextureFormat otherwise
 *  Auto: 1 channel -> BC4, 2 channels -> BC5, sRGB with opaque (or no) alpha -> BC1, anything else -> BC7. */
ENGINE_API TextureFormat ChooseFormat(TextureCompressionMode mode, const TextureDesc& desc, ConstByteView imageData);

/*! \brief True when this build can encode block-compressed textures (editor builds) */
ENGINE_API bool CanEncode();

/*! \brief Encodes an 8-bit image (R8 / RG8 / RGB8 / RGBA8, sRGB or not) to format defined by \p targetFormat.
 *  mips will be generated first, if the image needs mips, and none are stored.
 *  sRGB sources produce the sRGB variant of BC1 / BC7. Returns false, indicating failure, when mip 0 isn't aligned to 4x4 blocks. */
ENGINE_API bool Encode(const TextureDesc& desc, ConstByteView imageData, TextureFormat targetFormat, TextureDesc& outDesc, ByteBuffer& outImageData);

} // namespace TextureCompression

} // namespace Hyperion
