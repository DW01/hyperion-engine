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

#include <Core/DataProcessing/Shared/SourceFile.hpp>

namespace Hyperion::HMF {

using Hyperion::DataProcessing::SourceFile;

class ErrorList;

using ParseResult = TResult<BoxedValue>;

using ResolveAssetPathFn = bool (*)(const String& path, const TypeInfo& targetType, BoxedValue& out);

CORE_API extern ResolveAssetPathFn g_resolveAssetPath;

CORE_API ParseResult Parse(const FilePath& filePath, const String& source, ErrorList* outErrors = nullptr);
CORE_API ParseResult Parse(const String& source, ErrorList* outErrors = nullptr);

CORE_API ParseResult Parse(const SourceFile& sourceFile);

} // namespace Hyperion::HMF