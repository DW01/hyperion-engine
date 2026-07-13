/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Baking/Baker.hpp>

#include <Baking/ReflectionProbe/ReflectionProbeBakeData.hpp>

#include <Scene/EnvProbe.hpp>

namespace Hyperion {

class ReflectionProbe;

namespace Baking {

template <>
class Baker<ReflectionProbe> final : public BakerBase
{
public:
    Baker(BakerConfig&& config, const Handle<ReflectionProbe>& envProbe);

    Baker(const Baker& other) = delete;
    Baker& operator=(const Baker& other) = delete;

    Baker(Baker&& other) noexcept = delete;
    Baker& operator=(Baker&& other) noexcept = delete;

    virtual ~Baker() override = default;

    virtual bool ShouldSplitIntoJobs() const override
    {
        return false;
    }

    virtual bool OnlyOverlappingElements() const
    {
        return false;
    }

    virtual uint32 GetShadingTypesMask() const override
    {
        uint32 mask = 1u << int(LightmapShadingType::FULL);

        if (m_envProbe && (m_envProbe->GetEnvProbeFlags() & EPF_HAS_VISIBILITY))
        {
            mask |= 1u << int(LightmapShadingType::DISTANCE);
        }

        return mask;
    }

    virtual const TypeInfo& GetInnerType() const
    {
        return TypeOf<ReflectionProbe>();
    }

protected:
    virtual BakeDataBase& GetBakeData() override
    {
        return m_bakeData;
    }

    virtual UniquePtr<BakeJobBase> CreateJob(BakeJobParams&& params) override;

    virtual void CreateLightmapRenderers() override;

    virtual Result Build_Internal() override;
    virtual void OnCompleted_Internal() override;

    Handle<ReflectionProbe> m_envProbe;
    BakeData<ReflectionProbe> m_bakeData;
};

} // namespace Baking

} // namespace Hyperion
