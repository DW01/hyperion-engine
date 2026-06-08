/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Core/Logging/Logger.hpp>

#include <Scene/EnvGrid.hpp>
#include <Scene/EnvProbe.hpp>

#include <Rendering/RenderProxy.hpp>

#include <EnvGrid.generated.inl>

namespace Hyperion {

#pragma region EnvGrid

EnvGrid::EnvGrid()
    : VolumeBase()
{
}

EnvGrid::EnvGrid(Name name, const BoundingBox& localBounds)
    : VolumeBase(name, localBounds)
{
}

EnvGrid::EnvGrid(const BoundingBox& localBounds)
    : VolumeBase(localBounds)
{
}

EnvGrid::~EnvGrid()
{
}

void EnvGrid::OnAddedToWorld(World* world)
{
    for (const Handle<IrradianceProbe>& probe : probes)
    {
        AddChild(probe);
    }
}

void EnvGrid::OnRemovedFromWorld(World* world)
{
    for (const Handle<IrradianceProbe>& probe : probes)
    {
        RemoveChild(probe, /* moveToDetached */ false);
    }
}

void EnvGrid::UpdateRenderProxy(RenderProxyEnvGrid* proxy)
{
    *proxy = {};
    proxy->envGrid = MakeWeakRef(this);
    proxy->bufferData = {};
}

#pragma endregion EnvGrid

} // namespace Hyperion
