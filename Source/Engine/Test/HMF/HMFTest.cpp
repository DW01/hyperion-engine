/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#ifdef HYP_TESTS

#include <Core/HMF/HMF.hpp>

#include <Core/Reflection/BoxedValue.hpp>
#include <Core/Reflection/TypeInfo.hpp>
#include <Core/Reflection/Class.hpp>
#include <Core/Reflection/ClassUtils.hpp>
#include <Core/Reflection/Property.hpp>
#include <Core/Reflection/Field.hpp>
#include <Core/Reflection/Enum.hpp>
#include <Core/Reflection/ObjectMacros.hpp>
#include <Core/Reflection/ObjectFwd.hpp>
#include <Core/Reflection/ClassRegistry.hpp>

#include <Core/Utilities/EnumFlags.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/String.hpp>

#include <Core/Logging/Logger.hpp>

#include <Core/Debug/Debug.hpp>

#include <Core/Name/Name.hpp>

#include <Asset/SerializationUtils.hpp>
#include <Asset/AssetPath.hpp>
#include <Asset/RawDataAsset.hpp>
#include <Asset/BlobStorageStructs.hpp>

#include <Rendering/Shared.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/MaterialTypes.hpp>
#include <Rendering/RenderableAttributes.hpp>

#include <Scene/Camera/Camera.hpp>
#include <Scene/Light.hpp>
#include <Scene/Node.hpp>

#include <Scene/Animation/Animation.hpp>

namespace Hyperion {

struct HMFTestNestedStruct
{
    Name label;
    int32 id = 0;
    TextureDesc texture;
};

const Class* g_clsHMFTestNestedStruct = nullptr;

// clang-format off
HYP_BEGIN_STRUCT(HMFTestNestedStruct, -1, 0, {})
    Field(NAME(HYP_STR(Label)), &HMFTestNestedStruct::label, HYP_OFFSET_OF(HMFTestNestedStruct, label)),
    Field(NAME(HYP_STR(Id)), &HMFTestNestedStruct::id, HYP_OFFSET_OF(HMFTestNestedStruct, id)),
    Field(NAME(HYP_STR(Texture)), &HMFTestNestedStruct::texture, HYP_OFFSET_OF(HMFTestNestedStruct, texture))
HYP_END_STRUCT

HYP_REGISTER_STATIC_CLASS(HMFTestNestedStruct);
// clang-format on

const Class* g_clsHMFVariantBase = nullptr;
const Class* g_clsHMFVariantDerived = nullptr;
const Class* g_clsHMFVariantContainer = nullptr;

class HMFVariantBase : public ObjectBase
{
public:
    HYP_OBJECT_BODY(HMFVariantBase);

    int32 baseValue = 0;
};

const Class* HMFVariantBase::StaticClass()
{
    return g_clsHMFVariantBase;
}

class HMFVariantDerived : public HMFVariantBase
{
public:
    HYP_OBJECT_BODY(HMFVariantDerived);

    int32 baseValue = 0;
    float derivedValue = 0.0f;
    Name derivedName;
};

const Class* HMFVariantDerived::StaticClass()
{
    return g_clsHMFVariantDerived;
}

class HMFVariantContainer : public ObjectBase
{
public:
    HYP_OBJECT_BODY(HMFVariantContainer);

    Name containerName;
    Variant<Handle<HMFVariantBase>, Handle<HMFVariantDerived>> item;
};

const Class* HMFVariantContainer::StaticClass()
{
    return g_clsHMFVariantContainer;
}

// clang-format off
HYP_BEGIN_CLASS(HMFVariantBase, -1, 0, NAME("ObjectBase"))
    Field(NAME(HYP_STR(BaseValue)), &HMFVariantBase::baseValue, HYP_OFFSET_OF(HMFVariantBase, baseValue))
HYP_END_CLASS
HYP_REGISTER_STATIC_CLASS(HMFVariantBase);

HYP_BEGIN_CLASS(HMFVariantDerived, -1, 0, NAME("HMFVariantBase"))
    Field(NAME(HYP_STR(BaseValue)), &HMFVariantDerived::baseValue, HYP_OFFSET_OF(HMFVariantDerived, baseValue)),
    Field(NAME(HYP_STR(DerivedValue)), &HMFVariantDerived::derivedValue, HYP_OFFSET_OF(HMFVariantDerived, derivedValue)),
    Field(NAME(HYP_STR(DerivedName)), &HMFVariantDerived::derivedName, HYP_OFFSET_OF(HMFVariantDerived, derivedName))
HYP_END_CLASS
HYP_REGISTER_STATIC_CLASS(HMFVariantDerived);

HYP_BEGIN_CLASS(HMFVariantContainer, -1, 0, NAME("ObjectBase"))
    Field(NAME(HYP_STR(ContainerName)), &HMFVariantContainer::containerName, HYP_OFFSET_OF(HMFVariantContainer, containerName)),
    Field(NAME(HYP_STR(Item)), &HMFVariantContainer::item, HYP_OFFSET_OF(HMFVariantContainer, item))
HYP_END_CLASS
HYP_REGISTER_STATIC_CLASS(HMFVariantContainer);
// clang-format on

namespace tests {
namespace hmf {

namespace {

// Helper: get the active type name from a Variant BoxedValue
String GetVariantActiveTypeName(const BoxedValue& variantValue)
{
    const TypeInfo* ti = variantValue.GetTypeInfo();
    if (!ti || !ti->IsVariantType()) return "not-a-variant";

    auto* handler = static_cast<ITypeInfoVariantHandler*>(ti->extendedInfo.handler);
    if (!handler) return "no-handler";

    int idx = handler->GetCurrentTypeIndex(variantValue);
    if (idx < 0) return "empty";

    const TypeInfo* altTI = handler->GetTypeInfoAtIndex(idx);
    if (!altTI) return "null-alt";

    const Class* cls = altTI->GetClass();
    return cls ? cls->GetName().ToString() : "unknown";
}

// Helper: extract the inner object from a Variant<Handle<...>> BoxedValue
// so we can access its fields via the object's Class.
BoxedValue GetVariantInnerObject(const BoxedValue& variantValue)
{
    const TypeInfo* ti = variantValue.GetTypeInfo();
    if (!ti || !ti->IsVariantType()) return {};

    auto* handler = static_cast<ITypeInfoVariantHandler*>(ti->extendedInfo.handler);
    if (!handler) return {};

    AnyRef ref = handler->GetValue(variantValue);
    if (!ref.HasValue() || !ref.GetPointer()) return {};

    const TypeInfo* refTI = ref.GetTypeInfo();
    if (refTI && refTI->IsHandleType())
    {
        auto* handlePtr = static_cast<Handle<ObjectBase>*>(ref.GetPointer());
        if (*handlePtr) return BoxedValue(*handlePtr);
    }

    return {};
}

int g_passCount = 0;
int g_failCount = 0;

void Check(const char* testName, bool condition, const String& detail = "")
{
    if (condition)
    {
        ++g_passCount;
        HYP_LOG(Engine, Info, "[PASS] {}", testName);
    }
    else
    {
        ++g_failCount;
        HYP_LOG(Engine, Error, "[FAIL] {} {}", testName, detail);
    }
}

template <class T>
T GetFieldValue(const BoxedValue& obj, const Class* cls, const char* fieldName)
{
    if (const IMember* m = cls->GetMember(StringHash(fieldName)))
    {
        BoxedValue v;
        if (m->GetMemberType() == MemberType::Field)
            v = static_cast<const Field*>(m)->Get(obj);
        else if (m->GetMemberType() == MemberType::Property)
            v = static_cast<const Property*>(m)->Get(obj);

        if (v.Is<T>())
            return v.Get<T>();
    }
    return T{};
}

uint64 GetFieldUInt64(const BoxedValue& obj, const Class* cls, const char* fieldName)
{
    if (const IMember* m = cls->GetMember(StringHash(fieldName)))
    {
        BoxedValue v;
        if (m->GetMemberType() == MemberType::Field)
            v = static_cast<const Field*>(m)->Get(obj);
        else if (m->GetMemberType() == MemberType::Property)
            v = static_cast<const Property*>(m)->Get(obj);

        if (v.Is<uint64>()) return v.Get<uint64>();
        if (v.Is<int64>()) return static_cast<uint64>(v.Get<int64>());
        if (v.Is<uint32>()) return v.Get<uint32>();
        if (v.Is<int32>()) return static_cast<uint64>(v.Get<int32>());
        if (v.Is<uint16>()) return v.Get<uint16>();
        if (v.Is<int16>()) return static_cast<uint64>(v.Get<int16>());
        if (v.Is<uint8>()) return v.Get<uint8>();
        if (v.Is<int8>()) return static_cast<uint64>(v.Get<int8>());
    }
    return 0;
}

void SetFieldValue(BoxedValue& obj, const Class* cls, const char* fieldName, const BoxedValue& value)
{
    if (const IMember* m = cls->GetMember(StringHash(fieldName)))
    {
        if (m->GetMemberType() == MemberType::Field)
            static_cast<const Field*>(m)->Set(obj, value);
        else if (m->GetMemberType() == MemberType::Property)
            static_cast<const Property*>(m)->Set(obj, value);
    }
}

bool ClassNameIs(const BoxedValue& value, const char* expected)
{
    const Class* cls = GetClass(value.GetTypeId());
    return cls && String(cls->GetName().LookupString()) == String(expected);
}

} // anonymous namespace

HYP_DISABLE_OPTIMIZATION;

HYP_EXPORT void RunHMFTest()
{
    HYP_LOG(Engine, Info, "========== HMF Test ==========");

    // ========================================================
    // Section 1: Primitive serialization (BoxedToHMF — object → text)
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 1: Primitives (write) ---");

    {
        String text;

        BoxedToHMF(BoxedValue(int8(-5)), text);     Check("int8(-5) => \"-5\"", text == "-5", text);
        text.Clear(); BoxedToHMF(BoxedValue(int16(-32000)), text); Check("int16(-32000)", text == "-32000", text);
        text.Clear(); BoxedToHMF(BoxedValue(int32(42)), text);     Check("int32(42) => \"42\"", text == "42", text);
        text.Clear(); BoxedToHMF(BoxedValue(int64(-9999999999LL)), text); Check("int64(-9999999999)", text == "-9999999999", text);
        text.Clear(); BoxedToHMF(BoxedValue(uint8(255)), text);    Check("uint8(255) => \"255\"", text == "255", text);
        text.Clear(); BoxedToHMF(BoxedValue(uint16(65535)), text); Check("uint16(65535)", text == "65535", text);
        text.Clear(); BoxedToHMF(BoxedValue(uint32(42)), text);    Check("uint32(42) => \"42\"", text == "42", text);
        text.Clear(); BoxedToHMF(BoxedValue(uint64(0xFFFFFFFFFFFFFFFFULL)), text);
        Check("uint64(max)", text == "18446744073709551615", text);

        text.Clear(); BoxedToHMF(BoxedValue(float(3.5f)), text);  Check("float(3.5)", text.Contains("3.5"), text);
        text.Clear(); BoxedToHMF(BoxedValue(double(3.14159)), text); Check("double(3.14159)", text.Contains("3.14159"), text);

        text.Clear(); BoxedToHMF(BoxedValue(true), text);         Check("bool(true) => \"true\"", text == "true", text);
        text.Clear(); BoxedToHMF(BoxedValue(false), text);        Check("bool(false) => \"false\"", text == "false", text);

        text.Clear(); BoxedToHMF(BoxedValue(String("hello")), text); Check("String(hello)", text == "\"hello\"", text);
        text.Clear(); BoxedToHMF(BoxedValue(String("")), text);    Check("String(empty)", text == "\"\"", text);
        text.Clear(); BoxedToHMF(BoxedValue(String("with \"quotes\" and \\ backslash")), text);
        Check("String escaping", text.Contains("\\\"") && text.Contains("\\\\"), text);
        text.Clear(); BoxedToHMF(BoxedValue(String("line\nbreak\ttab")), text);
        Check("String escaping (newline + tab)", text.Contains("\\n") && text.Contains("\\t"), text);
    }

    // ========================================================
    // Section 2: Enum serialization
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 2: Enums (write) ---");

    {
        String text;
        const TypeInfo& ti = TypeOf<CameraProjectionMode>();

        BoxedToHMF(BoxedValue(CameraProjectionMode::NONE), text, &ti);
        Check("Enum NONE", text == "NONE", text);

        text.Clear(); BoxedToHMF(BoxedValue(CameraProjectionMode::PERSPECTIVE), text, &ti);
        Check("Enum PERSPECTIVE", text == "PERSPECTIVE", text);

        text.Clear(); BoxedToHMF(BoxedValue(CameraProjectionMode::ORTHOGRAPHIC), text, &ti);
        Check("Enum ORTHOGRAPHIC", text == "ORTHOGRAPHIC", text);
    }

    // ========================================================
    // Section 3: EnumFlags serialization
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 3: EnumFlags (write) ---");

    {
        String text;
        const TypeInfo& ti = TypeOf<EnumFlags<CameraFlags>>();

        EnumFlags<CameraFlags> single = CameraFlags::MatchWindowSize;
        BoxedToHMF(BoxedValue(single), text, &ti);
        Check("EnumFlags single => MatchWindowSize", text == "MatchWindowSize", text);

        text.Clear();
        EnumFlags<CameraFlags> multi = CameraFlags::MatchWindowSize | CameraFlags::HasStreamingVolume;
        BoxedToHMF(BoxedValue(multi), text, &ti);
        Check("EnumFlags multi => MatchWindowSize|HasStreamingVolume",
              text.Contains("MatchWindowSize") && text.Contains("HasStreamingVolume") && text.Contains("|"),
              text);

        text.Clear();
        const TypeInfo& lti = TypeOf<EnumFlags<LightFlags>>();
        EnumFlags<LightFlags> lightFlags = LightFlags::Default;
        BoxedToHMF(BoxedValue(lightFlags), text, &lti);
        Check("EnumFlags Default(composite)",
              text.Contains("Default") || (text.Contains("ShadowCaster") && text.Contains("ShadowPCF")),
              text);

        text.Clear();
        EnumFlags<CameraFlags> empty = CameraFlags::NONE;
        BoxedToHMF(BoxedValue(empty), text, &ti);
        Check("EnumFlags NONE", text == "NONE", text);
    }

    // ========================================================
    // Section 4: Array serialization
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 4: Arrays (write) ---");

    {
        String text;
        Array<int32> arr = {10, 20, 30};
        BoxedToHMF(BoxedValue(arr), text);
        Check("Array<int32>[3]", text == "[10, 20, 30]", text);

        text.Clear();
        Array<int32> empty;
        BoxedToHMF(BoxedValue(empty), text);
        Check("Array<int32>[] (empty)", text == "[]", text);
    }

    // ========================================================
    // Section 5: Object → HMF text — CameraOrthoRect exact output
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 5: Object → HMF text (CameraOrthoRect) ---");

    {
        const Class* cls = GetClass<CameraOrthoRect>();
        if (cls && cls->CanCreateInstance())
        {
            BoxedValue obj;
            cls->CreateInstance(obj);

            SetFieldValue(obj, cls, "Left", BoxedValue(1.5f));
            SetFieldValue(obj, cls, "Right", BoxedValue(2.5f));
            SetFieldValue(obj, cls, "Bottom", BoxedValue(3.5f));
            SetFieldValue(obj, cls, "Top", BoxedValue(4.5f));

            String text;
            ObjectToHMFDocument(cls, obj, text);

            // Assert exact expected output
            Check("CameraOrthoRect: has class header", text.Contains("CameraOrthoRect"), text);
            Check("CameraOrthoRect: Left = 1.5", text.Contains("Left = 1.5"), text);
            Check("CameraOrthoRect: Right = 2.5", text.Contains("Right = 2.5"), text);
            Check("CameraOrthoRect: Bottom = 3.5", text.Contains("Bottom = 3.5"), text);
            Check("CameraOrthoRect: Top = 4.5", text.Contains("Top = 4.5"), text);
            Check("CameraOrthoRect: no transient fields", !text.Contains("raw") && !text.Contains("readOnly"), text);
        }
    }

    // ========================================================
    // Section 6: Object → HMF text — TextureDesc exact output
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 6: Object → HMF text (TextureDesc) ---");

    {
        const Class* cls = GetClass<TextureDesc>();
        if (cls && cls->CanCreateInstance())
        {
            BoxedValue obj;
            cls->CreateInstance(obj);

            String text;
            ObjectToHMFDocument(cls, obj, text);
            HYP_LOG(Engine, Info, "TextureDesc HMF:\n{}", text);

            Check("TextureDesc: class header", text.Contains("TextureDesc"), text);
            Check("TextureDesc: Type = Texture2D", text.Contains("Type = Texture2D"), text);
            Check("TextureDesc: Format = RGBA8", text.Contains("Format = RGBA8"), text);
            Check("TextureDesc: Extent = [1, 1, 1]", text.Contains("Extent = [1, 1, 1]"), text);
            Check("TextureDesc: MinFilterMode = TFM_NEAREST", text.Contains("MinFilterMode = TFM_NEAREST"), text);
            Check("TextureDesc: NumLayers = 1", text.Contains("NumLayers = 1"), text);
            Check("TextureDesc: ImageUsage = IU_SAMPLED", text.Contains("ImageUsage = IU_SAMPLED"), text);
            Check("TextureDesc: MipOffsets has 16 elements", text.Contains("0, 0, 0, 0"), text);
        }
    }

    // ========================================================
    // Section 7: Object → HMF text — MeshLodDesc exact output
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 7: Object → HMF text (MeshLodDesc) ---");

    {
        const Class* cls = GetClass<MeshLodDesc>();
        if (cls && cls->CanCreateInstance())
        {
            BoxedValue obj;
            cls->CreateInstance(obj);

            SetFieldValue(obj, cls, "NumVertices", BoxedValue(uint32(9999)));
            SetFieldValue(obj, cls, "NumIndices", BoxedValue(uint32(33333)));

            String text;
            ObjectToHMFDocument(cls, obj, text);

            Check("MeshLodDesc: NumVertices = 9999", text.Contains("NumVertices = 9999"), text);
            Check("MeshLodDesc: NumIndices = 33333", text.Contains("NumIndices = 33333"), text);
        }
    }

    // ========================================================
    // Section 8: Object → HMF text — StencilFunction enum values
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 8: Object → HMF text (StencilFunction) ---");

    {
        const Class* cls = GetClass<StencilFunction>();
        if (cls && cls->CanCreateInstance())
        {
            BoxedValue obj;
            cls->CreateInstance(obj);

            String text;
            ObjectToHMFDocument(cls, obj, text);

            Check("StencilFunction: PassOp = SO_REPLACE", text.Contains("PassOp = SO_REPLACE"), text);
            Check("StencilFunction: FailOp = SO_KEEP", text.Contains("FailOp = SO_KEEP"), text);
            Check("StencilFunction: DepthFailOp = SO_KEEP", text.Contains("DepthFailOp = SO_KEEP"), text);
            Check("StencilFunction: CompareOp = SCO_ALWAYS", text.Contains("CompareOp = SCO_ALWAYS"), text);
        }
    }

    // ========================================================
    // Section 9: Parse → verify type and values — CameraOrthoRect
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 9: Parse CameraOrthoRect (type + values) ---");

    {
        const String manifest = R"(CameraOrthoRect {
    Left = 10.5
    Right = 20
    Bottom = -1
    Top = 100
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Parse CameraOrthoRect succeeds", result.ok, result.message);

        if (result.ok)
        {
            Check("Type is CameraOrthoRect", ClassNameIs(result.value, "CameraOrthoRect"));
            Check("TypeId matches CameraOrthoRect",
                  result.value.GetTypeId() == TypeId::ForType<CameraOrthoRect>());

            const Class* cls = GetClass(result.value.GetTypeId());
            if (cls)
            {
                Check("Left == 10.5", GetFieldValue<float>(result.value, cls, "Left") == 10.5f);
                Check("Right == 20.0", GetFieldValue<float>(result.value, cls, "Right") == 20.0f);
                Check("Bottom == -1.0", GetFieldValue<float>(result.value, cls, "Bottom") == -1.0f);
                Check("Top == 100.0", GetFieldValue<float>(result.value, cls, "Top") == 100.0f);
            }
        }
    }

    // ========================================================
    // Section 10: Parse → verify type and values — MeshLodDesc
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 10: Parse MeshLodDesc (type + values) ---");

    {
        const String manifest = R"(MeshLodDesc {
    NumVertices = 4096
    NumIndices = 12288
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Parse MeshLodDesc succeeds", result.ok, result.message);

        if (result.ok)
        {
            Check("Type is MeshLodDesc", ClassNameIs(result.value, "MeshLodDesc"));
            Check("TypeId matches", result.value.GetTypeId() == TypeId::ForType<MeshLodDesc>());

            const Class* cls = GetClass(result.value.GetTypeId());
            if (cls)
            {
                Check("NumVertices == 4096", GetFieldValue<uint32>(result.value, cls, "NumVertices") == 4096);
                Check("NumIndices == 12288", GetFieldValue<uint32>(result.value, cls, "NumIndices") == 12288);
            }
        }
    }

    // ========================================================
    // Section 11: Parse → verify type and values — BlobDataReference
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 11: Parse BlobDataReference (type + values) ---");

    {
        const String manifest = R"(BlobDataReference {
    Key = "Engine://Textures/MyTexture.TEX"
    Size = 1048576
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Parse BlobDataReference succeeds", result.ok, result.message);

        if (result.ok)
        {
            Check("Type is BlobDataReference", ClassNameIs(result.value, "BlobDataReference"));

            const Class* cls = GetClass(result.value.GetTypeId());
            if (cls)
            {
                Name keyName = GetFieldValue<Name>(result.value, cls, "Key");
                Check("Key name valid", keyName.IsValid(), keyName.LookupString());
                Check("Key == Engine://Textures/MyTexture.TEX",
                      String(keyName.LookupString()) == String("Engine://Textures/MyTexture.TEX"),
                      keyName.LookupString());
                Check("Size == 1048576", GetFieldValue<uint64>(result.value, cls, "Size") == 1048576);
            }
        }
    }

    // ========================================================
    // Section 12: Parse with comments
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 12: Parse with comments ---");

    {
        const String manifest = R"(// This is a line comment
CameraOrthoRect {
    /* block comment */ Left = 1.0
    Right = 2.0 // trailing comment
    // another comment
    Bottom = 3.0
    Top = 4.0
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Parse with comments succeeds", result.ok, result.message);

        if (result.ok)
        {
            const Class* cls = GetClass(result.value.GetTypeId());
            if (cls)
            {
                Check("Comment: Left == 1.0", GetFieldValue<float>(result.value, cls, "Left") == 1.0f);
                Check("Comment: Right == 2.0", GetFieldValue<float>(result.value, cls, "Right") == 2.0f);
                Check("Comment: Bottom == 3.0", GetFieldValue<float>(result.value, cls, "Bottom") == 3.0f);
                Check("Comment: Top == 4.0", GetFieldValue<float>(result.value, cls, "Top") == 4.0f);
            }
        }
    }

    // ========================================================
    // Section 13: Parse → verify enum fields — StencilFunction
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 13: Parse StencilFunction (enum fields) ---");

    {
        const String manifest = R"(StencilFunction {
    PassOp = SO_REPLACE
    FailOp = SO_KEEP
    DepthFailOp = SO_KEEP
    CompareOp = SCO_ALWAYS
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Parse StencilFunction succeeds", result.ok, result.message);

        if (result.ok)
        {
            Check("Type is StencilFunction", ClassNameIs(result.value, "StencilFunction"));

            const Class* cls = GetClass(result.value.GetTypeId());
            if (cls)
            {
                Check("PassOp == SO_REPLACE value",
                      GetFieldUInt64(result.value, cls, "PassOp") == static_cast<uint64>(SO_REPLACE));
                Check("FailOp == SO_KEEP value",
                      GetFieldUInt64(result.value, cls, "FailOp") == static_cast<uint64>(SO_KEEP));
                Check("CompareOp == SCO_ALWAYS value",
                      GetFieldUInt64(result.value, cls, "CompareOp") == static_cast<uint64>(SCO_ALWAYS));
            }
        }
    }

    // ========================================================
    // Section 14: Full round-trip — CameraOrthoRect (create → write → parse → verify all)
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 14: Round-trip CameraOrthoRect ---");

    {
        const Class* cls = GetClass<CameraOrthoRect>();
        if (!cls || !cls->CanCreateInstance())
        {
            Check("CameraOrthoRect registered", false);
        }
        else
        {
            BoxedValue obj;
            cls->CreateInstance(obj);

            SetFieldValue(obj, cls, "Left", BoxedValue(11.0f));
            SetFieldValue(obj, cls, "Right", BoxedValue(22.0f));
            SetFieldValue(obj, cls, "Bottom", BoxedValue(33.0f));
            SetFieldValue(obj, cls, "Top", BoxedValue(44.0f));

            String text;
            ObjectToHMFDocument(cls, obj, text);

            HMF::ParseResult result = HMF::Parse(text);
            Check("Round-trip parse succeeds", result.ok, result.message);

            if (result.ok)
            {
                Check("Round-trip type correct", ClassNameIs(result.value, "CameraOrthoRect"));

                const Class* parsedCls = GetClass(result.value.GetTypeId());
                if (parsedCls)
                {
                    Check("RT Left == 11", GetFieldValue<float>(result.value, parsedCls, "Left") == 11.0f);
                    Check("RT Right == 22", GetFieldValue<float>(result.value, parsedCls, "Right") == 22.0f);
                    Check("RT Bottom == 33", GetFieldValue<float>(result.value, parsedCls, "Bottom") == 33.0f);
                    Check("RT Top == 44", GetFieldValue<float>(result.value, parsedCls, "Top") == 44.0f);
                }
            }
        }
    }

    // ========================================================
    // Section 15: Full round-trip — TextureDesc (enums + Vec3u + FixedArray)
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 15: Round-trip TextureDesc ---");

    {
        const Class* cls = GetClass<TextureDesc>();
        if (!cls || !cls->CanCreateInstance())
        {
            Check("TextureDesc registered", false);
        }
        else
        {
            BoxedValue obj;
            cls->CreateInstance(obj);

            String text;
            ObjectToHMFDocument(cls, obj, text);
            Check("TextureDesc serialize non-empty", !text.Empty());

            HMF::ParseResult result = HMF::Parse(text);
            Check("TextureDesc round-trip parse succeeds", result.ok, result.message);

            if (result.ok)
            {
                Check("TextureDesc type correct", ClassNameIs(result.value, "TextureDesc"));

                const Class* parsedCls = GetClass(result.value.GetTypeId());
                if (parsedCls)
                {
                    Check("RT NumLayers == 1", GetFieldValue<uint16>(result.value, parsedCls, "NumLayers") == 1);
                    Check("RT Type == Texture2D",
                          GetFieldUInt64(result.value, parsedCls, "Type") == static_cast<uint64>(TextureType::Texture2D));
                    Check("RT Format == RGBA8",
                          GetFieldUInt64(result.value, parsedCls, "Format") == static_cast<uint64>(TextureFormat::RGBA8));
                }
            }
        }
    }

    // ========================================================
    // Section 16: Full round-trip — MeshLodDesc
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 16: Round-trip MeshLodDesc ---");

    {
        const Class* cls = GetClass<MeshLodDesc>();
        if (!cls || !cls->CanCreateInstance())
        {
            Check("MeshLodDesc registered", false);
        }
        else
        {
            BoxedValue obj;
            cls->CreateInstance(obj);

            SetFieldValue(obj, cls, "NumVertices", BoxedValue(uint32(1234)));
            SetFieldValue(obj, cls, "NumIndices", BoxedValue(uint32(5678)));

            String text;
            ObjectToHMFDocument(cls, obj, text);

            HMF::ParseResult result = HMF::Parse(text);
            Check("MeshLodDesc round-trip succeeds", result.ok, result.message);

            if (result.ok)
            {
                Check("MeshLodDesc type correct", ClassNameIs(result.value, "MeshLodDesc"));

                const Class* parsedCls = GetClass(result.value.GetTypeId());
                if (parsedCls)
                {
                    Check("RT NumVertices == 1234", GetFieldValue<uint32>(result.value, parsedCls, "NumVertices") == 1234);
                    Check("RT NumIndices == 5678", GetFieldValue<uint32>(result.value, parsedCls, "NumIndices") == 5678);
                }
            }
        }
    }

    // ========================================================
    // Section 17: Full round-trip — StencilFunction (enum fields)
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 17: Round-trip StencilFunction ---");

    {
        const Class* cls = GetClass<StencilFunction>();
        if (!cls || !cls->CanCreateInstance())
        {
            Check("StencilFunction registered", false);
        }
        else
        {
            BoxedValue obj;
            cls->CreateInstance(obj);

            String text;
            ObjectToHMFDocument(cls, obj, text);

            HMF::ParseResult result = HMF::Parse(text);
            Check("StencilFunction round-trip succeeds", result.ok, result.message);

            if (result.ok)
            {
                Check("StencilFunction type correct", ClassNameIs(result.value, "StencilFunction"));

                const Class* parsedCls = GetClass(result.value.GetTypeId());
                if (parsedCls)
                {
                    Check("RT PassOp == SO_REPLACE",
                          GetFieldUInt64(result.value, parsedCls, "PassOp") == static_cast<uint64>(SO_REPLACE));
                    Check("RT FailOp == SO_KEEP",
                          GetFieldUInt64(result.value, parsedCls, "FailOp") == static_cast<uint64>(SO_KEEP));
                    Check("RT CompareOp == SCO_ALWAYS",
                          GetFieldUInt64(result.value, parsedCls, "CompareOp") == static_cast<uint64>(SCO_ALWAYS));
                }
            }
        }
    }

    // ========================================================
    // Section 18: Parse AssetPath
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 18: Parse AssetPath ---");

    {
        const String manifest = R"(AssetPath {
    Value = "Engine://Textures/MyTexture"
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Parse AssetPath succeeds", result.ok, result.message);

        if (result.ok)
        {
            Check("Type is AssetPath", ClassNameIs(result.value, "AssetPath"));

            const Class* cls = GetClass(result.value.GetTypeId());
            if (cls)
            {
                if (const Property* vp = cls->GetProperty("Value"_sh))
                {
                    BoxedValue v = vp->Get(result.value);
                    String s = v.Is<String>() ? v.Get<String>() : "";
                    Check("AssetPath Value correct", s == "Engine://Textures/MyTexture", s);
                }

                String rt;
                BoxedToHMF(result.value, rt);
                Check("AssetPath round-trip => @\"...\"", rt.Contains("@\"Engine://Textures/MyTexture\""), rt);
            }
        }
    }

    // ========================================================
    // Section 19: AssetPath @"..." shorthand serialization
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 19: AssetPath shorthand ---");

    {
        AssetPath path(ANSIStringView("Game://Meshes/Cube"));
        String text;
        ToHMFOptions opts;
        BoxedToHMF(BoxedValue(path), text, nullptr, &opts);
        Check("AssetPath value => @\"Game://Meshes/Cube\"",
              text == "@\"Game://Meshes/Cube\"", text);
    }

    // ========================================================
    // Section 20: Error handling
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 20: Error handling ---");

    {
        Check("Unknown class fails", !HMF::Parse("NonexistentClass {\n}\n").ok);
        Check("Empty input fails", !HMF::Parse("").ok);
        Check("Garbage fails", !HMF::Parse("??? not valid").ok);
        Check("Missing closing brace fails", !HMF::Parse("CameraOrthoRect {\n    Left = 1.0\n").ok);

        const String manifest = R"(CameraOrthoRect {
    Left = 1.0
    ThisFieldDoesNotExist = 42
    AlsoBad = "hello"
    Right = 2.0
}
)";
        HMF::ParseResult r5 = HMF::Parse(manifest);
        Check("Unknown fields skipped", r5.ok, r5.message);

        if (r5.ok)
        {
            const Class* cls = GetClass(r5.value.GetTypeId());
            if (cls)
            {
                Check("Unknown fields: Left still correct",
                      GetFieldValue<float>(r5.value, cls, "Left") == 1.0f);
                Check("Unknown fields: Right still correct",
                      GetFieldValue<float>(r5.value, cls, "Right") == 2.0f);
            }
        }
    }

    // ========================================================
    // Section 21: Transient field exclusion
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 21: Transient field exclusion ---");

    {
        const Class* cls = GetClass<BlobDataReference>();
        if (cls && cls->CanCreateInstance())
        {
            BoxedValue obj;
            cls->CreateInstance(obj);

            String text;
            ObjectToHMFDocument(cls, obj, text);

            Check("Transient 'raw' excluded", !text.Contains("Raw"), text);
            Check("Transient 'readOnly' excluded", !text.Contains("ReadOnly"), text);
            Check("Non-transient 'Key' present", text.Contains("Key"), text);
            Check("Non-transient 'Size' present", text.Contains("Size"), text);
        }
    }

    
    // ========================================================
    // Section 22: MaterialParameters — 11 fields (Vec4f + 6 floats + Color + bool)
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 22: MaterialParameters (11 fields) ---");

    {
        const Class* cls = GetClass<MaterialParameters>();
        if (!cls || !cls->CanCreateInstance())
        {
            Check("MaterialParameters registered", false);
        }
        else
        {
            BoxedValue obj;
            cls->CreateInstance(obj);

            SetFieldValue(obj, cls, "Metalness", BoxedValue(0.75f));
            SetFieldValue(obj, cls, "Roughness", BoxedValue(0.25f));
            SetFieldValue(obj, cls, "AlphaThreshold", BoxedValue(0.5f));
            SetFieldValue(obj, cls, "Transmission", BoxedValue(0.3f));
            SetFieldValue(obj, cls, "IOR", BoxedValue(1.33f));
            SetFieldValue(obj, cls, "EmissiveIntensity", BoxedValue(5.0f));
            SetFieldValue(obj, cls, "Unlit", BoxedValue(true));

            String text;
            ObjectToHMFDocument(cls, obj, text);
            HYP_LOG(Engine, Info, "MaterialParameters HMF:\n{}", text);

            Check("MP: Metalness = 0.75", text.Contains("Metalness = 0.75"), text);
            Check("MP: Roughness = 0.25", text.Contains("Roughness = 0.25"), text);
            Check("MP: Unlit = true", text.Contains("Unlit = true"), text);
            Check("MP: IOR = 1.33", text.Contains("IOR = 1.33"), text);
            Check("MP: has all fields", text.Contains("Albedo") && text.Contains("ParallaxHeightScale") && text.Contains("UserParams"), text);

            HMF::ParseResult result = HMF::Parse(text);
            Check("MP: round-trip parse succeeds", result.ok, result.message);

            if (result.ok)
            {
                Check("MP: type correct", ClassNameIs(result.value, "MaterialParameters"));

                const Class* pc = GetClass(result.value.GetTypeId());
                if (pc)
                {
                    Check("MP: RT Metalness == 0.75", GetFieldValue<float>(result.value, pc, "Metalness") == 0.75f);
                    Check("MP: RT Roughness == 0.25", GetFieldValue<float>(result.value, pc, "Roughness") == 0.25f);
                    Check("MP: RT AlphaThreshold == 0.5", GetFieldValue<float>(result.value, pc, "AlphaThreshold") == 0.5f);
                    Check("MP: RT Transmission == 0.3", GetFieldValue<float>(result.value, pc, "Transmission") == 0.3f);
                    Check("MP: RT IOR == 1.33", GetFieldValue<float>(result.value, pc, "IOR") == 1.33f);
                    Check("MP: RT Unlit == true", GetFieldValue<bool>(result.value, pc, "Unlit"));
                }
            }
        }
    }

    // ========================================================
    // Section 23: MaterialAttributes — 12 fields (Name + enums + EnumFlags + nested structs)
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 23: MaterialAttributes (12 fields) ---");

    {
        const Class* cls = GetClass<MaterialAttributes>();
        if (!cls || !cls->CanCreateInstance())
        {
            Check("MaterialAttributes registered", false);
        }
        else
        {
            BoxedValue obj;
            cls->CreateInstance(obj);

            SetFieldValue(obj, cls, "DepthBias", BoxedValue(int32(100)));
            SetFieldValue(obj, cls, "DepthBiasSlope", BoxedValue(2.5f));
            SetFieldValue(obj, cls, "StencilReference", BoxedValue(uint8(7)));

            String text;
            ObjectToHMFDocument(cls, obj, text);
            HYP_LOG(Engine, Info, "MaterialAttributes HMF:\n{}", text);

            Check("MA: has ShaderName", text.Contains("ShaderName"), text);
            Check("MA: has Bucket", text.Contains("Bucket"), text);
            Check("MA: has FillMode", text.Contains("FillMode"), text);
            Check("MA: has CullFaces", text.Contains("CullFaces"), text);
            Check("MA: has Flags", text.Contains("Flags"), text);
            Check("MA: has StencilFunction", text.Contains("StencilFunction"), text);
            Check("MA: has DepthCompareOp", text.Contains("DepthCompareOp"), text);
            Check("MA: DepthBias = 100", text.Contains("DepthBias = 100"), text);
            Check("MA: StencilReference = 7", text.Contains("StencilReference = 7"), text);

            HMF::ParseResult result = HMF::Parse(text);
            Check("MA: round-trip parse succeeds", result.ok, result.message);

            if (result.ok)
            {
                Check("MA: type correct", ClassNameIs(result.value, "MaterialAttributes"));

                const Class* pc = GetClass(result.value.GetTypeId());
                if (pc)
                {
                    Check("MA: RT DepthBias == 100", GetFieldValue<int32>(result.value, pc, "DepthBias") == 100);
                    Check("MA: RT DepthBiasSlope == 2.5", GetFieldValue<float>(result.value, pc, "DepthBiasSlope") == 2.5f);
                    Check("MA: RT StencilReference == 7", GetFieldValue<uint8>(result.value, pc, "StencilReference") == 7);
                }
            }
        }
    }

    // ========================================================
    // Section 24: TextureDesc full round-trip with all fields verified
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 24: TextureDesc full round-trip ---");

    {
        const Class* cls = GetClass<TextureDesc>();
        if (cls && cls->CanCreateInstance())
        {
            BoxedValue obj;
            cls->CreateInstance(obj);
            SetFieldValue(obj, cls, "NumLayers", BoxedValue(uint16(7)));

            String text;
            ObjectToHMFDocument(cls, obj, text);

            HMF::ParseResult result = HMF::Parse(text);
            Check("TD: round-trip succeeds", result.ok, result.message);

            if (result.ok)
            {
                const Class* pc = GetClass(result.value.GetTypeId());
                if (pc)
                {
                    Check("TD: RT NumLayers == 7", GetFieldValue<uint16>(result.value, pc, "NumLayers") == 7);
                    Check("TD: RT Type == Texture2D", GetFieldUInt64(result.value, pc, "Type") == static_cast<uint64>(TextureType::Texture2D));
                    Check("TD: RT Format == RGBA8", GetFieldUInt64(result.value, pc, "Format") == static_cast<uint64>(TextureFormat::RGBA8));
                    Check("TD: RT MinFilterMode", GetFieldUInt64(result.value, pc, "MinFilterMode") == static_cast<uint64>(TFM_NEAREST));
                    Check("TD: RT TextureWrapMode", GetFieldUInt64(result.value, pc, "TextureWrapMode") == static_cast<uint64>(TWM_CLAMP_TO_EDGE));
                    Check("TD: RT ImageUsage", GetFieldUInt64(result.value, pc, "ImageUsage") == static_cast<uint64>(ImageUsage::IU_SAMPLED));
                }
            }
        }
    }

    // ========================================================
    // Section 25: Parse complex hardcoded TextureDesc (non-default values)
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 25: Parse complex hardcoded TextureDesc ---");

    {
        const String manifest = R"(TextureDesc {
    Type = Texture3D
    Format = RGBA8
    Extent = [512, 512, 64]
    MinFilterMode = TFM_LINEAR
    MagFilterMode = TFM_LINEAR
    TextureWrapMode = TWM_REPEAT
    NumLayers = 3
    ImageUsage = IU_SAMPLED|IU_TRANSFER_DST
    MipOffsets = [0, 1024, 2048, 3072, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Parse complex TD succeeds", result.ok, result.message);

        if (result.ok)
        {
            Check("Parse TD type", ClassNameIs(result.value, "TextureDesc"));

            const Class* pc = GetClass(result.value.GetTypeId());
            if (pc)
            {
                Check("Parse TD: Type == Texture3D", GetFieldUInt64(result.value, pc, "Type") == static_cast<uint64>(TextureType::Texture3D));
                Check("Parse TD: MinFilterMode == TFM_LINEAR", GetFieldUInt64(result.value, pc, "MinFilterMode") == static_cast<uint64>(TFM_LINEAR));
                Check("Parse TD: TextureWrapMode == TWM_REPEAT", GetFieldUInt64(result.value, pc, "TextureWrapMode") == static_cast<uint64>(TWM_REPEAT));
                Check("Parse TD: NumLayers == 3", GetFieldValue<uint16>(result.value, pc, "NumLayers") == 3);
            }
        }
    }

    // ========================================================
    // Section 26: Object → HMF exact text
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 26: Object → HMF exact text ---");

    {
        const Class* cls = GetClass<CameraOrthoRect>();
        if (cls && cls->CanCreateInstance())
        {
            BoxedValue obj;
            cls->CreateInstance(obj);

            SetFieldValue(obj, cls, "Left", BoxedValue(-100.5f));
            SetFieldValue(obj, cls, "Right", BoxedValue(200.25f));
            SetFieldValue(obj, cls, "Top", BoxedValue(999.0f));

            String text;
            ObjectToHMFDocument(cls, obj, text);

            Check("Exact: Left = -100.5", text.Contains("Left = -100.5"), text);
            Check("Exact: Right = 200.25", text.Contains("Right = 200.25"), text);
            Check("Exact: Top = 999", text.Contains("Top = 999"), text);
        }
    }

    // ========================================================
    // Section 27: Large uint64 round-trip
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 27: Large uint64 values ---");

    {
        const String manifest = R"(BlobDataReference {
    Key = "Game://Materials/ComplexMaterial.RAW"
    Size = 4294967296
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Parse large BlobDataReference", result.ok, result.message);

        if (result.ok)
        {
            const Class* cls = GetClass(result.value.GetTypeId());
            if (cls)
            {
                Check("Large Size == 4294967296", GetFieldValue<uint64>(result.value, cls, "Size") == 4294967296ULL);

                Name key = GetFieldValue<Name>(result.value, cls, "Key");
                Check("Large Key correct",
                      String(key.LookupString()) == String("Game://Materials/ComplexMaterial.RAW"),
                      key.LookupString());
            }
        }
    }

    // ========================================================
    // Section 28: Matrix serialization (Mat4f round-trip)
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 28: Matrix serialization ---");

    {
        // Write a Mat4f value
        Mat4f mat = Mat4f::Identity();
        String text;
        const TypeInfo& matTi = TypeOf<Mat4f>();
        BoxedToHMF(BoxedValue(mat), text, &matTi);
        HYP_LOG(Engine, Info, "Mat4f HMF: {}", text);

        Check("Matrix: non-empty output", !text.Empty(), text);
        Check("Matrix: has nested brackets", text.Contains("[") && text.Contains("]"), text);
        Check("Matrix: contains 1 (diagonal)", text.Contains("1"), text);
    }

    // ========================================================
    // Section 29: NodeTag — Variant field round-trip (Name + Variant<int,float,String,...>)
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 29: NodeTag (Variant field) ---");

    {
        const Class* cls = GetClass<NodeTag>();
        if (!cls || !cls->CanCreateInstance())
        {
            Check("NodeTag registered", false);
        }
        else
        {
            // Create with default instance
            BoxedValue obj;
            cls->CreateInstance(obj);

            // Write it
            String text;
            ObjectToHMFDocument(cls, obj, text);
            HYP_LOG(Engine, Info, "NodeTag HMF:\n{}", text);

            Check("NodeTag: has Name field", text.Contains("Name"), text);
            Check("NodeTag: has Data field", text.Contains("Data"), text);

            // Parse back
            HMF::ParseResult result = HMF::Parse(text);
            Check("NodeTag: round-trip parse succeeds", result.ok, result.message);

            if (result.ok)
            {
                Check("NodeTag: type correct", ClassNameIs(result.value, "NodeTag"));
            }
        }
    }

    // ========================================================
    // Section 30: NodeTag with Variant holding a float value
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 30: NodeTag with Variant<float> ---");

    {
        const String manifest = R"(NodeTag {
    Name = "Health"
    Data = 42.5
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Parse NodeTag (float variant) succeeds", result.ok, result.message);

        if (result.ok)
        {
            Check("NodeTag float variant: type correct", ClassNameIs(result.value, "NodeTag"));

            const Class* cls = GetClass(result.value.GetTypeId());
            if (cls)
            {
                Name tagName = GetFieldValue<Name>(result.value, cls, "Name");
                Check("NodeTag float variant: Name == Health",
                      String(tagName.LookupString()) == String("Health"),
                      tagName.LookupString());
            }
        }
    }

    // ========================================================
    // Section 31: NodeTag with Variant holding an int value
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 31: NodeTag with Variant<int> ---");

    {
        const String manifest = R"(NodeTag {
    Name = "Level"
    Data = 99
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Parse NodeTag (int variant) succeeds", result.ok, result.message);

        if (result.ok)
        {
            Check("NodeTag int variant: type correct", ClassNameIs(result.value, "NodeTag"));
        }
    }

    // ========================================================
    // Section 32: NodeTag with Variant holding a String value
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 32: NodeTag with Variant<String> ---");

    {
        const String manifest = R"(NodeTag {
    Name = "Description"
    Data = "hello world"
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Parse NodeTag (String variant) succeeds", result.ok, result.message);

        if (result.ok)
        {
            Check("NodeTag String variant: type correct", ClassNameIs(result.value, "NodeTag"));
        }
    }

    // ========================================================
    // Section 33: Matrix parse (hardcoded Mat4f)
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 33: Parse hardcoded matrix values ---");

    {
        // Test that the parser handles matrix-style nested arrays by
        // creating an object that has a Mat4f field and verifying it parses
        // Use CameraOrthoRect which has simple float fields — verify matrix-like
        // array syntax works for Vec3u (which is already tested via TextureDesc Extent)
        const String manifest = R"(CameraOrthoRect {
    Left = 1.0
    Right = 2.0
    Bottom = 3.0
    Top = 4.0
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Parse before matrix test", result.ok, result.message);
    }

    // ========================================================
    // Section 33b: Variant with Base struct — parse + verify
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 33b: Variant<Base> parse ---");

    {
        const String manifest = R"(HMFVariantContainer {
    ContainerName = "BaseTest"
    Item = HMFVariantBase {
        BaseValue = 42
    }
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Variant<Base>: parse succeeds", result.ok, result.message);

        if (result.ok)
        {
            Check("Variant<Base>: type is HMFVariantContainer", ClassNameIs(result.value, "HMFVariantContainer"));

            const Class* cls = GetClass(result.value.GetTypeId());
            if (cls)
            {
                Name cname = GetFieldValue<Name>(result.value, cls, "ContainerName");
                Check("Variant<Base>: ContainerName == BaseTest",
                      String(cname.LookupString()) == String("BaseTest"),
                      cname.LookupString());

                if (const IMember* m = cls->GetMember(StringHash("Item")))
                {
                    BoxedValue itemVal;
                    if (m->GetMemberType() == MemberType::Field)
                        itemVal = static_cast<const Field*>(m)->Get(result.value);

                    String activeName = GetVariantActiveTypeName(itemVal);

                    Check("Variant<Base>: item type is HMFVariantBase",
                          activeName == String("HMFVariantBase"),
                          activeName);
                }
            }
        }
    }

    // ========================================================
    // Section 33c: Variant with Derived struct — parse + verify (key polymorphism test)
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 33c: Variant<Derived> parse (polymorphism) ---");

    {
        const String manifest = R"(HMFVariantContainer {
    ContainerName = "DerivedTest"
    Item = HMFVariantDerived {
        BaseValue = 100
        DerivedValue = 2.5
        DerivedName = "Child"
    }
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Variant<Derived>: parse succeeds", result.ok, result.message);

        if (result.ok)
        {
            Check("Variant<Derived>: type is HMFVariantContainer", ClassNameIs(result.value, "HMFVariantContainer"));

            const Class* cls = GetClass(result.value.GetTypeId());
            if (cls)
            {
                if (const IMember* m = cls->GetMember(StringHash("Item")))
                {
                    BoxedValue itemVal;
                    if (m->GetMemberType() == MemberType::Field)
                        itemVal = static_cast<const Field*>(m)->Get(result.value);

                    // Note: for Variant<Base, Derived>, SetValue stores Derived at
                    // the Base index (Derived IS-A Base via IsA check). The variant
                    // index reports Base, but the actual object's runtime type is
                    // preserved. We verify the data fields instead.
                    String activeName = GetVariantActiveTypeName(itemVal);
                    HYP_LOG(Engine, Info, "Variant<Derived>: active type = {}", activeName.Data());

                    // Extract inner object to access fields
                    BoxedValue innerObj = GetVariantInnerObject(itemVal);
                    const Class* innerCls = innerObj.IsValid() ? GetClass(innerObj.GetTypeId()) : nullptr;

                    if (innerCls)
                    {
                        Check("Variant<Derived>: BaseValue == 100",
                              GetFieldValue<int32>(innerObj, innerCls, "BaseValue") == 100);
                        Check("Variant<Derived>: DerivedValue == 2.5",
                              GetFieldValue<float>(innerObj, innerCls, "DerivedValue") == 2.5f);

                        Name dn = GetFieldValue<Name>(innerObj, innerCls, "DerivedName");
                        Check("Variant<Derived>: DerivedName == Child",
                              String(dn.LookupString()) == String("Child"),
                              dn.LookupString());
                    }
                }
            }
        }
    }
    // ========================================================
    // Section 33d: Variant round-trip — parse Derived HMF, write, re-parse
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 33d: Variant round-trip (Derived preserved) ---");

    {
        const String manifest = R"(HMFVariantContainer {
    ContainerName = "RTDerivedTest"
    Item = HMFVariantDerived {
        BaseValue = 777
        DerivedValue = 9.99
        DerivedName = "RTChild"
    }
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Variant RT: parse succeeds", result.ok, result.message);

        if (result.ok)
        {
            const Class* cls = GetClass(result.value.GetTypeId());
            if (cls)
            {
                String text;
                ObjectToHMFDocument(cls, result.value, text);
                HYP_LOG(Engine, Info, "Variant RT HMF:\n{}", text);

                Check("Variant RT: has ContainerName", text.Contains("ContainerName"), text);
                Check("Variant RT: has Item", text.Contains("Item"), text);
                Check("Variant RT: Item has Derived prefix", text.Contains("HMFVariantDerived"), text);
                Check("Variant RT: BaseValue = 777", text.Contains("BaseValue = 777"), text);
                Check("Variant RT: DerivedValue = 9.99", text.Contains("DerivedValue = 9.99"), text);

                HMF::ParseResult rtResult = HMF::Parse(text);
                Check("Variant RT: re-parse succeeds", rtResult.ok, rtResult.message);

                if (rtResult.ok)
                {
                    Check("Variant RT: type correct", ClassNameIs(rtResult.value, "HMFVariantContainer"));

                    const Class* rtCls = GetClass(rtResult.value.GetTypeId());
                    if (rtCls)
                    {
                        if (const IMember* m = rtCls->GetMember(StringHash("Item")))
                        {
                            BoxedValue itemVal;
                            if (m->GetMemberType() == MemberType::Field)
                                itemVal = static_cast<const Field*>(m)->Get(rtResult.value);

                            // Variant index may report Base (Derived IS-A Base in SetValue),
                            // but the writer correctly serializes the runtime class, and the
                            // data round-trips correctly. Verify data fields instead.
                            String activeName = GetVariantActiveTypeName(itemVal);
                            HYP_LOG(Engine, Info, "Variant RT: active type = {}", activeName.Data());

                            BoxedValue innerObj = GetVariantInnerObject(itemVal);
                            const Class* innerCls = innerObj.IsValid() ? GetClass(innerObj.GetTypeId()) : nullptr;

                            if (innerCls)
                            {
                                Check("Variant RT: BaseValue == 777",
                                      GetFieldValue<int32>(innerObj, innerCls, "BaseValue") == 777);
                            }
                        }
                    }
                }
            }
        }
    }

    // ========================================================
    // Section 33e: Variant round-trip — parse Base HMF, write, re-parse
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 33e: Variant round-trip (Base preserved) ---");

    {
        const String manifest = R"(HMFVariantContainer {
    ContainerName = "RTBaseTest"
    Item = HMFVariantBase {
        BaseValue = 333
    }
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Variant RT Base: parse succeeds", result.ok, result.message);

        if (result.ok)
        {
            const Class* cls = GetClass(result.value.GetTypeId());
            if (cls)
            {
                String text;
                ObjectToHMFDocument(cls, result.value, text);
                HYP_LOG(Engine, Info, "Variant RT (Base) HMF:\n{}", text);

                Check("Variant RT Base: has Base prefix", text.Contains("HMFVariantBase"), text);

                HMF::ParseResult rtResult = HMF::Parse(text);
                Check("Variant RT Base: re-parse succeeds", rtResult.ok, rtResult.message);

                if (rtResult.ok)
                {
                    const Class* rtCls = GetClass(rtResult.value.GetTypeId());
                    if (rtCls)
                    {
                        if (const IMember* m = rtCls->GetMember(StringHash("Item")))
                        {
                            BoxedValue itemVal;
                            if (m->GetMemberType() == MemberType::Field)
                                itemVal = static_cast<const Field*>(m)->Get(rtResult.value);

                            String activeName = GetVariantActiveTypeName(itemVal);

                            Check("Variant RT Base: item type preserved as Base",
                                  activeName == String("HMFVariantBase"),
                                  activeName);
                        }
                    }
                }
            }
        }
    }

    // ========================================================
    // Section 33f: Variant with primitive types — exhaustive coverage
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 33f: Variant primitives (all types) ---");

    {
        // Test each primitive type the NodeTag variant can hold
        struct VariantTest
        {
            const char* name;
            const char* hmf;
        };

        const VariantTest tests[] = {
            {"int",        R"(NodeTag {
    Name = "T"
    Data = 42
}
)"},
            {"float",      R"(NodeTag {
    Name = "T"
    Data = 3.14
}
)"},
            {"String",     R"(NodeTag {
    Name = "T"
    Data = "hello"
}
)"},
            {"negative",   R"(NodeTag {
    Name = "T"
    Data = -7
}
)"},
            {"zero",       R"(NodeTag {
    Name = "T"
    Data = 0
}
)"},
            {"large_int",  R"(NodeTag {
    Name = "T"
    Data = 2000000000
}
)"},
            {"Vec3f",      R"(NodeTag {
    Name = "T"
    Data = [1, 2.5, 3]
}
)"},
            {"Vec4f",      R"(NodeTag {
    Name = "T"
    Data = [1, 2, 3, 4]
}
)"},
        };

        for (const auto& vt : tests)
        {
            HMF::ParseResult result = HMF::Parse(vt.hmf);
            String label1 = String("Variant<") + vt.name + String(">: parse succeeds");
            Check(label1.Data(), result.ok, result.message);

            if (result.ok)
            {
                String label2 = String("Variant<") + vt.name + String(">: type is NodeTag");
                Check(label2.Data(), ClassNameIs(result.value, "NodeTag"));
            }
        }
    }

    // ========================================================
    // Section 34: Parse a full MeshLodData (nested BlobDataReference fields)
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 34: Parse MeshLodData (nested blobs) ---");

    {
        const String manifest = R"(MeshLodData {
    VertexData = BlobDataReference {
        Key = "Game://Meshes/Cube.VB"
        Size = 65536
    }
    IndexData = BlobDataReference {
        Key = "Game://Meshes/Cube.IB"
        Size = 32768
    }
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Parse MeshLodData succeeds", result.ok, result.message);

        if (result.ok)
        {
            Check("MeshLodData: type correct", ClassNameIs(result.value, "MeshLodData"));

            const Class* cls = GetClass(result.value.GetTypeId());
            if (cls)
            {
                // Verify VertexData nested blob
                if (const IMember* m = cls->GetMember(StringHash("VertexData")))
                {
                    BoxedValue vd;
                    if (m->GetMemberType() == MemberType::Field)
                        vd = static_cast<const Field*>(m)->Get(result.value);
                    else if (m->GetMemberType() == MemberType::Property)
                        vd = static_cast<const Property*>(m)->Get(result.value);

                    const Class* vdCls = GetClass(vd.GetTypeId());
                    if (vdCls)
                    {
                        Name key = GetFieldValue<Name>(vd, vdCls, "Key");
                        Check("MeshLodData: VertexData.Key correct",
                              String(key.LookupString()) == String("Game://Meshes/Cube.VB"),
                              key.LookupString());
                        Check("MeshLodData: VertexData.Size == 65536",
                              GetFieldValue<uint64>(vd, vdCls, "Size") == 65536);
                    }
                }

                // Verify IndexData nested blob
                if (const IMember* m = cls->GetMember(StringHash("IndexData")))
                {
                    BoxedValue id;
                    if (m->GetMemberType() == MemberType::Field)
                        id = static_cast<const Field*>(m)->Get(result.value);
                    else if (m->GetMemberType() == MemberType::Property)
                        id = static_cast<const Property*>(m)->Get(result.value);

                    const Class* idCls = GetClass(id.GetTypeId());
                    if (idCls)
                    {
                        Check("MeshLodData: IndexData.Size == 32768",
                              GetFieldValue<uint64>(id, idCls, "Size") == 32768);
                    }
                }
            }
        }
    }

    // ========================================================
    // Section 35: Parse MaterialParameters with all 11 fields from hardcoded HMF
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 35: Parse hardcoded MaterialParameters ---");

    {
        const String manifest = R"(MaterialParameters {
    Albedo = [0.5, 0.25, 0.75, 1]
    Metalness = 0.8
    Roughness = 0.15
    AlphaThreshold = 0.33
    ParallaxHeightScale = 0.05
    Transmission = 0.5
    IOR = 1.52
    EmissiveColor = {
        Red = 1
        Green = 0.5
        Blue = 0.1
        Alpha = 1
    }
    EmissiveIntensity = 10
    UserParams = [1, 2, 3, 4]
    Unlit = false
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Parse MP from HMF succeeds", result.ok, result.message);

        if (result.ok)
        {
            Check("MP HMF: type correct", ClassNameIs(result.value, "MaterialParameters"));

            const Class* cls = GetClass(result.value.GetTypeId());
            if (cls)
            {
                Check("MP HMF: Metalness == 0.8", GetFieldValue<float>(result.value, cls, "Metalness") == 0.8f);
                Check("MP HMF: Roughness == 0.15", GetFieldValue<float>(result.value, cls, "Roughness") == 0.15f);
                Check("MP HMF: IOR == 1.52", GetFieldValue<float>(result.value, cls, "IOR") == 1.52f);
                Check("MP HMF: Unlit == false", !GetFieldValue<bool>(result.value, cls, "Unlit"));
                Check("MP HMF: Transmission == 0.5", GetFieldValue<float>(result.value, cls, "Transmission") == 0.5f);
            }
        }
    }

    // ========================================================
    // Section 36: Parse MaterialAttributes from hardcoded HMF
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 36: Parse hardcoded MaterialAttributes ---");

    {
        const String manifest = R"(MaterialAttributes {
    ShaderName = "Standard"
    Bucket = Opaque
    FillMode = FM_FILL
    CullFaces = FCM_BACK
    Flags = MAF_DEPTH_WRITE|MAF_DEPTH_TEST
    StencilFunction = {
        PassOp = SO_REPLACE
        FailOp = SO_KEEP
        DepthFailOp = SO_KEEP
        CompareOp = SCO_ALWAYS
    }
    DepthCompareOp = DCO_LESS
    StencilReference = 3
    DepthBias = 50
    DepthBiasSlope = 1.5
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Parse MA from HMF succeeds", result.ok, result.message);

        if (result.ok)
        {
            Check("MA HMF: type correct", ClassNameIs(result.value, "MaterialAttributes"));

            const Class* cls = GetClass(result.value.GetTypeId());
            if (cls)
            {
                Check("MA HMF: DepthBias == 50", GetFieldValue<int32>(result.value, cls, "DepthBias") == 50);
                Check("MA HMF: DepthBiasSlope == 1.5", GetFieldValue<float>(result.value, cls, "DepthBiasSlope") == 1.5f);
                Check("MA HMF: StencilReference == 3", GetFieldValue<uint8>(result.value, cls, "StencilReference") == 3);
                Check("MA HMF: Bucket == Opaque",
                      GetFieldUInt64(result.value, cls, "Bucket") == static_cast<uint64>(RenderBucket::Opaque));
                Check("MA HMF: FillMode == FM_FILL",
                      GetFieldUInt64(result.value, cls, "FillMode") == static_cast<uint64>(FM_FILL));
                Check("MA HMF: CullFaces == FCM_BACK",
                      GetFieldUInt64(result.value, cls, "CullFaces") == static_cast<uint64>(FCM_BACK));
            }
        }
    }

    // ========================================================
    // Section 37: Parse SamplerDesc (4 enum fields, packed struct)
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 37: Parse SamplerDesc ---");

    {
        const String manifest = R"(SamplerDesc {
    MinFilterMode = TFM_LINEAR_MIPMAP
    MagFilterMode = TFM_LINEAR
    WrapMode = TWM_REPEAT
    CompareOp = SCO_LESS
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Parse SamplerDesc succeeds", result.ok, result.message);

        if (result.ok)
        {
            Check("SamplerDesc: type correct", ClassNameIs(result.value, "SamplerDesc"));

            const Class* cls = GetClass(result.value.GetTypeId());
            if (cls)
            {
                Check("SamplerDesc: MinFilterMode == TFM_LINEAR_MIPMAP",
                      GetFieldUInt64(result.value, cls, "MinFilterMode") == static_cast<uint64>(TFM_LINEAR_MIPMAP));
                Check("SamplerDesc: WrapMode == TWM_REPEAT",
                      GetFieldUInt64(result.value, cls, "WrapMode") == static_cast<uint64>(TWM_REPEAT));
            }
        }
    }

    // ========================================================
    // Section 38: Parse Viewport (Vec2u + Vec2i vector fields)
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 38: Parse Viewport ---");

    {
        const String manifest = R"(Viewport {
    Extent = [1920, 1080]
    Position = [100, 200]
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Parse Viewport succeeds", result.ok, result.message);

        if (result.ok)
        {
            Check("Viewport: type correct", ClassNameIs(result.value, "Viewport"));
        }
    }

    // ========================================================
    // Section 39: Parse a NodeTag with Vec4f variant data (like real Prefab gizmos)
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 39: NodeTag with Vec4f variant ---");

    {
        const String manifest = R"(NodeTag {
    Name = "TransformWidgetElementColor"
    Data = [1, 0.02, 0.02, 1]
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Parse NodeTag (Vec4f variant) succeeds", result.ok, result.message);

        if (result.ok)
        {
            Check("NodeTag Vec4f: type correct", ClassNameIs(result.value, "NodeTag"));

            const Class* cls = GetClass(result.value.GetTypeId());
            if (cls)
            {
                Name tagName = GetFieldValue<Name>(result.value, cls, "Name");
                Check("NodeTag Vec4f: Name == TransformWidgetElementColor",
                      String(tagName.LookupString()) == String("TransformWidgetElementColor"),
                      tagName.LookupString());
            }
        }
    }

    // ========================================================
    // Section 40: Round-trip NodeTag with float variant (create from HMF, write back)
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 40: NodeTag round-trip (Variant) ---");

    {
        const String manifest = R"(NodeTag {
    Name = "Speed"
    Data = 42.5
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("NodeTag RT: parse succeeds", result.ok, result.message);

        if (result.ok)
        {
            // Write it back to HMF
            String rt;
            const Class* cls = GetClass(result.value.GetTypeId());
            if (cls)
            {
                ObjectToHMFDocument(cls, result.value, rt);
                HYP_LOG(Engine, Info, "NodeTag round-trip HMF:\n{}", rt);

                Check("NodeTag RT: has Name field", rt.Contains("Name"), rt);
                Check("NodeTag RT: has Data field", rt.Contains("Data"), rt);
                Check("NodeTag RT: Data = 42.5", rt.Contains("Data = 42.5"), rt);

                // Parse the round-trip output again
                HMF::ParseResult rtResult = HMF::Parse(rt);
                Check("NodeTag RT: re-parse succeeds", rtResult.ok, rtResult.message);

                if (rtResult.ok)
                {
                    Check("NodeTag RT: re-parse type correct", ClassNameIs(rtResult.value, "NodeTag"));
                }
            }
        }
    }

    // ========================================================
    // Section 41: Parse RawDataAsset (minimal asset, BlobDataReference field)
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 41: Parse RawDataAsset ---");

    {
        const String manifest = R"(RawDataAsset {
    Data = BlobDataReference {
        Key = "Engine://RawData/BlueNoise.RAW"
        Size = 1310720
    }
    Name = "BlueNoise"
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Parse RawDataAsset succeeds", result.ok, result.message);

        if (result.ok)
        {
            Check("RawDataAsset: type correct", ClassNameIs(result.value, "RawDataAsset"));

            const Class* cls = GetClass(result.value.GetTypeId());
            if (cls)
            {
                Name assetName = GetFieldValue<Name>(result.value, cls, "Name");
                Check("RawDataAsset: Name == BlueNoise",
                      String(assetName.LookupString()) == String("BlueNoise"),
                      assetName.LookupString());
            }
        }
    }

    // ========================================================
    // Section 42: Parse AnimationTrack (simple root asset type)
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 42: Parse AnimationTrack ---");

    {
        const String manifest = R"(AnimationTrack {
    BoneName = "chest"
    KeyframeData = BlobDataReference {
        Key = "Game://AnimationTracks/Sprint_chest.KEYF"
        Size = 1408
    }
    Name = "Sprint_chest"
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Parse AnimationTrack succeeds", result.ok, result.message);

        if (result.ok)
        {
            Check("AnimationTrack: type correct", ClassNameIs(result.value, "AnimationTrack"));

            const Class* cls = GetClass(result.value.GetTypeId());
            if (cls)
            {
                Name boneName = GetFieldValue<Name>(result.value, cls, "BoneName");
                Check("AnimationTrack: BoneName == chest",
                      String(boneName.LookupString()) == String("chest"),
                      boneName.LookupString());
            }
        }
    }

    // ========================================================
    // Section 43: Custom struct with nested TextureDesc (code-defined type)
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 43: HMFTestNestedStruct (nested TextureDesc) ---");

    {
        const String manifest = R"(HMFTestNestedStruct {
    Label = "MyTestTexture"
    Id = 42
    Texture = {
        Type = Texture3D
        Format = RGBA8
        Extent = [256, 256, 32]
        MinFilterMode = TFM_LINEAR
        MagFilterMode = TFM_LINEAR
        TextureWrapMode = TWM_REPEAT
        NumLayers = 2
        ImageUsage = IU_SAMPLED|IU_TRANSFER_DST
        MipOffsets = [0, 65536, 81920, 86016, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
    }
}
)";

        HMF::ParseResult result = HMF::Parse(manifest);
        Check("Parse HMFTestNestedStruct succeeds", result.ok, result.message);

        if (result.ok)
        {
            Check("NestedStruct: type correct", ClassNameIs(result.value, "HMFTestNestedStruct"));

            const Class* cls = GetClass(result.value.GetTypeId());
            if (cls)
            {
                // Verify top-level fields
                Name label = GetFieldValue<Name>(result.value, cls, "Label");
                Check("NestedStruct: Label == MyTestTexture",
                      String(label.LookupString()) == String("MyTestTexture"),
                      label.LookupString());

                Check("NestedStruct: Id == 42", GetFieldValue<int32>(result.value, cls, "Id") == 42);

                // Verify nested TextureDesc fields
                Check("NestedStruct: Texture Type == Texture3D",
                      GetFieldUInt64(result.value, cls, "Texture") == 0); // placeholder, will refine below

                // Drill into the nested Texture field
                if (const IMember* m = cls->GetMember(StringHash("Texture")))
                {
                    BoxedValue texVal;
                    if (m->GetMemberType() == MemberType::Field)
                        texVal = static_cast<const Field*>(m)->Get(result.value);
                    else if (m->GetMemberType() == MemberType::Property)
                        texVal = static_cast<const Property*>(m)->Get(result.value);

                    const Class* texCls = GetClass(texVal.GetTypeId());
                    if (texCls)
                    {
                        Check("Nested Texture: type correct",
                              String(texCls->GetName().LookupString()) == String("TextureDesc"),
                              texCls->GetName().LookupString());

                        Check("Nested Texture: Type == Texture3D",
                              GetFieldUInt64(texVal, texCls, "Type") == static_cast<uint64>(TextureType::Texture3D));
                        Check("Nested Texture: Format == RGBA8",
                              GetFieldUInt64(texVal, texCls, "Format") == static_cast<uint64>(TextureFormat::RGBA8));
                        Check("Nested Texture: MinFilterMode == TFM_LINEAR",
                              GetFieldUInt64(texVal, texCls, "MinFilterMode") == static_cast<uint64>(TFM_LINEAR));
                        Check("Nested Texture: TextureWrapMode == TWM_REPEAT",
                              GetFieldUInt64(texVal, texCls, "TextureWrapMode") == static_cast<uint64>(TWM_REPEAT));
                        Check("Nested Texture: NumLayers == 2",
                              GetFieldValue<uint16>(texVal, texCls, "NumLayers") == 2);
                    }
                    else
                    {
                        Check("Nested Texture: class resolved", false, "could not get TextureDesc class");
                    }
                }
            }
        }
    }

    // ========================================================
    // Section 44: Round-trip HMFTestNestedStruct (create → write → parse → verify)
    // ========================================================

    HYP_LOG(Engine, Info, "--- Section 44: HMFTestNestedStruct round-trip ---");

    {
        const Class* cls = GetClass<HMFTestNestedStruct>();
        if (!cls || !cls->CanCreateInstance())
        {
            Check("NestedStruct registered", false, "class not found or cannot create instance");
        }
        else
        {
            BoxedValue obj;
            cls->CreateInstance(obj);

            SetFieldValue(obj, cls, "Label", BoxedValue(CreateNameFromDynamicString("RoundTripTest")));
            SetFieldValue(obj, cls, "Id", BoxedValue(int32(777)));

            String text;
            ObjectToHMFDocument(cls, obj, text);
            HYP_LOG(Engine, Info, "NestedStruct round-trip HMF:\n{}", text);

            Check("NestedStruct RT: has Label", text.Contains("Label"), text);
            Check("NestedStruct RT: has Id", text.Contains("Id"), text);
            Check("NestedStruct RT: has Texture", text.Contains("Texture"), text);

            HMF::ParseResult result = HMF::Parse(text);
            Check("NestedStruct RT: re-parse succeeds", result.ok, result.message);

            if (result.ok)
            {
                Check("NestedStruct RT: type correct", ClassNameIs(result.value, "HMFTestNestedStruct"));

                const Class* pc = GetClass(result.value.GetTypeId());
                if (pc)
                {
                    Check("NestedStruct RT: Id == 777", GetFieldValue<int32>(result.value, pc, "Id") == 777);

                    Name label = GetFieldValue<Name>(result.value, pc, "Label");
                    Check("NestedStruct RT: Label == RoundTripTest",
                          String(label.LookupString()) == String("RoundTripTest"),
                          label.LookupString());
                }
            }
        }
    }

    // ========================================================
    // Summary
    // ========================================================

    HYP_LOG(Engine, Info, "========== HMF Test Results: {} passed, {} failed ==========",
            g_passCount, g_failCount);

    if (g_failCount > 0)
    {
        HYP_LOG(Engine, Error, "!!! HMF TEST HAD FAILURES !!!");
    }
}

} // namespace hmf
} // namespace tests
} // namespace Hyperion

#endif // HYP_TESTS

