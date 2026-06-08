/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/HashCode.hpp>

#include <Core/Config/Config.hpp>

#include <Core/Utilities/EnumFlags.hpp>

#include <Core/Math/BoundingBox.hpp>

#include <Scene/Volume.hpp>

namespace Hyperion {

class IrradianceProbe;
class RenderProxyEnvGrid;

HYP_CLASS()
class ENGINE_API EnvGrid : public VolumeBase
{
    HYP_OBJECT_BODY(EnvGrid);

public:
    EnvGrid();
    
    explicit EnvGrid(Name name, const BoundingBox& localBounds = {});
    explicit EnvGrid(const BoundingBox& localBounds);

    EnvGrid(const EnvGrid& other) = delete;
    EnvGrid& operator=(const EnvGrid& other) = delete;

    ~EnvGrid() override;

    void OnAddedToWorld(World* world) override;
    void OnRemovedFromWorld(World* world) override;

    void UpdateRenderProxy(RenderProxyEnvGrid* proxy);

    HYP_FIELD()
    FixedArray<Handle<IrradianceProbe>, 4> probes;
};

} // namespace Hyperion
