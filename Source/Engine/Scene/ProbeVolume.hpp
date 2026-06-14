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
#include <Core/Math/Mat3f.hpp>

#include <Framework/EngineMemory.hpp>

#include <Scene/Volume.hpp>

namespace Hyperion {

class IrradianceProbe;
struct RenderProxyProbeVolume;

struct SphericalHarmonicsData;
enum class EvaluateSphericalHarmonicsResult : int8;

HYP_STRUCT()
struct Tetrahedron
{
    HYP_STRUCT_BODY(Tetrahedron);

    HYP_FIELD()
    FixedArray<uint32, 4> probeIndices;

    HYP_FIELD()
    FixedArray<int32, 4> neighbours;

    HYP_FIELD()
    Mat3f invEdgeMatrix;
};

HYP_CLASS()
class ENGINE_API ProbeVolume : public VolumeBase
{
    HYP_OBJECT_BODY(ProbeVolume);

public:
    ProbeVolume();
    
    explicit ProbeVolume(Name name, const BoundingBox& localBounds = {});
    explicit ProbeVolume(const BoundingBox& localBounds);

    ProbeVolume(const ProbeVolume& other) = delete;
    ProbeVolume& operator=(const ProbeVolume& other) = delete;

    ~ProbeVolume() override;

    void OnAddedToWorld(World* world) override;
    void OnRemovedFromWorld(World* world) override;

    void UpdateRenderProxy(RenderProxyProbeVolume* proxy);

    void RefreshProbe(IrradianceProbe& probe);

    EvaluateSphericalHarmonicsResult EvaluateSphericalHarmonics(
        const Entity& inEntity, SphericalHarmonicsData& out) const;

    Span<IrradianceProbe* const> GetProbes() const
    {
        return m_probes;
    }

private:
    void OnTransformUpdated() override;

    void RemoveAllProbes(bool freeMemory);

    int32 FindEnclosingTetrahedron(const Vec3f& position) const;

    void RebuildRuntimeData();

#if HYP_EDITOR
public:
    HYP_METHOD(Property = "GridSize", EditorOnly)
    const Vec3u& GetGridSize() const
    {
        return m_gridSize;
    }

    HYP_METHOD(Property = "GridSize", EditorOnly)
    void SetGridSize(const Vec3u& gridSize);

    void CreateProbes();

    HYP_METHOD(EditAction = "Rebuild Connectivity")
    void BakeTetrahedra();

    Span<const Tetrahedron> GetTetrahedra() const
    {
        return m_tetrahedra;
    }
    
private:
    HYP_FIELD(Property = "GridSize", EditorOnly, Serialize)
    Vec3u m_gridSize = Vec3u { 2, 2, 2 };
#endif // HYP_EDITOR

    HYP_FIELD(Property = "Probes", Transient, EditHide)
    Array<IrradianceProbe*, SceneAllocator> m_probes;

    HYP_FIELD(Property = "Tetrahedra", Serialize, EditHide)
    Array<Tetrahedron, SceneAllocator> m_tetrahedra;

    mutable int32 m_lastTetHint = 0;
};

} // namespace Hyperion
