/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Containers/String.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <Core/Reflection/BoxedValue.hpp>

#include <Core/Utilities/Result.hpp>

#include <Core/Defines.hpp>


namespace Hyperion::DataProcessing {

template <class TErrorType>
class ErrorList;

class SourceFile;
class CompilerError;

} // namespace Hyperion::DataProcessing

namespace Hyperion::DataProcessing::HMF {

using ErrorList = ::Hyperion::DataProcessing::ErrorList<CompilerError>;

using ParseResult = TResult<BoxedValue>;

using ResolveAssetPathFn = bool (*)(const String& path, const TypeInfo& targetType, BoxedValue& out);

CORE_API extern ResolveAssetPathFn g_resolveAssetPath;

CORE_API ParseResult Parse(const FilePath& filePath, const String& source, ErrorList* outErrors = nullptr);
CORE_API ParseResult Parse(const String& source, ErrorList* outErrors = nullptr);

CORE_API ParseResult Parse(const SourceFile& sourceFile);

} // namespace Hyperion::DataProcessing::HMF

namespace Hyperion {
namespace HMF = DataProcessing::HMF;
} // namespace Hyperion
