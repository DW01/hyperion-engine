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
    for (AssetReference& probeAssetReference : probes)
    {
        if (probeAssetReference.IsValid())
        {
            Handle<IrradianceProbe> probe = DynamicCast<IrradianceProbe>(probeAssetReference.Resolve());

            if (!probe.IsValid())
            {
                HYP_LOG(Scene, Warning, "Failed to load irradiance probe at path: {}", probeAssetReference.GetAssetPath().ToString());

                continue;
            }

            AddChild(probe);
        }
    }
}

void EnvGrid::OnRemovedFromWorld(World* world)
{
    auto childNodes = GetChildren();
    for (Node* node : childNodes)
    {
        if (node && node->IsA(IrradianceProbe::StaticClass()))
        {
            RemoveChild(node, /* moveToDetached */ false);
        }
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
