/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Math/Vector2.hpp>

#include <Core/Reflection/Handle.hpp>

namespace Hyperion {

class View;

namespace filesystem {
class FilePath;
} // namespace filesystem

using filesystem::FilePath;

namespace EditorHelpers {

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
