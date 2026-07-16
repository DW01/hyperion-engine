/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Containers/String.hpp>
#include <Core/Reflection/BoxedValue.hpp>
#include <Core/Utilities/Result.hpp>
#include <Core/Defines.hpp>

namespace Hyperion::HMF {

class SourceFile;
class ErrorList;

/*! \brief Result of parsing an HMF document.
 *  On success, \ref ok is true and \ref value holds the typed BoxedValue produced by the type-directed parser.
 *  On failure, \ref ok is false and \ref message contains a description of the first error. */
struct CORE_API ParseResult
{
    bool ok = false;
    String message;
    BoxedValue value;
};

/*! \brief Parse an HMF manifest from a string.
 *  The manifest must be of the form `ClassName "name"? { fields }`.
 *  The class is looked up via the reflection registry and an instance is constructed and populated. */
CORE_API ParseResult Parse(const String& source);

/*! \brief Parse an HMF manifest from a SourceFile. */
CORE_API ParseResult Parse(const SourceFile& sourceFile);

/*! \brief Parse an HMF manifest from a string, capturing the full ErrorList (for diagnostics).
 *  \param outErrors If non-null, receives all errors and warnings generated during parsing. */
CORE_API ParseResult Parse(const String& source, ErrorList* outErrors);

} // namespace Hyperion::HMF
