/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include "MCPViewportCapture.hpp"

#include <Core/Math/MathUtil.hpp>

#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace Hyperion {
namespace MCP {

void DownscaleImage(CapturedImage& image, uint32 maxWidth)
{
    if (maxWidth == 0 || image.width <= maxWidth)
    {
        return;
    }

    const uint32 newWidth = maxWidth;
    const uint32 newHeight = MathUtil::Max(1u, uint32(uint64(image.height) * newWidth / image.width));

    Array<ubyte> scaled;
    scaled.Resize(size_t(newWidth) * newHeight * 3);

    for (uint32 y = 0; y < newHeight; y++)
    {
        const uint32 sourceY0 = uint32(uint64(y) * image.height / newHeight);
        const uint32 sourceY1 = MathUtil::Max(sourceY0 + 1, uint32(uint64(y + 1) * image.height / newHeight));

        for (uint32 x = 0; x < newWidth; x++)
        {
            const uint32 sourceX0 = uint32(uint64(x) * image.width / newWidth);
            const uint32 sourceX1 = MathUtil::Max(sourceX0 + 1, uint32(uint64(x + 1) * image.width / newWidth));

            uint32 sums[3] = { 0, 0, 0 };

            for (uint32 sourceY = sourceY0; sourceY < sourceY1; sourceY++)
            {
                const ubyte* sourcePixel = image.rgb.Data() + (size_t(sourceY) * image.width + sourceX0) * 3;

                for (uint32 sourceX = sourceX0; sourceX < sourceX1; sourceX++, sourcePixel += 3)
                {
                    sums[0] += sourcePixel[0];
                    sums[1] += sourcePixel[1];
                    sums[2] += sourcePixel[2];
                }
            }

            const uint32 count = (sourceX1 - sourceX0) * (sourceY1 - sourceY0);
            ubyte* destPixel = scaled.Data() + (size_t(y) * newWidth + x) * 3;

            for (uint32 channel = 0; channel < 3; channel++)
            {
                destPixel[channel] = ubyte(sums[channel] / count);
            }
        }
    }

    image.width = newWidth;
    image.height = newHeight;
    image.rgb = std::move(scaled);
}

bool EncodePng(const CapturedImage& image, Array<ubyte>& outPngBytes)
{
    outPngBytes.Clear();

    auto appendBytes = [](void* context, void* data, int size)
    {
        Array<ubyte>& bytes = *static_cast<Array<ubyte>*>(context);
        const ubyte* begin = static_cast<const ubyte*>(data);

        for (int i = 0; i < size; i++)
        {
            bytes.PushBack(begin[i]);
        }
    };

    return stbi_write_png_to_func(appendBytes, &outPngBytes, int(image.width), int(image.height), 3, image.rgb.Data(), int(image.width * 3)) != 0;
}

String EncodeBase64(const Array<ubyte>& bytes)
{
    static constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    String out;
    out.Reserve((bytes.Size() + 2) / 3 * 4);

    const ubyte* data = bytes.Data();
    const size_t size = bytes.Size();

    for (size_t i = 0; i < size; i += 3)
    {
        const uint32 remaining = uint32(MathUtil::Min(size - i, size_t(3)));

        uint32 triple = uint32(data[i]) << 16;

        if (remaining > 1)
        {
            triple |= uint32(data[i + 1]) << 8;
        }

        if (remaining > 2)
        {
            triple |= uint32(data[i + 2]);
        }

        out.Append(alphabet[(triple >> 18) & 0x3F]);
        out.Append(alphabet[(triple >> 12) & 0x3F]);
        out.Append(remaining > 1 ? alphabet[(triple >> 6) & 0x3F] : '=');
        out.Append(remaining > 2 ? alphabet[triple & 0x3F] : '=');
    }

    return out;
}

} // namespace MCP
} // namespace Hyperion
