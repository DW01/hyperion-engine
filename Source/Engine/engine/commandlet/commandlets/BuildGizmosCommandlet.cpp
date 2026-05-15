#include <HyperionPch.hpp>

#include <engine/commandlet/Commandlet.hpp>

#include <Core/reflection/ClassUtils.hpp>
#include <Core/reflection/ClassRegistry.hpp>

#include <Core/cli/CommandLine.hpp>

#include <asset/AssetRegistry.hpp>

#include <rendering/Mesh.hpp>
#include <rendering/MaterialDefinition.hpp>
#include <rendering/MaterialInstance.hpp>

#include <scene/Entity.hpp>
#include <scene/EntityManager.hpp>

#include <scene/components/MeshComponent.hpp>
#include <scene/components/VisibilityStateComponent.hpp>

#include <util/MeshBuilder.hpp>

#include <Core/math/Quat4f.hpp>
#include <Core/math/Transform.hpp>

namespace Hyperion {

static Handle<Entity> CreateAxisEntity(
    const char* entityName,
    const Handle<Mesh>& axisMesh,
    const Vec4f& axisColor,
    int axisIndex,
    const Quat4f& axisRotation)
{
    MaterialAttributes materialAttributes;
    materialAttributes.bucket = RenderBucket::Debug;
    materialAttributes.cullFaces = FCM_NONE;

    MaterialParameters materialParameters;
    materialParameters.albedo = axisColor;

    Handle<MaterialDefinition> materialDefinition = MakeHandle<MaterialDefinition>(
        NAME_FMT("{}_Material", entityName),
        materialAttributes,
        materialParameters,
        MaterialTextures {});
    InitObject(materialDefinition);
    GetCurrentAssetRegistry()->PutAssetsDeep(materialDefinition);

    Handle<MaterialInstance> materialInstance = materialDefinition->CreateInstance();
    materialInstance->SetIsDynamic(true);
    InitObject(materialInstance);
    GetCurrentAssetRegistry()->PutAssetsDeep(materialInstance);

    Handle<Entity> axisEntity = MakeHandle<Entity>(NAME_FMT("{}", entityName));
    axisEntity->SetIsDynamic(true);
    axisEntity->UnlockTransform();
    axisEntity->SetLocalRotation(axisRotation);

    AssertDebug(axisEntity->GetScene() != nullptr);

    axisEntity->Node::AddTag(NodeTag(NAME("TransformWidgetAxis"), axisIndex));
    axisEntity->Node::AddTag(NodeTag(NAME("TransformWidgetElementColor"), axisColor));

    axisEntity->AddComponent<MeshComponent>(MeshComponent { axisMesh, materialInstance });
    axisEntity->SetLocalBounds(axisMesh->GetAABB());

    axisEntity->AddComponent<VisibilityStateComponent>(VisibilityStateComponent { VisibilityStateFlags::ALWAYS_VISIBLE });


    return axisEntity;
}

static Handle<Entity> CreateCentroidEntity(
    const char* entityName,
    const Handle<Mesh>& centroidMesh,
    const Vec4f& centroidColor)
{
    MaterialAttributes materialAttributes;
    materialAttributes.bucket = RenderBucket::Debug;
    materialAttributes.cullFaces = FCM_NONE;

    MaterialParameters materialParameters;
    materialParameters.albedo = centroidColor;

    Handle<MaterialDefinition> materialDefinition = MakeHandle<MaterialDefinition>(
        NAME_FMT("{}_Material", entityName),
        materialAttributes,
        materialParameters,
        MaterialTextures {});
    InitObject(materialDefinition);
    GetCurrentAssetRegistry()->PutAssetsDeep(materialDefinition);

    Handle<MaterialInstance> materialInstance = materialDefinition->CreateInstance();
    materialInstance->SetIsDynamic(true);
    InitObject(materialInstance);
    GetCurrentAssetRegistry()->PutAssetsDeep(materialInstance);

    Handle<Entity> centroidEntity = MakeHandle<Entity>(NAME_FMT("{}", entityName));
    centroidEntity->SetIsDynamic(true);

    centroidEntity->Node::AddTag(NodeTag(NAME("TransformWidgetAxis"), -1));
    centroidEntity->Node::AddTag(NodeTag(NAME("TransformWidgetElementColor"), centroidColor));

    centroidEntity->AddComponent<MeshComponent>(MeshComponent { centroidMesh, materialInstance });
    centroidEntity->SetLocalBounds(centroidMesh->GetAABB());

    centroidEntity->AddComponent<VisibilityStateComponent>(VisibilityStateComponent { VisibilityStateFlags::ALWAYS_VISIBLE });

    return centroidEntity;
}

static void BuildTranslateGizmo(Handle<AssetRegistry>& assetRegistry)
{
    GlobalContextScope assetRegistryScope { AssetRegistryContext { assetRegistry } };

    if (Handle<Node> existingNode = GetCurrentAssetRegistry()->GetAsset<Node>(AssetBuckets::Nodes, "TranslateGizmo"_sh); existingNode.IsValid())
    {
        HYP_LOG(Engine, Info, "TranslateGizmo already exists in editor asset registry, skipping build.");
        return;
    }

    Handle<Mesh> cylinderMesh = MeshBuilder::Cylinder(0.03f, 0.7f, 16);
    InitObject(cylinderMesh);

    Handle<Mesh> coneMesh = MeshBuilder::Cone(0.08f, 0.25f, 16);
    InitObject(coneMesh);

    Transform coneTransform;
    coneTransform.translation = Vec3f(0.0f, 0.475f, 0.0f);

    Handle<Mesh> axisMesh = MeshBuilder::Merge(cylinderMesh.Get(), coneMesh.Get(), Transform::identity, coneTransform);
    axisMesh->SetName(NAME("TranslateGizmo_AxisMesh"));
    InitObject(axisMesh);

    Handle<Mesh> centroidMesh = MeshBuilder::Cube();
    centroidMesh->SetName(NAME("TranslateGizmo_CentroidMesh"));
    InitObject(centroidMesh);

    static const Vec4f s_axisColors[3] = {
        Vec4f(1.0f, 0.2f, 0.2f, 1.0f),
        Vec4f(0.2f, 1.0f, 0.2f, 1.0f),
        Vec4f(0.2f, 0.2f, 1.0f, 1.0f)
    };

    static const Quat4f s_axisRotations[3] = {
        Quat4f::AxisAngles(Vec3f::UnitZ(), -MathUtil::pi<float> * 0.5f),
        Quat4f::Identity(),
        Quat4f::AxisAngles(Vec3f::UnitX(), MathUtil::pi<float> * 0.5f)
    };

    static const char* s_axisNames[3] = { "TranslateGizmo_AxisX", "TranslateGizmo_AxisY", "TranslateGizmo_AxisZ" };

    Handle<Node> rootNode = MakeHandle<Node>();
    rootNode->SetName(NAME("TranslateGizmo"));
    rootNode->UnlockTransform();
    rootNode->SetWorldScale(2.5f);
    rootNode->SetNodeFlags(rootNode->GetNodeFlags() | NodeFlags::HideInSceneOutline);
    rootNode->SetIsTransient(false);

    for (int i = 0; i < 3; i++)
    {
        rootNode->AddChild(CreateAxisEntity(s_axisNames[i], axisMesh, s_axisColors[i], i, s_axisRotations[i]));
    }

    const Vec4f centroidColor(0.8f, 0.8f, 0.8f, 1.0f);
    rootNode->AddChild(CreateCentroidEntity("TranslateGizmo_Centroid", centroidMesh, centroidColor));

    GetCurrentAssetRegistry()->PutAssetsDeep(rootNode);

    HYP_LOG(Engine, Info, "TranslateGizmo built and registered successfully.");
}

static void BuildRotateGizmo(Handle<AssetRegistry>& assetRegistry)
{
    GlobalContextScope assetRegistryScope { AssetRegistryContext { assetRegistry } };

    if (Handle<Node> existingNode = GetCurrentAssetRegistry()->GetAsset<Node>(AssetBuckets::Nodes, "RotateGizmo"_sh); existingNode.IsValid())
    {
        HYP_LOG(Engine, Info, "RotateGizmo already exists in editor asset registry, skipping build.");
        return;
    }

    Handle<Mesh> torusMesh = MeshBuilder::Torus(0.8f, 0.03f, 48, 16);
    torusMesh->SetName(NAME("RotateGizmo_TorusMesh"));
    InitObject(torusMesh);

    static const Vec4f s_axisColors[3] = {
        Vec4f(1.0f, 0.2f, 0.2f, 1.0f),
        Vec4f(0.2f, 1.0f, 0.2f, 1.0f),
        Vec4f(0.2f, 0.2f, 1.0f, 1.0f)
    };

    static const Quat4f s_axisRotations[3] = {
        Quat4f::AxisAngles(Vec3f::UnitZ(), MathUtil::pi<float> * 0.5f),
        Quat4f::Identity(),
        Quat4f::AxisAngles(Vec3f::UnitX(), MathUtil::pi<float> * 0.5f)
    };

    static const char* s_axisNames[3] = { "RotateGizmo_AxisX", "RotateGizmo_AxisY", "RotateGizmo_AxisZ" };

    Handle<Node> rootNode = MakeHandle<Node>();
    rootNode->SetName(NAME("RotateGizmo"));
    rootNode->UnlockTransform();
    rootNode->SetWorldScale(2.5f);
    rootNode->SetNodeFlags(rootNode->GetNodeFlags() | NodeFlags::HideInSceneOutline);
    rootNode->SetIsTransient(false);

    for (int i = 0; i < 3; i++)
    {
        rootNode->AddChild(CreateAxisEntity(s_axisNames[i], torusMesh, s_axisColors[i], i, s_axisRotations[i]));
    }

    GetCurrentAssetRegistry()->PutAssetsDeep(rootNode);

    HYP_LOG(Engine, Info, "RotateGizmo built and registered successfully.");
}

static void BuildScaleGizmo(Handle<AssetRegistry>& assetRegistry)
{
    GlobalContextScope assetRegistryScope { AssetRegistryContext { assetRegistry } };

    if (Handle<Node> existingNode = GetCurrentAssetRegistry()->GetAsset<Node>(AssetBuckets::Nodes, "ScaleGizmo"_sh); existingNode.IsValid())
    {
        HYP_LOG(Engine, Info, "ScaleGizmo already exists in editor asset registry, skipping build.");
        return;
    }

    Handle<Mesh> shaftMesh = MeshBuilder::Cylinder(0.03f, 0.7f, 16);
    shaftMesh->SetName(NAME("ScaleGizmo_ShaftMesh"));
    InitObject(shaftMesh);

    Handle<Mesh> handleCubeMesh = MeshBuilder::Cube();
    handleCubeMesh->SetName(NAME("ScaleGizmo_HandleCubeMesh"));
    InitObject(handleCubeMesh);

    Transform handleTransform;
    handleTransform.translation = Vec3f(0.0f, 0.35f + 0.06f, 0.0f);
    handleTransform.scale = Vec3f(0.12f);

    Handle<Mesh> axisMesh = MeshBuilder::Merge(shaftMesh.Get(), handleCubeMesh.Get(), Transform::identity, handleTransform);
    axisMesh->SetName(NAME("ScaleGizmo_AxisMesh"));
    InitObject(axisMesh);

    Handle<Mesh> centroidMesh = MeshBuilder::Cube();
    centroidMesh->SetName(NAME("ScaleGizmo_CentroidMesh"));
    InitObject(centroidMesh);

    static const Vec4f s_axisColors[3] = {
        Vec4f(1.0f, 0.2f, 0.2f, 1.0f),
        Vec4f(0.2f, 1.0f, 0.2f, 1.0f),
        Vec4f(0.2f, 0.2f, 1.0f, 1.0f)
    };

    static const Quat4f s_axisRotations[3] = {
        Quat4f::AxisAngles(Vec3f::UnitZ(), -MathUtil::pi<float> * 0.5f),
        Quat4f::Identity(),
        Quat4f::AxisAngles(Vec3f::UnitX(), MathUtil::pi<float> * 0.5f)
    };

    static const char* s_axisNames[3] = { "ScaleGizmo_AxisX", "ScaleGizmo_AxisY", "ScaleGizmo_AxisZ" };

    Handle<Node> rootNode = MakeHandle<Node>();
    rootNode->SetName(NAME("ScaleGizmo"));
    rootNode->UnlockTransform();
    rootNode->SetWorldScale(2.5f);
    rootNode->SetNodeFlags(rootNode->GetNodeFlags() | NodeFlags::HideInSceneOutline);
    rootNode->SetIsTransient(false);

    for (int i = 0; i < 3; i++)
    {
        rootNode->AddChild(CreateAxisEntity(s_axisNames[i], axisMesh, s_axisColors[i], i, s_axisRotations[i]));
    }

    const Vec4f centroidColor(0.8f, 0.8f, 0.8f, 1.0f);
    rootNode->AddChild(CreateCentroidEntity("ScaleGizmo_Centroid", centroidMesh, centroidColor));

    GetCurrentAssetRegistry()->PutAssetsDeep(rootNode);

    HYP_LOG(Engine, Info, "ScaleGizmo built and registered successfully.");
}

class BuildGizmosCommandlet : public CommandletBase
{
    HYP_OBJECT_BODY(BuildGizmosCommandlet);

public:
    virtual ~BuildGizmosCommandlet() override = default;

protected:
    virtual Result Run_Impl(const CommandLineArguments& args) override
    {
        Handle<AssetRegistry> editorRegistry = GetEditorAssetRegistry();

        if (!editorRegistry.IsValid())
        {
            return HYP_MAKE_ERROR(Error, "Editor asset registry is not available");
        }

        BuildTranslateGizmo(editorRegistry);
        BuildRotateGizmo(editorRegistry);
        BuildScaleGizmo(editorRegistry);

        GlobalContextScope assetRegistryScope { AssetRegistryContext { editorRegistry } };
        GetCurrentAssetRegistry()->SaveDirtyAssets();

        HYP_LOG(Engine, Info, "All gizmos built and saved.");

        return {};
    }
};

HYP_API const Class* g_clsBuildGizmosCommandlet = nullptr;

HYP_BEGIN_CLASS(BuildGizmosCommandlet, -1, 0, NAME("CommandletBase"), ClassAttribute("command", "buildgizmos"))
HYP_END_CLASS

HYP_REGISTER_STATIC_CLASS(BuildGizmosCommandlet);

} // namespace Hyperion
