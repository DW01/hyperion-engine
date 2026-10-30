/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Math/Vector2.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Name/Name.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <Core/Memory/ByteBuffer.hpp>

#include <Core/Functional/Proc.hpp>

namespace Hyperion {

class View;
class World;
class AssetRegistry;

namespace EditorHelpers {

struct ViewCaptureResult
{
    ByteBuffer rgba8; // sRGB, top-down; empty if the capture failed
    Vec2u extent;
};

/*! \brief Read back the view's rendered output on the GPU. \p onComplete runs exactly once, on the render thread. */
EDITOR_API void CaptureViewAsync(
    const Handle<View>& view,
    const Vec2u& wireframeExtent,
    const Vec2u& outputExtent,
    Proc<void(ViewCaptureResult&&)>&& onComplete);

EDITOR_API Task<void> CaptureViewToPNGAsync(
    const Handle<View>& view,
    const FilePath& path,
    const Vec2u& wireframeExtent,
    const Vec2u& outputExtent = Vec2u::Zero());

EDITOR_API void PopulateNewWorld(
    World& world,
    const Handle<AssetRegistry>& assetRegistry,
    Name sceneName);

} // namespace EditorHelpers
} // namespace Hyperion
