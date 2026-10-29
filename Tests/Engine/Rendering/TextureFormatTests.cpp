/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#ifdef HYP_TESTS

#include <Framework/Testing/EngineTest.hpp>

#include <Rendering/Shared.hpp>

namespace Hyperion {
namespace tests {
namespace rendering {

namespace {

TextureDesc MakeMippedDesc(TextureFormat format, Vec3u extent)
{
    return TextureDesc {
        TextureType::Texture2D,
        format,
        extent,
        TextureFilterMode::LinearMipmap,
        TextureFilterMode::Linear,
        TextureWrapMode::Repeat
    };
}

} // namespace

HYP_ENGINE_TEST(TextureFormat, BlockCompressedClassification)
{
    Check("BC7 is block compressed", TextureUtils::IsBlockCompressed(TextureFormat::BC7));
    Check("RGBA8 is not block compressed", !TextureUtils::IsBlockCompressed(TextureFormat::RGBA8));
    Check("BC formats are not depth", !TextureUtils::IsDepthFormat(TextureFormat::BC1) && !TextureUtils::IsDepthFormat(TextureFormat::BC7_SRGB));
    Check("D32F_S8 is still depth", TextureUtils::IsDepthFormat(TextureFormat::D32F_S8));
    Check("BC1_SRGB and BC7_SRGB are sRGB", TextureUtils::IsSRGB(TextureFormat::BC1_SRGB) && TextureUtils::IsSRGB(TextureFormat::BC7_SRGB));
    Check("BC4, BC5, BC7 are linear", !TextureUtils::IsSRGB(TextureFormat::BC4) && !TextureUtils::IsSRGB(TextureFormat::BC5) && !TextureUtils::IsSRGB(TextureFormat::BC7));
    Check("BC7 <-> BC7_SRGB", TextureUtils::ChangeFormatSRGB(TextureFormat::BC7, true) == TextureFormat::BC7_SRGB && TextureUtils::ChangeFormatSRGB(TextureFormat::BC7_SRGB, false) == TextureFormat::BC7);
    Check("BC1 <-> BC1_SRGB", TextureUtils::ChangeFormatSRGB(TextureFormat::BC1, true) == TextureFormat::BC1_SRGB && TextureUtils::ChangeFormatSRGB(TextureFormat::BC1_SRGB, false) == TextureFormat::BC1);
    Check("BC5 has no sRGB variant", TextureUtils::ChangeFormatSRGB(TextureFormat::BC5, true) == TextureFormat::BC5);
    Check("RGBA8 -> RGBA8_SRGB unchanged", TextureUtils::ChangeFormatSRGB(TextureFormat::RGBA8, true) == TextureFormat::RGBA8_SRGB);
}

HYP_ENGINE_TEST(TextureFormat, BlockSizes)
{
    Check("BC1 row of 1024 texels", TextureUtils::GetRowByteSize(TextureFormat::BC1, 1024) == 256 * 8);
    Check("BC7 row of 1024 texels", TextureUtils::GetRowByteSize(TextureFormat::BC7, 1024) == 256 * 16);
    Check("BC7 row of 1000 texels rounds up", TextureUtils::GetRowByteSize(TextureFormat::BC7, 1000) == 250 * 16);
    Check("BC7 row of 2 texels is one block", TextureUtils::GetRowByteSize(TextureFormat::BC7, 2) == 16);
    Check("BC7 rows of 600 texels", TextureUtils::GetNumRows(TextureFormat::BC7, 600) == 150);
    Check("BC7 rows of 1 texel", TextureUtils::GetNumRows(TextureFormat::BC7, 1) == 1);
    Check("RGBA8 row is per texel", TextureUtils::GetRowByteSize(TextureFormat::RGBA8, 1000) == 4000);
    Check("RGBA8 rows are per texel", TextureUtils::GetNumRows(TextureFormat::RGBA8, 600) == 600);

    Check("BC7 1024x1024 image", TextureUtils::GetImageByteSize(TextureFormat::BC7, Vec3u { 1024, 1024, 1 }) == 1024 * 1024);
    Check("BC1 1024x1024 image", TextureUtils::GetImageByteSize(TextureFormat::BC1, Vec3u { 1024, 1024, 1 }) == 1024 * 1024 / 2);
    Check("BC7 1000x600 image", TextureUtils::GetImageByteSize(TextureFormat::BC7, Vec3u { 1000, 600, 1 }) == size_t(250) * 150 * 16);
    Check("BC4 2x2 image is one block", TextureUtils::GetImageByteSize(TextureFormat::BC4, Vec3u { 2, 2, 1 }) == 8);
    Check("RGBA8 1000x600 image", TextureUtils::GetImageByteSize(TextureFormat::RGBA8, Vec3u { 1000, 600, 1 }) == size_t(1000) * 600 * 4);
}

HYP_ENGINE_TEST(TextureFormat, BlockCompressedMipChain)
{
    const TextureDesc desc = MakeMippedDesc(TextureFormat::BC7_SRGB, Vec3u { 1024, 1024, 1 });

    Check("1024 BC7 has 11 mips", desc.NumMips() == 11);
    Check("mip 0 is 1 MiB", desc.GetMipByteSize(0) == 1024 * 1024);
    Check("mip 1 is 256 KiB", desc.GetMipByteSize(1) == 512 * 512);
    Check("4x4 mip is one block", desc.GetMipByteSize(8) == 16);
    Check("2x2 mip is one block", desc.GetMipByteSize(9) == 16);
    Check("1x1 mip is one block", desc.GetMipByteSize(10) == 16);

    size_t expectedTotal = 0;

    for (uint32 size = 1024; size >= 4; size >>= 1)
    {
        expectedTotal += size_t(size) * size;
    }

    expectedTotal += 16 * 2;

    Check("full BC7 chain size", desc.GetByteSize(/* includeAllMips */ true) == expectedTotal);

    const TextureDesc rgbaDesc = MakeMippedDesc(TextureFormat::RGBA8, Vec3u { 1024, 1024, 1 });

    Check("RGBA8 chain unchanged", rgbaDesc.GetByteSize(/* includeAllMips */ true) == 5592404);
}

} // namespace rendering
} // namespace tests
} // namespace Hyperion

#endif // HYP_TESTS
