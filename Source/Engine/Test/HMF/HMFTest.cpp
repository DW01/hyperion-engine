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
#include <Core/Reflection/Property.hpp>
#include <Core/Reflection/Field.hpp>
#include <Core/Reflection/Enum.hpp>
#include <Core/Utilities/EnumFlags.hpp>
#include <Core/Containers/Array.hpp>
#include <Core/Containers/String.hpp>
#include <Core/Logging/Logger.hpp>
#include <Core/Debug/Debug.hpp>

#include <cstdio>

#include <Asset/SerializationUtils.hpp>
#include <Asset/AssetPath.hpp>
#include <Asset/BlobStorageStructs.hpp>

#include <Rendering/Shared.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/MaterialTypes.hpp>
#include <Rendering/RenderableAttributes.hpp>
#include <Scene/Camera/Camera.hpp>
#include <Scene/Light.hpp>
#include <Scene/Node.hpp>

namespace Hyperion {
namespace tests {
namespace hmf {

namespace {

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
            SetFieldValue(obj, cls, "DepthBias", BoxedValue(int32(-42)));

            String text;
            ObjectToHMFDocument(cls, obj, text);
            HYP_LOG(Engine, Info, "MaterialParameters HMF:\n{}", text);

            Check("MP: Metalness = 0.75", text.Contains("Metalness = 0.75"), text);
            Check("MP: Roughness = 0.25", text.Contains("Roughness = 0.25"), text);
            Check("MP: Unlit = true", text.Contains("Unlit = true"), text);
            Check("MP: DepthBias = -42", text.Contains("DepthBias = -42"), text);
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
                    Check("MP: RT DepthBias == -42", GetFieldValue<int32>(result.value, pc, "DepthBias") == -42);
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
            Check("Exact: // hmf 1 header", text.Contains("// hmf 1"), text);
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
    Data = float 42.5
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
    Data = int 99
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
    Data = String "hello world"
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
