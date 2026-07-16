/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/HMF/Parser/Parser.hpp>
#include <Core/HMF/Parser/CompilerError.hpp>
#include <Core/HMF/Parser/Token.hpp>

#include <Core/Logging/Logger.hpp>

#include <Core/Reflection/BoxedValue.hpp>
#include <Core/Reflection/TypeInfo.hpp>
#include <Core/Reflection/Class.hpp>
#include <Core/Reflection/Enum.hpp>
#include <Core/Reflection/Field.hpp>
#include <Core/Reflection/Property.hpp>
#include <Core/Reflection/StaticField.hpp>
#include <Core/Reflection/Member.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Utilities/StringUtil.hpp>

#include <cstdlib>

namespace Hyperion::HMF {

using ClassMemberTypes = EnumFlags<MemberType>;

Parser::Parser(TokenStream* tokenStream, CompilationUnit* compilationUnit)
    : m_tokenStream(tokenStream),
      m_compilationUnit(compilationUnit)
{
}

bool Parser::Parse(BoxedValue& out)
{
    return ParseManifest(out);
}

bool Parser::ParseManifest(BoxedValue& out)
{
    String className;
    if (!ExpectIdentifier(className))
    {
        return false;
    }

    const Class* cls = Hyperion::GetClass(StringHash(className));
    if (!cls)
    {
        Error(MSG_CLASS_NOT_FOUND, Peek().GetLocation(), className);
        return false;
    }

    if (cls->IsAbstract() || !cls->CanCreateInstance())
    {
        Error(MSG_CLASS_NOT_FOUND, Peek().GetLocation(), className);
        return false;
    }

    // Optional instance-name string (`Texture "Checkerboard" { ... }`)
    String instanceName;
    if (Peek().GetTokenClass() == TK_STRING)
    {
        instanceName = Next().GetValue();
    }

    if (!Expect(TK_OPEN_BRACE, "{"))
    {
        return false;
    }

    if (!cls->CreateInstance(out))
    {
        Error(MSG_INTERNAL_ERROR, Peek().GetLocation());
        return false;
    }

    if (!ParseObjectBody(cls, out))
    {
        return false;
    }

    if (!Expect(TK_CLOSE_BRACE, "}"))
    {
        return false;
    }

    // Set name using property "Name" if it exists
    // Note that it must be a property, so needs `Property = "Name"`
    if (!instanceName.Empty())
    {
        const Name nameValue = CreateNameFromDynamicString(instanceName.Data());

        if (Property* nameProp = cls->GetProperty(StringHash("Name")))
        {
            nameProp->Set(out, BoxedValue(nameValue));
        }
    }

    // Note: the Engine-side code that invokes Parse may call cls->PostLoad() separately
    // once it has a concrete pointer (e.g. for AssetObject).

    return true;
}

bool Parser::ParseObjectBody(const Class* cls, BoxedValue& target)
{
    while (Peek().GetTokenClass() != TK_CLOSE_BRACE && Peek().GetTokenClass() != TK_EMPTY)
    {
        if (Peek().GetTokenClass() == TK_COMMA)
        {
            Token comma = Next();
            if (Peek().GetTokenClass() == TK_CLOSE_BRACE)
            {
                Error(MSG_UNEXPECTED_TOKEN, comma.GetLocation(), ",");
                return false;
            }
        }

        // Field name
        String fieldName;
        if (!ExpectIdentifier(fieldName))
        {
            return false;
        }

        // Expect '=' separator
        if (!Expect(TK_EQUALS, "="))
        {
            return false;
        }

        // Look up the member on the class
        const IMember* member = cls->GetMember(StringHash(fieldName), MemberType::Field | MemberType::Property);

        if (!member)
        {
            // Unknown field: warn and skip its value
            Warning(MSG_UNKNOWN_FIELD, Peek().GetLocation(), cls->GetName().ToString() + "::" + fieldName);
            SkipValue();
            continue;
        }

        if (member->GetAttribute(Attributes::g_attrJsonIgnore).IsValid())
        {
            SkipValue();
            continue;
        }

        // If has `Property = ...`, we want to grab the synthetic Property created instead.
        if (member->GetMemberType() == MemberType::Field && member->GetAttribute(Attributes::g_attrProperty).IsValid())
        {
            if (Property* prop = cls->GetProperty(StringHash(fieldName)))
            {
                member = prop;
            }
        }

        // Parse the value with the member's declared TypeInfo
        
        const TypeInfo& fieldTypeInfo = member->GetTypeInfo();
        
        BoxedValue fieldValue;
        if (!ParseValue(fieldTypeInfo, fieldValue))
        {
            return false;
        }

        // Assign to target via the appropriate member type
        switch (member->GetMemberType())
        {
        case MemberType::Field:
            static_cast<const Field*>(member)->Set(target, fieldValue);
            break;
        case MemberType::Property:
            static_cast<const Property*>(member)->Set(target, fieldValue);
            break;
        default:
            break;
        }
    }

    return true;
}

bool Parser::ParseValue(const TypeInfo& typeInfo, BoxedValue& out)
{
    const Token next = Peek();

    // '@' before string denotes AssetPath literal
    if (next.GetTokenClass() == TK_AT_STRING)
    {
        return ParseAssetPathLiteral(out);
    }

    if (typeInfo.IsBoolType())
    {
        return ParseBoolValue(out);
    }

    if (typeInfo.IsIntegralType())
    {
        return ParseIntegralValue(typeInfo, out);
    }

    if (typeInfo.IsFloatType())
    {
        return ParseFloatValue(typeInfo, out);
    }

    if (typeInfo.IsStringType())
    {
        return ParseStringValue(typeInfo, out);
    }

    // Name / StringHash — these have STRUCT_TYPE from HYP_STRUCT but should be
    // parsed as string literals, not objects.
    {
        const TypeId id = TypeInfo_GetId(typeInfo);
        if (id == TypeId::ForType<Name>() || id == TypeId::ForType<StringHash>())
        {
            return ParseStringValue(typeInfo, out);
        }
    }

    if (typeInfo.IsEnumFlags())
    {
        return ParseEnumFlagsValue(typeInfo, out);
    }

    if (typeInfo.IsEnum())
    {
        return ParseEnumValue(typeInfo, out);
    }

    if (typeInfo.IsVectorType())
    {
        return ParseVectorValue(typeInfo, out);
    }

    if (typeInfo.IsArrayType())
    {
        return ParseArrayValue(typeInfo, out);
    }

    if (typeInfo.IsSetType())
    {
        return ParseSetValue(typeInfo, out);
    }

    if (typeInfo.IsMapType())
    {
        return ParseMapValue(typeInfo, out);
    }

    if (typeInfo.IsPairType())
    {
        return ParsePairValue(typeInfo, out);
    }

    if (typeInfo.IsTupleType())
    {
        return ParseTupleValue(typeInfo, out);
    }

    if (typeInfo.IsMatrixType())
    {
        return ParseMatrixValue(typeInfo, out);
    }

    if (typeInfo.IsVariantType())
    {
        return ParseVariantValue(typeInfo, out);
    }

    if (typeInfo.IsClass() || typeInfo.IsStruct())
    {
        return ParseObjectValue(typeInfo, out);
    }

    Error(MSG_NOT_IMPLEMENTED, next.GetLocation(), String("Parsing of type ") + TypeInfo_GetName(typeInfo).ToString());

    return false;
}

bool Parser::ParseBoolValue(BoxedValue& out)
{
    Token tok = Peek();
    if (tok.GetTokenClass() != TK_IDENT)
    {
        Error(MSG_UNEXPECTED_TOKEN, tok.GetLocation(), Token::TokenTypeToString(tok.GetTokenClass()));
        return false;
    }

    Next();

    if (tok.GetValue() == "true")
    {
        out = BoxedValue(true);
        return true;
    }

    if (tok.GetValue() == "false")
    {
        out = BoxedValue(false);
        return true;
    }

    Error(MSG_UNEXPECTED_IDENTIFIER, tok.GetLocation(), tok.GetValue());

    return false;
}

bool Parser::ParseIntegralValue(const TypeInfo& typeInfo, BoxedValue& out)
{
    Token tok = Peek();
    if (tok.GetTokenClass() != TK_INTEGER)
    {
        Error(MSG_UNEXPECTED_TOKEN, tok.GetLocation(), Token::TokenTypeToString(tok.GetTokenClass()));
        return false;
    }

    Next();

    const String& text = tok.GetValue();

    const bool isHex = text.Size() >= 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X');
    const int base = isHex ? 16 : 10;

    const TypeId id = TypeInfo_GetId(typeInfo);

    if (id == TypeId::ForType<int8>())
    {
        out = BoxedValue(static_cast<int8>(std::strtoll(text.Data(), nullptr, base)));
    }
    else if (id == TypeId::ForType<int16>())
    {
        out = BoxedValue(static_cast<int16>(std::strtoll(text.Data(), nullptr, base)));
    }
    else if (id == TypeId::ForType<int32>())
    {
        out = BoxedValue(static_cast<int32>(std::strtoll(text.Data(), nullptr, base)));
    }
    else if (id == TypeId::ForType<int64>())
    {
        out = BoxedValue(static_cast<int64>(std::strtoll(text.Data(), nullptr, base)));
    }
    else if (id == TypeId::ForType<uint8>())
    {
        out = BoxedValue(static_cast<uint8>(std::strtoull(text.Data(), nullptr, base)));
    }
    else if (id == TypeId::ForType<uint16>())
    {
        out = BoxedValue(static_cast<uint16>(std::strtoull(text.Data(), nullptr, base)));
    }
    else if (id == TypeId::ForType<uint32>())
    {
        out = BoxedValue(static_cast<uint32>(std::strtoull(text.Data(), nullptr, base)));
    }
    else if (id == TypeId::ForType<uint64>())
    {
        out = BoxedValue(static_cast<uint64>(std::strtoull(text.Data(), nullptr, base)));
    }
    else if (id == TypeId::ForType<char>())
    {
        out = BoxedValue(static_cast<char>(std::strtoll(text.Data(), nullptr, base)));
    }
    else
    {
        Error(MSG_INVALID_LITERAL_FOR_TYPE, tok.GetLocation(), text, TypeInfo_GetName(typeInfo).LookupString());
        return false;
    }

    return true;
}

bool Parser::ParseFloatValue(const TypeInfo& typeInfo, BoxedValue& out)
{
    Token tok = Peek();
    if (tok.GetTokenClass() != TK_FLOAT && tok.GetTokenClass() != TK_INTEGER)
    {
        Error(MSG_UNEXPECTED_TOKEN, tok.GetLocation(), Token::TokenTypeToString(tok.GetTokenClass()));
        return false;
    }

    Next();

    const String& text = tok.GetValue();
    const double parsed = std::strtod(text.Data(), nullptr);
    const TypeId id = TypeInfo_GetId(typeInfo);

    if (id == TypeId::ForType<float>())
    {
        out = BoxedValue(static_cast<float>(parsed));
    }
    else if (id == TypeId::ForType<double>())
    {
        out = BoxedValue(parsed);
    }
    else
    {
        Error(MSG_INVALID_LITERAL_FOR_TYPE, tok.GetLocation(), text, TypeInfo_GetName(typeInfo).LookupString());
        return false;
    }

    return true;
}

bool Parser::ParseStringValue(const TypeInfo& typeInfo, BoxedValue& out)
{
    Token tok = Peek();
    if (tok.GetTokenClass() != TK_STRING && tok.GetTokenClass() != TK_AT_STRING)
    {
        Error(MSG_UNEXPECTED_TOKEN, tok.GetLocation(), Token::TokenTypeToString(tok.GetTokenClass()));
        return false;
    }

    Next();

    const String& text = tok.GetValue();
    const TypeId id = TypeInfo_GetId(typeInfo);

    // Name / StringHash are stored as Name internally
    if (id == TypeId::ForType<Name>() || id == TypeId::ForType<StringHash>())
    {
        out = BoxedValue(CreateNameFromDynamicString(text.Data()));
    }
    else
    {
        out = BoxedValue(text);
    }

    return true;
}

bool Parser::ResolveEnumName(const Class* enumClass, const String& name, BoxedValue& outValue)
{
    if (!enumClass || !enumClass->IsEnumType())
    {
        return false;
    }

    if (StaticField* sf = enumClass->GetStaticField(StringHash(name)))
    {
        outValue = sf->Get();

        // NOT strict, we want to check if it is 'a' numeric type.
        if (outValue.Is<uint64>(/* strict */ false))
        {
            return true;
        }
    }

    return false;
}

bool Parser::ParseEnumValue(const TypeInfo& typeInfo, BoxedValue& out)
{
    Token tok = Peek();
    if (tok.GetTokenClass() != TK_IDENT)
    {
        Error(MSG_UNEXPECTED_TOKEN, tok.GetLocation(), Token::TokenTypeToString(tok.GetTokenClass()));

        return false;
    }

    Next();

    const Class* enumClass = typeInfo.GetClass();

    if (!ResolveEnumName(enumClass, tok.GetValue(), out))
    {
        Warning(MSG_UNRESOLVED_ENUM_NAME, tok.GetLocation(),
                enumClass ? enumClass->GetName().ToString() + "::" + tok.GetValue() : tok.GetValue());

        // Default to 0 -- still produce a value so we can keep going.
        out = BoxedValue(static_cast<uint64>(0));
    }

    return true;
}

bool Parser::ParseEnumFlagsValue(const TypeInfo& typeInfo, BoxedValue& out)
{
    const TypeInfo* enumType = typeInfo.GetEnumType();

    if (!enumType)
    {
        Error(MSG_NOT_AN_ENUM_FLAGS_TYPE, Peek().GetLocation(), TypeInfo_GetName(typeInfo).LookupString());

        return false;
    }

    const Class* enumClass = enumType->GetClass();

    Token firstTok = Peek();

    if (firstTok.GetTokenClass() != TK_IDENT)
    {
        Error(MSG_UNEXPECTED_TOKEN, firstTok.GetLocation(), Token::TokenTypeToString(firstTok.GetTokenClass()));

        return false;
    }

    Next();

    uint64 combined = 0;
    bool anyResolved = false;

    auto resolveOne = [&](const Token& nameTok)
    {
        BoxedValue boxed;
        if (ResolveEnumName(enumClass, nameTok.GetValue(), boxed))
        {
            Assert(boxed.Is<uint64>());
            combined |= boxed.Get<uint64>();
            anyResolved = true;
        }
        else
        {
            // Check for numeric
            const String& s = nameTok.GetValue();

            if (!s.Empty() && std::isdigit(s.GetChar(0)))
            {
                uint64 uValue;
                if (StringUtil::Parse(s, &uValue))
                {
                    combined |= uValue;
                    anyResolved = true;
                }
                else
                {
                    Error(MSG_UNEXPECTED_TOKEN, nameTok.GetLocation());
                }
            }
            else
            {
                Warning(MSG_UNRESOLVED_ENUM_NAME, nameTok.GetLocation(),
                        enumClass ? enumClass->GetName().ToString() + "::" + s : s);
            }
        }
    };

    resolveOne(firstTok);

    // Consume any "| Name" pairs
    while (Peek().GetTokenClass() == TK_PIPE)
    {
        Next(); // consume '|'

        Token nameTok = Peek();

        if (nameTok.GetTokenClass() != TK_IDENT)
        {
            Error(MSG_EXPECTED_IDENTIFIER, nameTok.GetLocation());

            return false;
        }

        Next();

        resolveOne(nameTok);
    }

    if (!anyResolved)
    {
        // No names resolved; leave at 0 (warning already emitted)
    }

    // Store as the enum's underlying integer type
    const TypeInfo* underlying = typeInfo.GetUnderlyingType();
    const TypeId underlyingId = underlying ? TypeInfo_GetId(*underlying) : TypeId::ForType<uint32>();

    // Select the proper type based on underlying type id
    if (underlyingId == TypeId::ForType<int8>()) { out = BoxedValue(static_cast<int8>(combined)); }
    else if (underlyingId == TypeId::ForType<int16>()) { out = BoxedValue(static_cast<int16>(combined)); }
    else if (underlyingId == TypeId::ForType<int32>()) { out = BoxedValue(static_cast<int32>(combined)); }
    else if (underlyingId == TypeId::ForType<int64>()) { out = BoxedValue(static_cast<int64>(combined)); }
    else if (underlyingId == TypeId::ForType<uint8>()) { out = BoxedValue(static_cast<uint8>(combined)); }
    else if (underlyingId == TypeId::ForType<uint16>()) { out = BoxedValue(static_cast<uint16>(combined)); }
    else if (underlyingId == TypeId::ForType<uint32>()) { out = BoxedValue(static_cast<uint32>(combined)); }
    else if (underlyingId == TypeId::ForType<uint64>()) { out = BoxedValue(static_cast<uint64>(combined)); }
    else { out = BoxedValue(static_cast<uint32>(combined)); }

    return true;
}

bool Parser::ParseVectorValue(const TypeInfo& typeInfo, BoxedValue& out)
{
    auto* handler = static_cast<ITypeInfoVectorHandler*>(typeInfo.extendedInfo.handler);
    if (!handler)
    {
        Error(MSG_INTERNAL_ERROR, Peek().GetLocation());
        return false;
    }

    if (!handler->CreateInstance(out))
    {
        Error(MSG_INTERNAL_ERROR, Peek().GetLocation());
        return false;
    }

    if (!Expect(TK_OPEN_BRACKET, "["))
    {
        return false;
    }

    const int numComponents = handler->GetNumComponents();
    const TypeInfo* elementType = typeInfo.GetElementType();

    for (int i = 0; i < numComponents; i++)
    {
        if (Peek().GetTokenClass() == TK_COMMA)
        {
            Token comma = Next();
            if (Peek().GetTokenClass() == TK_CLOSE_BRACKET)
            {
                Error(MSG_UNEXPECTED_TOKEN, comma.GetLocation(), ",");
                return false;
            }
        }

        BoxedValue component;
        if (!ParseValue(*elementType, component))
        {
            return false;
        }

        handler->SetComponent(out, i, component);
    }

    if (!Expect(TK_CLOSE_BRACKET, "]"))
    {
        return false;
    }

    return true;
}

bool Parser::ParseArrayValue(const TypeInfo& typeInfo, BoxedValue& out)
{
    if (!Expect(TK_OPEN_BRACKET, "["))
    {
        return false;
    }

    const TypeInfo* elementType = typeInfo.GetElementType();
    if (!elementType)
    {
        Error(MSG_INTERNAL_ERROR, Peek().GetLocation());
        return false;
    }

    Array<BoxedValue> elements;

    while (Peek().GetTokenClass() != TK_CLOSE_BRACKET && Peek().GetTokenClass() != TK_EMPTY)
    {
        if (Peek().GetTokenClass() == TK_COMMA)
        {
            Token comma = Next();

            if (Peek().GetTokenClass() == TK_CLOSE_BRACKET)
            {
                Error(MSG_UNEXPECTED_TOKEN, comma.GetLocation(), ",");

                return false;
            }
        }

        BoxedValue element;

        if (!ParseValue(*elementType, element))
        {
            return false;
        }

        elements.PushBack(std::move(element));
    }

    if (!Expect(TK_CLOSE_BRACKET, "]"))
    {
        return false;
    }

    // Now build the array via the handler
    auto* handler = static_cast<ITypeInfoArrayHandler*>(typeInfo.extendedInfo.handler);
    if (!handler)
    {
        Error(MSG_INTERNAL_ERROR, Peek().GetLocation());
        return false;
    }

    if (!handler->CreateInstance(out))
    {
        Error(MSG_INTERNAL_ERROR, Peek().GetLocation());
        return false;
    }

    handler->Resize(out, elements.Size());

    for (size_t i = 0; i < elements.Size(); i++)
    {
        handler->SetElementAt(out, i, std::move(elements[i]));
    }

    return true;
}

bool Parser::ParseSetValue(const TypeInfo& typeInfo, BoxedValue& out)
{
    if (!Expect(TK_OPEN_BRACKET, "["))
    {
        return false;
    }

    const TypeInfo* elementType = typeInfo.GetElementType();

    if (!elementType)
    {
        Error(MSG_INTERNAL_ERROR, Peek().GetLocation());

        return false;
    }

    auto* handler = static_cast<ITypeInfoSetHandler*>(typeInfo.extendedInfo.handler);

    if (!handler)
    {
        Error(MSG_INTERNAL_ERROR, Peek().GetLocation());

        return false;
    }

    if (!handler->CreateInstance(out))
    {
        Error(MSG_INTERNAL_ERROR, Peek().GetLocation());

        return false;
    }

    while (Peek().GetTokenClass() != TK_CLOSE_BRACKET && Peek().GetTokenClass() != TK_EMPTY)
    {
        if (Peek().GetTokenClass() == TK_COMMA)
        {
            Token comma = Next();
            
            if (Peek().GetTokenClass() == TK_CLOSE_BRACKET)
            {
                Error(MSG_UNEXPECTED_TOKEN, comma.GetLocation(), ",");

                return false;
            }
        }

        BoxedValue element;

        if (!ParseValue(*elementType, element))
        {
            return false;
        }

        handler->Insert(out, element);
    }

    if (!Expect(TK_CLOSE_BRACKET, "]"))
    {
        return false;
    }

    return true;
}

/// Map format: { "Key" = Value, "Key2" = Value2 }
bool Parser::ParseMapValue(const TypeInfo& typeInfo, BoxedValue& out)
{
    if (!Expect(TK_OPEN_BRACE, "{"))
    {
        return false;
    }

    const TypeInfo* keyType = typeInfo.GetKeyType();
    const TypeInfo* valueType = typeInfo.GetValueType();

    if (!keyType || !valueType)
    {
        Error(MSG_INTERNAL_ERROR, Peek().GetLocation());
        return false;
    }

    auto* handler = static_cast<ITypeInfoMapHandler*>(typeInfo.extendedInfo.handler);

    if (!handler)
    {
        Error(MSG_INTERNAL_ERROR, Peek().GetLocation());
        return false;
    }

    if (!handler->CreateInstance(out))
    {
        Error(MSG_INTERNAL_ERROR, Peek().GetLocation());
        return false;
    }

    while (Peek().GetTokenClass() != TK_CLOSE_BRACE && Peek().GetTokenClass() != TK_EMPTY)
    {
        if (Peek().GetTokenClass() == TK_COMMA)
        {
            Token comma = Next();

            if (Peek().GetTokenClass() == TK_CLOSE_BRACE)
            {
                Error(MSG_UNEXPECTED_TOKEN, comma.GetLocation(), ",");

                return false;
            }
        }

        // Parse key (type-directed)
        BoxedValue key;

        if (!ParseValue(*keyType, key))
        {
            return false;
        }

        if (!Expect(TK_EQUALS, "="))
        {
            return false;
        }

        // Parse value
        BoxedValue value;

        if (!ParseValue(*valueType, value))
        {
            return false;
        }

        handler->SetValueAt(out, key, value);
    }

    if (!Expect(TK_CLOSE_BRACE, "}"))
    {
        return false;
    }

    return true;
}

bool Parser::ParsePairValue(const TypeInfo& typeInfo, BoxedValue& out)
{
    if (!Expect(TK_OPEN_BRACKET, "["))
    {
        return false;
    }

    auto* handler = static_cast<ITypeInfoPairHandler*>(typeInfo.extendedInfo.handler);

    if (!handler)
    {
        Error(MSG_INTERNAL_ERROR, Peek().GetLocation());
        return false;
    }

    if (!handler->CreateInstance(out))
    {
        Error(MSG_INTERNAL_ERROR, Peek().GetLocation());
        return false;
    }

    const TypeInfo* firstType = handler->GetFirstTypeInfo();
    const TypeInfo* secondType = handler->GetSecondTypeInfo();

    BoxedValue first;
    if (!ParseValue(*firstType, first))
    {
        return false;
    }

    handler->SetFirst(out, first);

    // Optional comma between pair elements
    if (Peek().GetTokenClass() == TK_COMMA)
    {
        Next();
    }

    BoxedValue second;
    if (!ParseValue(*secondType, second))
    {
        return false;
    }

    handler->SetSecond(out, second);

    if (!Expect(TK_CLOSE_BRACKET, "]"))
    {
        return false;
    }

    return true;
}

bool Parser::ParseObjectValue(const TypeInfo& typeInfo, BoxedValue& out)
{
    const Class* declaredClass = typeInfo.GetClass();
    const Class* actualClass = declaredClass;

    // Polymorphism: IDENT followed by '{' is a runtime class override
    if (Peek().GetTokenClass() == TK_IDENT && Peek(1).GetTokenClass() == TK_OPEN_BRACE)
    {
        Token classTok = Next(); // consume IDENT
        const String& runtimeClassName = classTok.GetValue();

        actualClass = Hyperion::GetClass(StringHash(runtimeClassName));
        if (!actualClass)
        {
            Error(MSG_CLASS_NOT_FOUND, classTok.GetLocation(), runtimeClassName);
            return false;
        }

        if (declaredClass && !actualClass->IsDerivedFrom(declaredClass))
        {
            Error(MSG_CLASS_NOT_DERIVED, classTok.GetLocation(),
                runtimeClassName, declaredClass->GetName().LookupString());
            return false;
        }
    }

    if (!actualClass)
    {
        Error(MSG_INTERNAL_ERROR, Peek().GetLocation());
        return false;
    }

    if (!actualClass->CanCreateInstance())
    {
        // @TODO Better error.
        Error(MSG_CLASS_NOT_FOUND, Peek().GetLocation(), actualClass->GetName().LookupString());
        return false;
    }

    if (!Expect(TK_OPEN_BRACE, "{"))
    {
        return false;
    }

    if (!actualClass->CreateInstance(out))
    {
        Error(MSG_INTERNAL_ERROR, Peek().GetLocation());
        return false;
    }

    if (!ParseObjectBody(actualClass, out))
    {
        return false;
    }

    if (!Expect(TK_CLOSE_BRACE, "}"))
    {
        return false;
    }

    return true;
}

bool Parser::ParseTupleValue(const TypeInfo& typeInfo, BoxedValue& out)
{
    auto* handler = static_cast<ITypeInfoTupleHandler*>(typeInfo.extendedInfo.handler);

    if (!handler)
    {
        Error(MSG_INTERNAL_ERROR, Peek().GetLocation());
        return false;
    }

    if (!handler->CreateInstance(out))
    {
        Error(MSG_INTERNAL_ERROR, Peek().GetLocation());
        return false;
    }

    if (!Expect(TK_OPEN_BRACKET, "["))
    {
        return false;
    }

    const int numElements = handler->GetNumElements();

    for (int i = 0; i < numElements; i++)
    {
        if (Peek().GetTokenClass() == TK_COMMA)
        {
            Token comma = Next();

            if (Peek().GetTokenClass() == TK_CLOSE_BRACKET)
            {
                Error(MSG_UNEXPECTED_TOKEN, comma.GetLocation(), ",");
                return false;
            }
        }

        BoxedValue element;

        if (!ParseValue(*handler->GetElementTypeInfoAtIndex(i), element))
        {
            return false;
        }

        handler->SetElement(out, i, element);
    }

    if (!Expect(TK_CLOSE_BRACKET, "]"))
    {
        return false;
    }

    return true;
}

bool Parser::ParseMatrixValue(const TypeInfo& typeInfo, BoxedValue& out)
{
    auto* handler = static_cast<ITypeInfoMatrixHandler*>(typeInfo.extendedInfo.handler);

    if (!handler)
    {
        Error(MSG_INTERNAL_ERROR, Peek().GetLocation());
        return false;
    }

    if (!handler->CreateInstance(out))
    {
        Error(MSG_INTERNAL_ERROR, Peek().GetLocation());
        return false;
    }

    if (!Expect(TK_OPEN_BRACKET, "["))
    {
        return false;
    }

    const int numRows = handler->GetNumRows();
    const int numCols = handler->GetNumColumns();
    const TypeInfo* elementType = typeInfo.GetElementType();

    for (int row = 0; row < numRows; row++)
    {
        if (row > 0)
        {
            if (Peek().GetTokenClass() == TK_COMMA)
            {
                Next();
            }
        }

        // Each row is a sub-array: [v, v, v]
        if (!Expect(TK_OPEN_BRACKET, "["))
        {
            return false;
        }

        for (int col = 0; col < numCols; col++)
        {
            if (col > 0)
            {
                if (Peek().GetTokenClass() == TK_COMMA)
                {
                    Next();
                }
            }

            BoxedValue element;

            if (!ParseValue(*elementType, element))
            {
                return false;
            }

            handler->SetElement(out, row, col, element);
        }

        if (!Expect(TK_CLOSE_BRACKET, "]"))
        {
            return false;
        }
    }

    if (!Expect(TK_CLOSE_BRACKET, "]"))
    {
        return false;
    }

    return true;
}

bool Parser::ParseVariantValue(const TypeInfo& typeInfo, BoxedValue& out)
{
    auto* handler = static_cast<ITypeInfoVariantHandler*>(typeInfo.extendedInfo.handler);

    if (!handler)
    {
        Error(MSG_INTERNAL_ERROR, Peek().GetLocation());
        return false;
    }

    // null denotes uninitialized variant
    if (Peek().GetTokenClass() == TK_IDENT && Peek().GetValue() == "null")
    {
        Next();

        if (!handler->CreateInstance(out))
        {
            Error(MSG_INTERNAL_ERROR, Peek().GetLocation());
            return false;
        }

        return true;
    }

    BoxedValue variantInstance;

    if (!handler->CreateInstance(variantInstance))
    {
        Error(MSG_INTERNAL_ERROR, Peek().GetLocation());
        return false;
    }

    const int numTypes = handler->GetNumTypes();

    auto trySet = [&]() -> bool
    {
        for (int i = 0; i < numTypes; i++)
        {
            const TypeInfo* alternativeTypeInfo = handler->GetTypeInfoAtIndex(i);

            if (!alternativeTypeInfo)
            {
                continue;
            }

            const size_t savedPos = m_tokenStream->GetPosition();

            BoxedValue parsedValue;

            if (ParseValue(*alternativeTypeInfo, parsedValue))
            {
                if (handler->SetValue(variantInstance, parsedValue))
                {
                    out = variantInstance;
                    return true;
                }


            }
            m_tokenStream->SetPosition(savedPos);
        }

        return false;
    };

    if (trySet())
    {
        return true;
    }

    Error(MSG_UNKNOWN_VARIANT_TAG, Peek().GetLocation(), String("could not match any alternative type"));

    return false;
}

bool Parser::ParseAssetPathLiteral(BoxedValue& out)
{
    Token strTok = Peek();

    if (strTok.GetTokenClass() != TK_AT_STRING)
    {
        Error(MSG_UNEXPECTED_TOKEN, strTok.GetLocation(), Token::TokenTypeToString(strTok.GetTokenClass()));

        return false;
    }

    Next();

    // @TODO VERIFY
    // Store as String; the Engine-side Property::Set handles conversion to
    // AssetPath/AssetReference via their "Value"/"AssetPath" properties.
    out = BoxedValue(strTok.GetValue());

    return true;
}

void Parser::SkipValue()
{
    // Depth-tracking skip: consumes tokens that constitute a single value, stopping at
    // the next field boundary (IDENT '=') or at an unmatched closing brace/bracket.
    // This works without newline tokens because in HMF, '=' appears only as a field
    // separator, so "IDENT '='" unambiguously signals the start of the next field.
    bool consumedAny = false;
    int depth = 0;

    while (true)
    {
        Token tok = Peek();
        if (tok.GetTokenClass() == TK_EMPTY)
        {
            return;
        }

        // Boundary checks (only after we've consumed at least one token of the value)
        if (consumedAny && depth == 0)
        {
            const TokenClass tc = tok.GetTokenClass();

            // End of containing object or array
            if (tc == TK_CLOSE_BRACE || tc == TK_CLOSE_BRACKET)
            {
                return;
            }

            // Start of the next field in the containing object: IDENT followed by '='
            if (tc == TK_IDENT && Peek(1).GetTokenClass() == TK_EQUALS)
            {
                return;
            }

            // Optional comma between fields/elements belongs to the container, not the value
            if (tc == TK_COMMA)
            {
                return;
            }
        }

        // Track nesting
        const TokenClass tc = tok.GetTokenClass();

        if (tc == TK_OPEN_BRACE || tc == TK_OPEN_BRACKET)
        {
            depth++;
        }
        else if (tc == TK_CLOSE_BRACE || tc == TK_CLOSE_BRACKET)
        {
            if (depth == 0)
            {
                // Unmatched closer -- not part of our value
                return;
            }

            depth--;
        }

        Next();
        consumedAny = true;
    }
}

void Parser::SkipBracedBlock()
{
    if (!Match(TK_OPEN_BRACE))
    {
        return;
    }

    int depth = 1;

    while (depth > 0 && Peek().GetTokenClass() != TK_EMPTY)
    {
        Token tok = Next();

        if (tok.GetTokenClass() == TK_OPEN_BRACE)
        {
            depth++;
        }
        else if (tok.GetTokenClass() == TK_CLOSE_BRACE)
        {
            depth--;
        }
    }
}

void Parser::SkipBracketedBlock()
{
    if (!Match(TK_OPEN_BRACKET))
    {
        return;
    }

    int depth = 1;
    while (depth > 0 && Peek().GetTokenClass() != TK_EMPTY)
    {
        Token tok = Next();

        if (tok.GetTokenClass() == TK_OPEN_BRACE)
        {
            depth++;
        }
        else if (tok.GetTokenClass() == TK_CLOSE_BRACE)
        {
            depth--;
        }
    }
}

bool Parser::Match(TokenClass tokenClass)
{
    if (Peek().GetTokenClass() == tokenClass)
    {
        Next();
        return true;
    }

    return false;
}

bool Parser::Expect(TokenClass tokenClass, const char* what)
{
    if (Peek().GetTokenClass() == tokenClass)
    {
        Next();
        return true;
    }

    Error(MSG_EXPECTED_TOKEN, Peek().GetLocation(), String(what));
    return false;
}

bool Parser::ExpectIdentifier(String& outName)
{
    Token tok = Peek();
    if (tok.GetTokenClass() != TK_IDENT)
    {
        Error(MSG_EXPECTED_IDENTIFIER, tok.GetLocation());
        return false;
    }
    Next();
    outName = tok.GetValue();
    return true;
}

bool Parser::IsIdentKeyword(const Token& token, const char* keyword) const
{
    return token.GetTokenClass() == TK_IDENT && token.GetValue() == keyword;
}

void Parser::Error(ErrorMessage msg, const SourceLocation& loc)
{
    m_compilationUnit->GetErrorList().AddError(CompilerError(LEVEL_ERROR, msg, loc));
}

void Parser::Error(ErrorMessage msg, const SourceLocation& loc, const String& arg1)
{
    m_compilationUnit->GetErrorList().AddError(CompilerError(LEVEL_ERROR, msg, loc, arg1));
}

void Parser::Error(ErrorMessage msg, const SourceLocation& loc, const String& arg1, const String& arg2)
{
    m_compilationUnit->GetErrorList().AddError(CompilerError(LEVEL_ERROR, msg, loc, arg1, arg2));
}

void Parser::Warning(ErrorMessage msg, const SourceLocation& loc, const String& arg1)
{
    m_compilationUnit->GetErrorList().AddError(CompilerError(LEVEL_WARN, msg, loc, arg1));
}

} // namespace Hyperion::HMF
