/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/DataProcessing/Shared/SourceLocation.hpp>

#include <Core/Containers/String.hpp>
#include <Core/Containers/Map.hpp>

#include <Core/Utilities/Format.hpp>

namespace Hyperion::HMF {

// Error starts at zero so when we sort the error list, they appear first in the list.

enum class ErrorLevel : uint8
{
    Error = 0,
    Warning,
    Diagnostic
};

enum ErrorMessage
{
    /* Generic / lexical errors */
    MSG_INTERNAL_ERROR,
    MSG_CUSTOM_ERROR,
    MSG_NOT_IMPLEMENTED,
    MSG_ILLEGAL_SYNTAX,
    MSG_UNEXPECTED_CHARACTER,
    MSG_UNEXPECTED_IDENTIFIER,
    MSG_UNEXPECTED_TOKEN,
    MSG_UNEXPECTED_EOF,
    MSG_UNEXPECTED_EOL,
    MSG_UNRECOGNIZED_ESCAPE_SEQUENCE,
    MSG_UNTERMINATED_STRING_LITERAL,
    MSG_EXPECTED_IDENTIFIER,
    MSG_EXPECTED_TOKEN,

    /* HMF-specific semantic errors */
    MSG_UNKNOWN_FIELD,            // class '%' has no field '%'
    MSG_CANNOT_ASSIGN_PROPERTY,   // property '%' is not assignable
    MSG_UNRESOLVED_ENUM_NAME,     // enum '%' has no value named '%'
    MSG_TYPE_MISMATCH,            // value of type '%' is not assignable to '%'
    MSG_CLASS_NOT_FOUND,          // class '%' is not registered
    MSG_CLASS_NOT_DERIVED,        // class '%' is not derived from '%'
    MSG_NOT_AN_ENUM_FLAGS_TYPE,   // flag-list syntax used on non-EnumFlags type '%'
    MSG_NOT_AN_ENUM_TYPE,         // enum bareword used on non-enum type '%'
    MSG_UNKNOWN_VARIANT_TAG,      // variant has no type tagged '%'
    MSG_INVALID_LITERAL_FOR_TYPE, // literal '%' is not valid for type '%'
    MSG_UNBALANCED_BRACES,        // expected '}'
    MSG_UNBALANCED_BRACKETS,      // expected ']'
    MSG_UNKNOWN_ASSET_PATH        // asset path '%' could not be resolved
};

class CORE_API CompilerError
{
    static const Map<ErrorMessage, String> errorMessageStrings;

public:
    template <typename... Args>
    CompilerError(ErrorLevel level, ErrorMessage msg, const SourceLocation& location, const Args&... args)
        : m_level(level),
          m_msg(msg),
          m_location(location)
    {
        const String& msgStr = errorMessageStrings.At(m_msg);
        MakeMessage(msgStr.Data(), args...);
    }

    CompilerError(const CompilerError& other);
    ~CompilerError() = default;

    HYP_NODISCARD HYP_FORCE_INLINE ErrorLevel GetLevel() const
    {
        return m_level;
    }

    HYP_NODISCARD HYP_FORCE_INLINE ErrorMessage GetMessage() const
    {
        return m_msg;
    }

    HYP_NODISCARD HYP_FORCE_INLINE const SourceLocation& GetLocation() const
    {
        return m_location;
    }

    HYP_NODISCARD HYP_FORCE_INLINE const String& GetText() const
    {
        return m_text;
    }

    HYP_NODISCARD HYP_FORCE_INLINE bool operator==(const CompilerError& other) const
    {
        return m_level == other.m_level
            && m_msg == other.m_msg
            && m_location == other.m_location
            && m_text == other.m_text;
    }

    HYP_NODISCARD HYP_FORCE_INLINE bool operator!=(const CompilerError& other) const
    {
        return !(*this == other);
    }

    bool operator<(const CompilerError& other) const;

private:
    void MakeMessage(const char* format)
    {
        m_text += format;
    }

    template <typename T, typename... Args>
    void MakeMessage(const char* format, const T& value, Args&&... args)
    {
        for (; *format; format++)
        {
            if (*format == '%')
            {
                m_text += HYP_FORMAT("{}", value);
                MakeMessage(format + 1, std::forward<Args>(args)...);
                return;
            }

            m_text += *format;
        }
    }

    ErrorLevel m_level;
    ErrorMessage m_msg;
    SourceLocation m_location;
    String m_text;
};

} // namespace Hyperion::HMF
