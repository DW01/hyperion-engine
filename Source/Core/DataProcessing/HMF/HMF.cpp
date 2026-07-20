/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <cstdlib>

#include <Core/DataProcessing/HMF/HMF.hpp>

#include <Core/DataProcessing/Shared/SourceFile.hpp>
#include <Core/DataProcessing/Shared/SourceStream.hpp>
#include <Core/DataProcessing/Shared/TokenStream.hpp>
#include <Core/DataProcessing/Shared/ErrorList.hpp>
#include <Core/DataProcessing/Shared/Lexer.hpp>

#include <Core/DataProcessing/HMF/Parser/Parser.hpp>

namespace Hyperion::DataProcessing::HMF {

ResolveAssetPathFn g_resolveAssetPath = nullptr;

static const FilePath s_inMemoryFilePath = FilePath("<memory-buffer>");

namespace {

ParseResult RunParse(const SourceFile& sourceFile, ErrorList* outErrors, BoxedValue* target = nullptr)
{
    ErrorList errorList;
    SourceStream sourceStream(&sourceFile);
    TokenStream tokenStream(TokenStreamInfo(sourceFile.GetFilePath()));

    Lexer lexer(sourceStream, &tokenStream, &errorList);
    lexer.Analyze();

    Parser parser(&tokenStream, &errorList, target);

    ParseResult result = HYP_MAKE_ERROR(Error, "Failed due to unknown error");

    if (target != nullptr)
    {
        if (parser.Parse())
        {
            // ok
            result = *target;
        }
    }
    else
    {
        // grab value from parse result
        BoxedValue boxedResultValue;
        
        if (parser.Parse(boxedResultValue, /* moveResult */ true))
        {
            result = std::move(boxedResultValue);
        }
    }

    if (Failed(result))
    {
        if (errorList.Size() != 0)
        {
            const CompilerError& firstError = errorList[0];

            result = HYP_MAKE_ERROR(Error, "{} in file {}, line {} col {}",
                firstError.GetText(),
                firstError.GetLocation().GetFileName(),
                firstError.GetLocation().GetLine(),
                firstError.GetLocation().GetColumn());
        }
    }

    if (outErrors)
    {
        outErrors->Concatenate(errorList);
    }

    return result;
}

} // namespace anonymous

ParseResult Parse(const FilePath& filePath, const String& source, ErrorList* outErrors, BoxedValue* target)
{
    SourceFile sourceFile(filePath, source.Size());
    sourceFile.ReadIntoBuffer(reinterpret_cast<const ubyte*>(source.Data()), source.Size());

    return RunParse(sourceFile, outErrors, target);
}

ParseResult Parse(const String& source, ErrorList* outErrors, BoxedValue* target)
{
    return Parse(s_inMemoryFilePath, source, outErrors, target);
}

ParseResult Parse(const SourceFile& sourceFile, BoxedValue* target)
{
    return RunParse(sourceFile, nullptr, target);
}

} // namespace Hyperion::DataProcessing::HMF
