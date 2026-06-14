/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Core/Logging/Logger.hpp>

#include <Core/Memory/Allocator/ThreadAllocator.hpp>

#include <Scene/ProbeVolume.hpp>
#include <Scene/EnvProbe.hpp>
#include <Scene/Scene.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/World.hpp>

#include <Scene/Components/LightmapElementComponent.hpp>
#include <Scene/Components/MeshComponent.hpp>

#include <Scene/BakedLighting/SphericalHarmonics.hpp>

#include <Rendering/RenderProxy.hpp>

#include <ProbeVolume.generated.inl>

#include <algorithm>
#include <cmath>

namespace Hyperion {

#pragma region Helpers

namespace {

struct Circumsphere
{
    Vec3f center;
    float radiusSq;
};

static Circumsphere ComputeCircumsphere(
    Vec3f a, Vec3f b, Vec3f c, Vec3f d)
{
    const Vec3f ba = b - a;
    const Vec3f ca = c - a;
    const Vec3f da = d - a;

    Mat3f A;
    A[0][0] = 2.0f * ba.x; A[0][1] = 2.0f * ba.y; A[0][2] = 2.0f * ba.z;
    A[1][0] = 2.0f * ca.x; A[1][1] = 2.0f * ca.y; A[1][2] = 2.0f * ca.z;
    A[2][0] = 2.0f * da.x; A[2][1] = 2.0f * da.y; A[2][2] = 2.0f * da.z;

    const float detA = A.Determinant();

    if (MathUtil::Abs(detA) < 1e-10f)
    {
        // Degenerate (co-planar) — return infinite sphere so Bowyer-Watson
        // always considers these as inside and retries.
        return { Vec3f::Zero(), MathUtil::MaxSafeValue<float>() };
    }

    const Vec3f B = Vec3f(
        ba.LengthSquared(),
        ca.LengthSquared(),
        da.LengthSquared()
    );

    // Cramer's rule: substitute each column of A with B
    auto ColSub = [](const Mat3f& mat, const Vec3f& b, int col) -> Mat3f
    {
        Mat3f result = mat;
        result[0][col] = b.x;
        result[1][col] = b.y;
        result[2][col] = b.z;
        return result;
    };

    const Mat3f Ax = ColSub(A, B, 0);
    const Mat3f Ay = ColSub(A, B, 1);
    const Mat3f Az = ColSub(A, B, 2);

    const Vec3f center = Vec3f(
        a.x + Ax.Determinant() / detA,
        a.y + Ay.Determinant() / detA,
        a.z + Az.Determinant() / detA
    );

    const float radiusSq = (center - a).LengthSquared();

    return { center, radiusSq };
}

struct SortedFace
{
    uint32 v[3];

    SortedFace(uint32 a, uint32 b, uint32 c)
    {
        v[0] = a;
        v[1] = b;
        v[2] = c;

        // Manual sort of 3 elements (network sort)
        if (v[0] > v[1]) std::swap(v[0], v[1]);
        if (v[1] > v[2]) std::swap(v[1], v[2]);
        if (v[0] > v[1]) std::swap(v[0], v[1]);
    }

    bool operator==(const SortedFace& other) const
    {
        return v[0] == other.v[0]
            && v[1] == other.v[1]
            && v[2] == other.v[2];
    }
};


// Helper to calculate a 4x4 matrix determinant
static inline double Det4x4(double m[4][4])
{
    double c00 = m[2][2] * m[3][3] - m[2][3] * m[3][2];
    double c01 = m[2][1] * m[3][3] - m[2][3] * m[3][1];
    double c02 = m[2][1] * m[3][2] - m[2][2] * m[3][1];
    double c03 = m[2][0] * m[3][3] - m[2][3] * m[3][0];
    double c04 = m[2][0] * m[3][2] - m[2][2] * m[3][0];
    double c05 = m[2][0] * m[3][1] - m[2][1] * m[3][0];

    return m[0][0] * (m[1][1] * c00 - m[1][2] * c01 + m[1][3] * c02)
         - m[0][1] * (m[1][0] * c00 - m[1][2] * c03 + m[1][3] * c04)
         + m[0][2] * (m[1][0] * c01 - m[1][1] * c03 + m[1][3] * c05)
         - m[0][3] * (m[1][0] * c02 - m[1][1] * c04 + m[1][2] * c05);
}

// Returns > 0 if a, b, c, d are positively oriented (counter-clockwise)
static inline double Orient3D(const Vec3f& a, const Vec3f& b, const Vec3f& c, const Vec3f& d)
{
    double adx = (double)a.x - (double)d.x;
    double ady = (double)a.y - (double)d.y;
    double adz = (double)a.z - (double)d.z;
    
    double bdx = (double)b.x - (double)d.x;
    double bdy = (double)b.y - (double)d.y;
    double bdz = (double)b.z - (double)d.z;
    
    double cdx = (double)c.x - (double)d.x;
    double cdy = (double)c.y - (double)d.y;
    double cdz = (double)c.z - (double)d.z;

    return adx * (bdy * cdz - bdz * cdy)
         - ady * (bdx * cdz - bdz * cdx)
         + adz * (bdx * cdy - bdy * cdx);
}

// Returns > 0 if point 'e' is inside the circumsphere of positively oriented tet a, b, c, d
static inline double InSphere(const Vec3f& a, const Vec3f& b, const Vec3f& c, const Vec3f& d, const Vec3f& e)
{
    double aex = (double)a.x - (double)e.x;
    double aey = (double)a.y - (double)e.y;
    double aez = (double)a.z - (double)e.z;
    
    double bex = (double)b.x - (double)e.x;
    double bey = (double)b.y - (double)e.y;
    double bez = (double)b.z - (double)e.z;
    
    double cex = (double)c.x - (double)e.x;
    double cey = (double)c.y - (double)e.y;
    double cez = (double)c.z - (double)e.z;
    
    double dex = (double)d.x - (double)e.x;
    double dey = (double)d.y - (double)e.y;
    double dez = (double)d.z - (double)e.z;

    double m[4][4] = {
        { aex, aey, aez, aex*aex + aey*aey + aez*aez },
        { bex, bey, bez, bex*bex + bey*bey + bez*bez },
        { cex, cey, cez, cex*cex + cey*cey + cez*cez },
        { dex, dey, dez, dex*dex + dey*dey + dez*dez }
    };

    return Det4x4(m);
}

} // namespace

#pragma endregion Helpers

#pragma region ProbeVolume

ProbeVolume::ProbeVolume()
    : VolumeBase(),
      m_lastTetHint(0)
{
}

ProbeVolume::ProbeVolume(Name name, const BoundingBox& localBounds)
    : VolumeBase(name, localBounds),
      m_lastTetHint(0)
{
}

ProbeVolume::ProbeVolume(const BoundingBox& localBounds)
    : VolumeBase(localBounds),
      m_lastTetHint(0)
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

    // Precompute runtime data from loaded tetrahedra
    RebuildRuntimeData();
}

void ProbeVolume::OnRemovedFromWorld(World* world)
{
    m_probes.Clear();
    m_probes.Refit();

    m_lastTetHint = 0;
}

void ProbeVolume::OnTransformUpdated()
{
    VolumeBase::OnTransformUpdated();

    for (IrradianceProbe* probe : m_probes)
    {
        RefreshProbe(*probe);
    }
}

void ProbeVolume::UpdateRenderProxy(RenderProxyProbeVolume* proxy)
{
    *proxy = {};
    proxy->probeVolume = MakeWeakRef(this);
    proxy->bufferData = {};
}

void ProbeVolume::RebuildRuntimeData()
{
    if (m_tetrahedra.Empty() || m_probes.Empty())
    {
        return;
    }

    for (Tetrahedron& tet : m_tetrahedra)
    {
        const Vec3f p0 = m_probes[tet.probeIndices[0]]->GetWorldTranslation();
        const Vec3f e1 = m_probes[tet.probeIndices[1]]->GetWorldTranslation() - p0;
        const Vec3f e2 = m_probes[tet.probeIndices[2]]->GetWorldTranslation() - p0;
        const Vec3f e3 = m_probes[tet.probeIndices[3]]->GetWorldTranslation() - p0;

        // Build edge matrix (columns are e1, e2, e3), store inverse
        const float edgeData[9] = {
            e1.x, e2.x, e3.x,
            e1.y, e2.y, e3.y,
            e1.z, e2.z, e3.z
        };

        tet.invEdgeMatrix = Mat3f(edgeData).Inverse();

        // Reset neighbours — rebuilt below
        tet.neighbours[0] = -1;
        tet.neighbours[1] = -1;
        tet.neighbours[2] = -1;
        tet.neighbours[3] = -1;
    }

    // Build adjacency: for each tet face, find the neighbouring tet sharing it
    const uint32 numTets = m_tetrahedra.Size();

    for (uint32 i = 0; i < numTets; i++)
    {
        Tetrahedron& tet = m_tetrahedra[i];

        for (uint32 fi = 0; fi < 4; fi++)
        {
            if (tet.neighbours[fi] != -1)
            {
                continue;
            }

            // Build the face opposite vertex fi
            uint32 fv[3];
            uint32 k = 0;

            for (uint32 j = 0; j < 4; j++)
            {
                if (j != fi)
                {
                    fv[k++] = tet.probeIndices[j];
                }
            }

            const SortedFace key(fv[0], fv[1], fv[2]);

            for (uint32 j = i + 1; j < numTets; j++)
            {
                if (i == j)
                {
                    continue;
                }

                Tetrahedron& other = m_tetrahedra[j];

                // Check each face of the other tet
                for (uint32 ofi = 0; ofi < 4; ofi++)
                {
                    uint32 ofv[3];
                    uint32 ok = 0;

                    for (uint32 oj = 0; oj < 4; oj++)
                    {
                        if (oj != ofi)
                        {
                            ofv[ok++] = other.probeIndices[oj];
                        }
                    }

                    if (SortedFace(ofv[0], ofv[1], ofv[2]) == key)
                    {
                        tet.neighbours[fi] = static_cast<int32>(j);
                        other.neighbours[ofi] = static_cast<int32>(i);

                        break;
                    }
                }

                if (tet.neighbours[fi] != -1)
                {
                    break;
                }
            }
        }
    }
}

/// Light Probe Interpolation Using Tetrahedral Tessellations, Robert Cupisz
/// https://gdcvault.com/play/1015312/Light-Probe-Interpolation-Using-Tetrahedral
int32 ProbeVolume::FindEnclosingTetrahedron(const Vec3f& position) const
{
    int32 current = (m_lastTetHint >= 0 && m_lastTetHint < static_cast<int32>(m_tetrahedra.Size()))
        ? m_lastTetHint
        : 0;

    int32 previous = -1; // Track where we came from
    const int32 maxSteps = static_cast<int32>(m_tetrahedra.Size()) + 1;

    for (int32 step = 0; step < maxSteps; step++)
    {
        const Tetrahedron& tet = m_tetrahedra[current];
        const Vec3f d = position - m_probes[tet.probeIndices[0]]->GetWorldTranslation();

        const float b1 = tet.invEdgeMatrix[0][0] * d.x + tet.invEdgeMatrix[0][1] * d.y + tet.invEdgeMatrix[0][2] * d.z;
        const float b2 = tet.invEdgeMatrix[1][0] * d.x + tet.invEdgeMatrix[1][1] * d.y + tet.invEdgeMatrix[1][2] * d.z;
        const float b3 = tet.invEdgeMatrix[2][0] * d.x + tet.invEdgeMatrix[2][1] * d.y + tet.invEdgeMatrix[2][2] * d.z;
        const float b0 = 1.0f - b1 - b2 - b3;

        const float eps = -1e-5f;

        // Inside the tetrahedron
        if (b0 >= eps && b1 >= eps && b2 >= eps && b3 >= eps)
        {
            return current;
        }

        int32 worstFace = 0;
        float worstVal = b0;

        if (b1 < worstVal) { worstVal = b1; worstFace = 1; }
        if (b2 < worstVal) { worstVal = b2; worstFace = 2; }
        if (b3 < worstVal) { worstVal = b3; worstFace = 3; }

        const int32 next = tet.neighbours[worstFace];

        if (next == -1)
        {
            return -1;
        }
        
        //We are ping-ponging due to float precision [cite: 589, 590]
        if (next == previous)
        {
            return current; // We are essentially on the boundary face, return whichever [cite: 590]
        }

        previous = current;
        current = next;
    }

    // Max steps reached, just return the last one
    return current;
}

EvaluateSphericalHarmonicsResult ProbeVolume::EvaluateSphericalHarmonics(
    const Entity& inEntity, SphericalHarmonicsData& out) const
{
    const Vec3f position = inEntity.GetWorldTranslation();

    const int32 tetIdx = FindEnclosingTetrahedron(position);

    if (tetIdx < 0)
    {
        return EvaluateSphericalHarmonicsResult::Failure_OutsideOfVolume;
    }

    m_lastTetHint = tetIdx;

    const Tetrahedron& tet = m_tetrahedra[tetIdx];

    const Vec3f d = position - m_probes[tet.probeIndices[0]]->GetWorldTranslation();

    Vec4f weights;
    weights[1] = tet.invEdgeMatrix[0][0] * d.x + tet.invEdgeMatrix[0][1] * d.y + tet.invEdgeMatrix[0][2] * d.z;
    weights[2] = tet.invEdgeMatrix[1][0] * d.x + tet.invEdgeMatrix[1][1] * d.y + tet.invEdgeMatrix[1][2] * d.z;
    weights[3] = tet.invEdgeMatrix[2][0] * d.x + tet.invEdgeMatrix[2][1] * d.y + tet.invEdgeMatrix[2][2] * d.z;
    weights[0] = 1.0f - weights[1] - weights[2] - weights[3];

    const SphericalHarmonicsData& shA = m_probes[tet.probeIndices[0]]->GetSphericalHarmonicsData();
    const SphericalHarmonicsData& shB = m_probes[tet.probeIndices[1]]->GetSphericalHarmonicsData();
    const SphericalHarmonicsData& shC = m_probes[tet.probeIndices[2]]->GetSphericalHarmonicsData();
    const SphericalHarmonicsData& shD = m_probes[tet.probeIndices[3]]->GetSphericalHarmonicsData();

    out = shA * weights[0]
        + shB * weights[1]
        + shC * weights[2]
        + shD * weights[3];

    return EvaluateSphericalHarmonicsResult::Success_InTetra;
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

void ProbeVolume::RefreshProbe(IrradianceProbe& probe)
{
    // If probe has moved, we need to find entities in the world that overlap and ensure
    // they have a LightmapElementComponent so that LightmapSystem can do what it needs to do.

    World* world = GetWorld();

    if (!world)
    {
        return;
    }

    const BoundingBox probeBounds = probe.GetLocalBounds();
    const Mat4f worldTransform = probe.GetWorldMatrix();
    const BoundingBox worldBounds = worldTransform * probeBounds;

    Array<Entity*, ThreadAllocator> overlappingEntities;

    for (Scene* scene : world->GetScenes())
    {
        for (auto&& [entity, meshComponent] : scene->GetEntityManager()->GetEntitySet<MeshComponent>().GetScopedView(DataAccessFlags::ACCESS_RW))
        {
            const BoundingBox entityWorldBounds = entity->GetWorldBounds();

            if (entityWorldBounds.Overlaps(worldBounds))
            {
                overlappingEntities.PushBack(entity);
            }
        }
    }

    for (Entity* entity : overlappingEntities)
    {
        entity->AddTag<EntityTag::UpdateSphericalHarmonicsData>();

        if (entity->HasComponent<LightmapElementComponent>())
        {
            continue;
        }

        LightmapElementComponent component {};
        entity->AddComponent<LightmapElementComponent>(component);
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

    uint32 seed = static_cast<uint32>(Time::Now().ToMilliseconds() % UINT32_MAX);

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

                const float jitterAmount = cellSize.x * 0.05f;
                const Vec3f jitter = Vec3f(
                    MathUtil::RandomInRange(seed, -jitterAmount, jitterAmount),
                    MathUtil::RandomInRange(seed, -jitterAmount, jitterAmount),
                    MathUtil::RandomInRange(seed, -jitterAmount, jitterAmount)
                );

                Handle<IrradianceProbe> probe = MakeHandle<IrradianceProbe>(probeLocalBounds, Vec2u { 8, 8 });
                probe->SetLocalTranslation(cellMin + (cellSize * 0.5f) + jitter);

                AddChild(probe);

                // Probe handle is kept alive due to the fact that it's a child node
                m_probes.PushBack(probe.Get());
            }
        }
    }

    BakeTetrahedra();
    
    for (IrradianceProbe* probe : m_probes)
    {
        RefreshProbe(*probe);
    }
}

void ProbeVolume::BakeTetrahedra()
{
    m_tetrahedra.Clear();

    if (m_probes.Size() < 4)
    {
        return;
    }

    m_lastTetHint = 0;

    const uint32 realCount = m_probes.Size();

    Array<Vec3f, ThreadAllocator> positions;
    positions.Reserve(realCount + 4);

    for (IrradianceProbe* probe : m_probes)
    {
        positions.PushBack(probe->GetWorldTranslation());
    }

    // 1. Calculate bounds to create a massive Super-Tetrahedron
    Vec3f mn = positions[0];
    Vec3f mx = positions[0];

    for (uint32 i = 0; i < realCount; ++i)
    {
        const Vec3f& p = positions[i];
        mn.x = MathUtil::Min(mn.x, p.x);
        mn.y = MathUtil::Min(mn.y, p.y);
        mn.z = MathUtil::Min(mn.z, p.z);

        mx.x = MathUtil::Max(mx.x, p.x);
        mx.y = MathUtil::Max(mx.y, p.y);
        mx.z = MathUtil::Max(mx.z, p.z);
    }

    const Vec3f extent = mx - mn;
    const float maxDim = MathUtil::Max(extent.x, MathUtil::Max(extent.y, extent.z));
    const Vec3f center = (mn + mx) * 0.5f;
    
    // Massive padding prevents outer hull geometry from being clipped by the super-tet circumspheres
    const float s = maxDim * 10.0f; 

    const Vec3f svPositions[4] = {
        Vec3f(center.x, center.y + 3.0f * s, center.z),
        Vec3f(center.x - 2.0f * s, center.y - s, center.z + 2.0f * s),
        Vec3f(center.x + 2.0f * s, center.y - s, center.z + 2.0f * s),
        Vec3f(center.x, center.y - s, center.z - 2.0f * s)
    };

    for (const Vec3f& sv : svPositions)
    {
        positions.PushBack(sv);
    }

    const uint32 sv0 = realCount;
    const uint32 sv1 = realCount + 1;
    const uint32 sv2 = realCount + 2;
    const uint32 sv3 = realCount + 3;

    struct WorkingTet { uint32 v[4]; };
    Array<WorkingTet, ThreadAllocator> workingTets;
    workingTets.Reserve(realCount * 6);

    // Initial super-tetrahedron (ensure positive orientation)
    WorkingTet initialTet = { sv0, sv1, sv2, sv3 };
    if (Orient3D(positions[sv0], positions[sv1], positions[sv2], positions[sv3]) < 0.0)
    {
        std::swap(initialTet.v[1], initialTet.v[2]);
    }
    workingTets.PushBack(initialTet);

    // 2. Bowyer-Watson Point Insertion
    for (uint32 pi = 0; pi < realCount; pi++)
    {
        const Vec3f& pt = positions[pi];

        Array<uint32, ThreadAllocator> bad;
        bad.Reserve(workingTets.Size());

        for (uint32 ti = 0; ti < uint32(workingTets.Size()); ti++)
        {
            const WorkingTet& wt = workingTets[ti];

            // If point is inside circumsphere, this tet must be destroyed
            // We use an epsilon of 1e-9 to reject floating point border noise
            if (InSphere(positions[wt.v[0]], positions[wt.v[1]], positions[wt.v[2]], positions[wt.v[3]], pt) > 0)
            {
                bad.PushBack(ti);
            }
        }

        if (bad.Empty()) continue;

        Array<SortedFace, ThreadAllocator> boundary;
        boundary.Reserve(bad.Size() * 4);

        for (uint32 ti : bad)
        {
            const WorkingTet& wt = workingTets[ti];
            const SortedFace faces[4] = {
                SortedFace(wt.v[1], wt.v[2], wt.v[3]),
                SortedFace(wt.v[0], wt.v[2], wt.v[3]),
                SortedFace(wt.v[0], wt.v[1], wt.v[3]),
                SortedFace(wt.v[0], wt.v[1], wt.v[2])
            };

            // XOR the faces: Only keep faces that appear exactly once in the "bad" list
            for (const SortedFace& face : faces)
            {
                auto it = std::find(boundary.begin(), boundary.end(), face);
                if (it != boundary.end()) {
                    boundary.Erase(it);
                } else {
                    boundary.PushBack(face);
                }
            }
        }

        Bitset remove(workingTets.Size());
        for (uint32 ti : bad) remove.Set(ti, true);

        Array<WorkingTet, ThreadAllocator> kept;
        kept.Reserve(workingTets.Size() - bad.Size());

        for (uint32 ti = 0; ti < uint32(workingTets.Size()); ti++)
        {
            if (!remove.Test(ti)) kept.PushBack(workingTets[ti]);
        }

        workingTets = std::move(kept);

        // 3. Stitch the boundary cavity to the new point
        for (const SortedFace& face : boundary)
        {
            WorkingTet newTet = { pi, face.v[0], face.v[1], face.v[2] };
            
            // Ensure proper winding order!
            if (Orient3D(positions[newTet.v[0]], positions[newTet.v[1]], positions[newTet.v[2]], positions[newTet.v[3]]) < 0.0)
            {
                std::swap(newTet.v[1], newTet.v[2]);
            }

            workingTets.PushBack(newTet);
        }
    }

    Array<WorkingTet, ThreadAllocator> finalTets;
    finalTets.Reserve(workingTets.Size());

    for (const WorkingTet& wt : workingTets)
    {
        bool isSuper = false;
        for (uint32 k = 0; k < 4; k++)
        {
            if (wt.v[k] >= realCount)
            {
                isSuper = true;
                break;
            }
        }

        // Only save internal tetrahedra
        if (!isSuper)
        {
            finalTets.PushBack(wt);
        }
    }

    m_tetrahedra.Reserve(finalTets.Size());

    for (const WorkingTet& wt : finalTets)
    {
        Tetrahedron tet;

        for (uint32 k = 0; k < 4; k++)
        {
            tet.probeIndices[k] = wt.v[k];
            tet.neighbours[k] = -1;
        }

        const Vec3f p0 = positions[wt.v[0]];
        const Vec3f e1 = positions[wt.v[1]] - p0;
        const Vec3f e2 = positions[wt.v[2]] - p0;
        const Vec3f e3 = positions[wt.v[3]] - p0;

        const Mat3f edgeData { {
            e1.x, e2.x, e3.x,
            e1.y, e2.y, e3.y,
            e1.z, e2.z, e3.z
        } };

        tet.invEdgeMatrix = edgeData.Inverse();

        m_tetrahedra.PushBack(tet);
    }
    
    // Finally, build adjacency mapping
    RebuildRuntimeData();
}

#endif // HYP_EDITOR

#pragma endregion ProbeVolume

} // namespace Hyperion
