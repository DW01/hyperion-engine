/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/Systems/LightmapSystem.hpp>

#include <Scene/EntityManager.hpp>
#include <Scene/Scene.hpp>
#include <Scene/World.hpp>
#include <Scene/Entity.hpp>
#include <Scene/EnvGrid.hpp>
#include <Scene/LightmapVolume.hpp>

#include <Core/Memory/Allocator/ThreadAllocator.hpp>

#include <LightmapSystem.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Lightmap);

void LightmapSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    LightmapElementComponent& lightmapElementComponent = entity->GetComponent<LightmapElementComponent>();
    BoundingBoxComponent& boundingBoxComponent = entity->GetComponent<BoundingBoxComponent>();

    // Assign to LightmapVolume if it has a valid path to a LightmapVolume but isn't assigned to one yet
    if (lightmapElementComponent.lightmapVolumePath.IsValid() && !lightmapElementComponent.lightmapVolume.IsValid())
    {
        if (!AssignLightmapVolume(entity->GetScene(), lightmapElementComponent, boundingBoxComponent))
        {
            HYP_LOG(Lightmap, Warning, "LightmapElementComponent for Entity {} could not be associated at runtime",
                entity->GetName());
        }
    }

    // Update probe lighting if this entity is in an EnvGrid.
    World* world = GetWorld();
    Assert(world != nullptr);

    if (world != nullptr)
    {
        bool updatedSphericalHarmonics = false;

        for (Scene* scene : world->GetScenes())
        {
            if (scene == entity->GetScene())
            {
                for (auto&& [envGridEntity, _] : scene->GetEntityManager()->GetEntitySet<EntityType<EnvGrid>>().GetScopedView(GetComponentInfos()))
                {
                    EnvGrid* envGrid = static_cast<EnvGrid*>(envGridEntity);

                    EvaluateSphericalHarmonicsResult result = envGrid->EvaluateSphericalHarmonics(*entity, lightmapElementComponent.shData);
                    
                    if (IsSuccess(result))
                    {
                        updatedSphericalHarmonics = true;
                    }
                    else
                    {
                        HYP_LOG(Lightmap, Warning, "Failed to evaluate spherical harmonics for Entity {} in EnvGrid {}: {}",
                            entity->GetName(), envGrid->GetName(), int(result));
                    }
                }
            }
        }

        if (updatedSphericalHarmonics)
        {
            entity->SetNeedsRenderProxyUpdate();
        }
    }
}

void LightmapSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    LightmapElementComponent& lightmapElementComponent = entity->GetComponent<LightmapElementComponent>();

    if (lightmapElementComponent.lightmapVolume.IsValid())
    {
        lightmapElementComponent.lightmapVolume.Reset();
    }

    // Zeroize, to avoid rendering it with stale SH data.
    Memory::Zero(&lightmapElementComponent.shData, sizeof(lightmapElementComponent.shData));

    entity->SetNeedsRenderProxyUpdate();
}

void LightmapSystem::Process(float delta, Span<Handle<Scene>> scenes)
{
    // Process sh lighting for dynamic entities in EnvGrids.
    Array<EnvGrid*, ThreadAllocator> envGrids;
    envGrids.Reserve(4);

    for (Scene* scene : scenes)
    {
        for (auto&& [envGridEntity, _] : scene->GetEntityManager()->GetEntitySet<EntityType<EnvGrid>>().GetScopedView(GetComponentInfos()))
        {
            EnvGrid* envGrid = static_cast<EnvGrid*>(envGridEntity);
            AssertDebug(!envGrids.Contains(envGrid));

            envGrids.PushBack(envGrid);
        }
    }

    for (Scene* scene : scenes)
    {
        // only dynamic entities.
        for (auto&& [entity, lightmapElementComponent, _] : scene->GetEntityManager()->GetEntitySet<LightmapElementComponent, TagComponent<EntityTag::MobDynamic>>().GetScopedView(GetComponentInfos()))
        {
            const BoundingBox entityWorldBounds = entity->GetWorldBounds();

            bool updatedSphericalHarmonics = false;

            for (EnvGrid* envGrid : envGrids)
            {
                if (!envGrid->GetWorldBounds().Overlaps(entityWorldBounds))
                {
                    continue;
                }

                EvaluateSphericalHarmonicsResult result = envGrid->EvaluateSphericalHarmonics(*entity, lightmapElementComponent.shData);

                if (IsSuccess(result))
                {
                    updatedSphericalHarmonics = true;
                }
                else
                {
                    HYP_LOG(Lightmap, Warning, "Failed to evaluate spherical harmonics for Entity {} in EnvGrid {}: {}",
                        entity->GetName(), envGrid->GetName(), int(result));
                }
            }

            if (updatedSphericalHarmonics)
            {
                entity->SetNeedsRenderProxyUpdate();
            }
        }
    }
}

bool LightmapSystem::AssignLightmapVolume(
    Scene* scene,
    LightmapElementComponent& lightmapElementComponent,
    BoundingBoxComponent& boundingBoxComponent)
{
    for (auto [entity, _] : scene->GetEntityManager()->GetEntitySet<EntityType<LightmapVolume>>().GetScopedView(GetComponentInfos()))
    {
        LightmapVolume* lightmapVolume = DynamicCast<LightmapVolume>(entity);
        Assert(lightmapVolume != nullptr);

        if (lightmapElementComponent.lightmapVolume.GetUnsafe() != lightmapVolume
            && lightmapElementComponent.lightmapVolumePath == lightmapVolume->GetPath())
        {
            const LightmapElement* lightmapElement = lightmapVolume->GetElement(lightmapElementComponent.lightmapElementId);

            if (!lightmapElement)
            {
                return false;
            }

            lightmapElementComponent.lightmapVolume = MakeWeakRef(lightmapVolume);

            entity->SetNeedsRenderProxyUpdate();

            return true;
        }
    }

    return false;
}

} // namespace Hyperion
