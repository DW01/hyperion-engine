/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Core/Logging/Logger.hpp>

#include <Scene/ProbeVolume.hpp>
#include <Scene/EnvProbe.hpp>

#include <Scene/BakedLighting/SphericalHarmonics.hpp>

#include <Rendering/RenderProxy.hpp>

#include <ProbeVolume.generated.inl>

namespace Hyperion {

static bool GetBarycentrics(
    const Vec3f& inPosition,
    const Vec3f& A, const Vec3f& B, const Vec3f& C, const Vec3f& D,
    Vec4f& outWeights)
{
    const Vec3f vA = A - D;
    const Vec3f vB = B - D;
    const Vec3f vC = C - D;
    const Vec3f vP = inPosition - D;

    const float det = vA.Dot(vB.Cross(vC));

    if (MathUtil::Abs(det) < 1e-6f)
    {
        return false; // degenerate tetrahedron!
    }

    const float invDet = 1.0f / det;

    outWeights[0] = vP.Dot(vB.Cross(vC)) * invDet;
    outWeights[1] = vA.Dot(vP.Cross(vC)) * invDet;
    outWeights[2] = vA.Dot(vB.Cross(vP)) * invDet;
    outWeights[3] = 1.0f - outWeights[0] - outWeights[1] - outWeights[2];

    // All weights must be non-negative for P to be inside.
    return outWeights.Min() >= 0.0f;
}

#pragma region ProbeVolume

ProbeVolume::ProbeVolume()
    : VolumeBase()
{
}

ProbeVolume::ProbeVolume(Name name, const BoundingBox& localBounds)
    : VolumeBase(name, localBounds)
{
}

ProbeVolume::ProbeVolume(const BoundingBox& localBounds)
    : VolumeBase(localBounds)
{
}

ProbeVolume::~ProbeVolume()
{
    m_probes.Clear();
    m_probes.Refit();

    m_tetrahedra.Clear();
    m_tetrahedra.Refit();
}

void ProbeVolume::OnAddedToWorld(World* world)
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

void ProbeVolume::OnRemovedFromWorld(World* world)
{
    m_probes.Clear();
    m_probes.Refit();
}

void ProbeVolume::UpdateRenderProxy(RenderProxyProbeVolume* proxy)
{
    *proxy = {};
    proxy->probeVolume = MakeWeakRef(this);
    proxy->bufferData = {};
}

EvaluateSphericalHarmonicsResult ProbeVolume::EvaluateSphericalHarmonics(const Entity& inEntity, SphericalHarmonicsData& out) const
{
    // @TODO When a probe in the grid moves, it needs to notify us and then we need to update all
    // entities within this grid's aabb which have LightmapElementComponents to update their SH data as well.
    // also for dynamic entities, we'll need to recalculate their SH data in Entity::UpdateRenderProxy() based on their position and the probes in this grid.

    const Vec3f entityTranslation = inEntity.GetWorldTranslation();

    for (const Tetrahedron& tet : m_tetrahedra)
    {
        const Vec3f posA = m_probes[tet.probeIndices[0]]->GetWorldTranslation();
        const Vec3f posB = m_probes[tet.probeIndices[1]]->GetWorldTranslation();
        const Vec3f posC = m_probes[tet.probeIndices[2]]->GetWorldTranslation();
        const Vec3f posD = m_probes[tet.probeIndices[3]]->GetWorldTranslation();

        Vec4f weights;

        if (!GetBarycentrics(entityTranslation, posA, posB, posC, posD, weights))
        {
            continue;
        }

        const SphericalHarmonicsData& shA = m_probes[tet.probeIndices[0]]->GetSphericalHarmonicsData();
        const SphericalHarmonicsData& shB = m_probes[tet.probeIndices[1]]->GetSphericalHarmonicsData();
        const SphericalHarmonicsData& shC = m_probes[tet.probeIndices[2]]->GetSphericalHarmonicsData();
        const SphericalHarmonicsData& shD = m_probes[tet.probeIndices[3]]->GetSphericalHarmonicsData();

        out = shA * weights.x
            + shB * weights.y
            + shC * weights.z
            + shD * weights.w;

        return EvaluateSphericalHarmonicsResult::Success_InTetra;
    }

    // entity is outside all tetrahedra.
    float bestDist = FLT_MAX;
    
    const IrradianceProbe* bestProbe = nullptr;

    for (IrradianceProbe* probe : m_probes)
    {
        const float d = (probe->GetWorldTranslation() - entityTranslation).LengthSquared();

        if (d < bestDist)
        {
            bestDist = d;
            bestProbe = probe;
        }
    }

    if (bestProbe)
    {
        out = bestProbe->GetSphericalHarmonicsData();

        return EvaluateSphericalHarmonicsResult::Success_Fallback;
    }

    return EvaluateSphericalHarmonicsResult::Failure_OutsideOfVolume;
}

void ProbeVolume::RemoveAllProbes(bool freeMemory)
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

void ProbeVolume::SetGridSize(const Vec3u& gridSize)
{
    if (m_gridSize == gridSize)
    {
        return;
    }

    m_gridSize = gridSize;

    MarkDirty();

    CreateProbes();
}

void ProbeVolume::CreateProbes()
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

                BoundingBox probeLocalBounds;
                probeLocalBounds.min = -cellSize * 0.5f;
                probeLocalBounds.max = cellSize * 0.5f;

                Handle<IrradianceProbe> probe = MakeHandle<IrradianceProbe>(probeLocalBounds, Vec2u { 8, 8 });
                probe->SetLocalTranslation(cellMin + cellSize * 0.5f);

                AddChild(probe);

                // Probe handle is kept alive due to the fact that it's a child node
                m_probes.PushBack(probe.Get());
            }
        }
    }

    BakeTetrahedra();
}

void ProbeVolume::BakeTetrahedra()
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

#pragma endregion ProbeVolume

} // namespace Hyperion
