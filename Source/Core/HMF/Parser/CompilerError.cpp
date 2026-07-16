/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/HMF/Parser/CompilerError.hpp>

namespace Hyperion::HMF {

const Map<ErrorMessage, String> CompilerError::errorMessageStrings {
    /* Generic / lexical errors */
    { MSG_INTERNAL_ERROR, "Internal error" },
    { MSG_CUSTOM_ERROR, "%" },
    { MSG_NOT_IMPLEMENTED, "Feature '%' not implemented." },
    { MSG_ILLEGAL_SYNTAX, "Illegal syntax" },
    { MSG_UNEXPECTED_CHARACTER, "Unexpected character '%'" },
    { MSG_UNEXPECTED_IDENTIFIER, "Unexpected identifier '%'" },
    { MSG_UNEXPECTED_TOKEN, "Unexpected token '%'" },
    { MSG_UNEXPECTED_EOF, "Unexpected end of file" },
    { MSG_UNEXPECTED_EOL, "Unexpected end of line" },
    { MSG_UNRECOGNIZED_ESCAPE_SEQUENCE, "Unrecognized escape sequence '%'" },
    { MSG_UNTERMINATED_STRING_LITERAL, "Unterminated string quotes" },
    { MSG_EXPECTED_IDENTIFIER, "Expected an identifier" },
    { MSG_EXPECTED_TOKEN, "Expected '%'" },

    /* HMF-specific semantic errors */
    { MSG_UNKNOWN_FIELD, "Class '%' has no field named '%'" },
    { MSG_UNRESOLVED_ENUM_NAME, "Enum '%' has no value named '%'" },
    { MSG_TYPE_MISMATCH, "Value of type '%' is not assignable to '%'" },
    { MSG_CLASS_NOT_FOUND, "Class '%' is not registered" },
    { MSG_CLASS_NOT_DERIVED, "Class '%' is not derived from '%'" },
    { MSG_NOT_AN_ENUM_FLAGS_TYPE, "Flag-list syntax used on non-EnumFlags type '%'" },
    { MSG_NOT_AN_ENUM_TYPE, "Enum bareword used on non-enum type '%'" },
    { MSG_UNKNOWN_VARIANT_TAG, "Variant has no type tagged '%'" },
    { MSG_INVALID_LITERAL_FOR_TYPE, "Literal '%' is not valid for type '%'" },
    { MSG_UNBALANCED_BRACES, "Expected '}'" },
    { MSG_UNBALANCED_BRACKETS, "Expected ']'" },
    { MSG_UNKNOWN_ASSET_PATH, "Asset path '%' could not be resolved" }
};

CompilerError::CompilerError(const CompilerError& other)
    : m_level(other.m_level),
      m_msg(other.m_msg),
      m_location(other.m_location),
      m_text(other.m_text)
{
}

bool CompilerError::operator<(const CompilerError& other) const
{
    if (m_level != other.m_level)
    {
        return m_level < other.m_level;
    }

    if (m_location.GetFileName() != other.m_location.GetFileName())
    {
        return m_location.GetFileName() < other.m_location.GetFileName();
    }

    if (m_location.GetLine() != other.m_location.GetLine())
    {
        return m_location.GetLine() < other.m_location.GetLine();
    }

    if (m_location.GetColumn() != other.m_location.GetColumn())
    {
        return m_location.GetColumn() < other.m_location.GetColumn();
    }

    return m_text < other.m_text;
}

} // namespace Hyperion::HMF
