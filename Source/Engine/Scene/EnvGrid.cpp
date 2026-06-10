/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Core/Logging/Logger.hpp>

#include <Scene/EnvGrid.hpp>
#include <Scene/EnvProbe.hpp>

#include <Scene/BakedLighting/SphericalHarmonics.hpp>

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
    m_probes.Clear();
    m_probes.Refit();

    m_tetrahedra.Clear();
    m_tetrahedra.Refit();
}

void EnvGrid::OnAddedToWorld(World* world)
{
    // Load probes
    m_probes.Clear();
    m_probes.Reserve(m_gridSize.Volume());

    for (Node* node : GetChildren())
    {
        if (node->IsA<IrradianceProbe>())
        {
            m_probes.PushBack(static_cast<IrradianceProbe*>(node));
        }
    }
}

void EnvGrid::OnRemovedFromWorld(World* world)
{
    m_probes.Clear();
    m_probes.Refit();
}

void EnvGrid::UpdateRenderProxy(RenderProxyEnvGrid* proxy)
{
    *proxy = {};
    proxy->envGrid = MakeWeakRef(this);
    proxy->bufferData = {};
}

void EnvGrid::EvaluateSphericalHarmonics(const Entity& inEntity, SphericalHarmonicsData& out) const
{
    // @TODO! Implement proper SH evaluation using tetrahedron interpolation a la unity
    // @TODO When a probe in the grid moves, it needs to notify us and then we need to update all
    // entities within this grid's aabb which have LightmapElementComponents to update their SH data as well.
    // also for dynamic entities, we'll need to recalculate their SH data in Entity::UpdateRenderProxy() based on their position and the probes in this grid.
}

void EnvGrid::RemoveAllProbes(bool freeMemory)
{
    auto childNodes = GetChildren();

    for (Node* node : childNodes)
    {
        if (node->IsA<IrradianceProbe>())
        {
            RemoveChild(node, /* moveToDetached */ false);
        }
    }

    // Empty out probes array, release memory if applicable
    m_probes.Clear();

    if (freeMemory)
    {
        m_probes.Refit();
    }
}

#if HYP_EDITOR

void EnvGrid::SetGridSize(const Vec3u& gridSize)
{
    if (m_gridSize == gridSize)
    {
        return;
    }

    m_gridSize = gridSize;

    MarkDirty();

    CreateProbes();
}

void EnvGrid::CreateProbes()
{
    RemoveAllProbes(false);

    // Create new probes based on grid size
    const BoundingBox localBounds = GetLocalBounds();
    const Vec3f extent = localBounds.GetExtent();

    const Vec3f cellSize = Vec3f(
        extent.x / float(m_gridSize.x),
        extent.y / float(m_gridSize.y),
        extent.z / float(m_gridSize.z)
    );

    m_probes.Reserve(m_gridSize.Volume());

    for (uint32 z = 0; z < m_gridSize.z; z++)
    {
        for (uint32 y = 0; y < m_gridSize.y; y++)
        {
            for (uint32 x = 0; x < m_gridSize.x; x++)
            {
                const Vec3f cellMin = localBounds.min + Vec3f(
                    float(x) * cellSize.x,
                    float(y) * cellSize.y,
                    float(z) * cellSize.z
                );

                const Vec3f cellMax = cellMin + cellSize;

                Handle<IrradianceProbe> probe = MakeHandle<IrradianceProbe>(
                    BoundingBox(cellMin, cellMax),
                    Vec2u { 128, 128 }
                );

                probe->SetLocalTranslation(cellMin + cellSize * 0.5f);

                AddChild(probe);

                m_probes.PushBack(probe);
            }
        }
    }

    BakeTetrahedra();
}

void EnvGrid::BakeTetrahedra()
{
    m_tetrahedra.Clear();

    if (m_gridSize.x < 2 || m_gridSize.y < 2 || m_gridSize.z < 2)
    {
        return;
    }

    const uint32 strideX = 1;
    const uint32 strideY = m_gridSize.x;
    const uint32 strideZ = m_gridSize.x * m_gridSize.y;

    auto probeIndex = [&](uint32 x, uint32 y, uint32 z) -> uint32
    {
        return x * strideX + y * strideY + z * strideZ;
    };

    const uint32 numTetrahedra = 6 * (m_gridSize.x - 1) * (m_gridSize.y - 1) * (m_gridSize.z - 1);
    m_tetrahedra.Reserve(numTetrahedra);

    for (uint32 cz = 0; cz + 1 < m_gridSize.z; cz++)
    {
        for (uint32 cy = 0; cy + 1 < m_gridSize.y; cy++)
        {
            for (uint32 cx = 0; cx + 1 < m_gridSize.x; cx++)
            {
                const uint32 v000 = probeIndex(cx,     cy,     cz);
                const uint32 v100 = probeIndex(cx + 1, cy,     cz);
                const uint32 v101 = probeIndex(cx + 1, cy,     cz + 1);
                const uint32 v001 = probeIndex(cx,     cy,     cz + 1);
                const uint32 v010 = probeIndex(cx,     cy + 1, cz);
                const uint32 v110 = probeIndex(cx + 1, cy + 1, cz);
                const uint32 v111 = probeIndex(cx + 1, cy + 1, cz + 1);
                const uint32 v011 = probeIndex(cx,     cy + 1, cz + 1);

                m_tetrahedra.PushBack(Tetrahedron { { v000, v100, v110, v111 } });
                m_tetrahedra.PushBack(Tetrahedron { { v000, v110, v010, v111 } });
                m_tetrahedra.PushBack(Tetrahedron { { v000, v010, v011, v111 } });
                m_tetrahedra.PushBack(Tetrahedron { { v000, v011, v001, v111 } });
                m_tetrahedra.PushBack(Tetrahedron { { v000, v001, v101, v111 } });
                m_tetrahedra.PushBack(Tetrahedron { { v000, v101, v100, v111 } });
            }
        }
    }
}

#endif // HYP_EDITOR

#pragma endregion EnvGrid

} // namespace Hyperion
