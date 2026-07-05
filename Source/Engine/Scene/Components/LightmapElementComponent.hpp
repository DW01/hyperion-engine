/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Reflection/Handle.hpp>

#include <Core/Utilities/Uuid.hpp>

#include <Core/HashCode.hpp>

#include <Asset/AssetPath.hpp>

#include <Scene/BakedLighting/SphericalHarmonics.hpp>

namespace Hyperion {

class LightmapVolume;

enum class LightmapElementId : uint32;

/*! \brief Used for Entities which have baked light via LightmapVolumes or probes to manage their state
 *   as well as hold their evaluated spherical harmonics coefficients. */
HYP_STRUCT(Component)
struct ENGINE_API LightmapElementComponent
{
    HYP_STRUCT_BODY(LightmapElementComponent);

    HYP_FIELD()
    LightmapElementId lightmapElementId;

    HYP_FIELD()
    Name lightmapVolumeName;

    HYP_FIELD(Transient)
    WeakHandle<LightmapVolume> lightmapVolume;

    // Spherical Harmonics for light probes, computed dynamically via the LightmapSystem.
    HYP_FIELD(Transient)
    SphericalHarmonicsData shData;

    LightmapElementComponent();
};

} // namespace Hyperion
