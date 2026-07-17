/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <cstdlib>

#include <Core/DataProcessing/HMF/HMF.hpp>
#include <Core/DataProcessing/Shared/SourceFile.hpp>
#include <Core/DataProcessing/Shared/SourceStream.hpp>
#include <Core/DataProcessing/HMF/Parser/TokenStream.hpp>
#include <Core/DataProcessing/HMF/Parser/ErrorList.hpp>
#include <Core/DataProcessing/HMF/Parser/Lexer.hpp>
#include <Core/DataProcessing/HMF/Parser/Parser.hpp>

namespace Hyperion::HMF {

ResolveAssetPathFn g_resolveAssetPath = nullptr;

namespace {

ParseResult RunParse(const SourceFile& sourceFile, ErrorList* outErrors)
{
    ErrorList errorList;
    SourceStream sourceStream(&sourceFile);
    TokenStream tokenStream(TokenStreamInfo(sourceFile.GetFilePath()));

    Lexer lexer(sourceStream, &tokenStream, &errorList);
    lexer.Analyze();

    Parser parser(&tokenStream, &errorList);

    ParseResult result = HYP_MAKE_ERROR(Error, "Failed due to unknown error");

    {
        BoxedValue value;
        if (parser.Parse(value))
        {
            result = std::move(value);
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

ParseResult Parse(const FilePath& filePath, const String& source, ErrorList* outErrors)
{
    SourceFile sourceFile(filePath, source.Size());
    sourceFile.ReadIntoBuffer(reinterpret_cast<const ubyte*>(source.Data()), source.Size());

    return RunParse(sourceFile, outErrors);
}

ParseResult Parse(const String& source, ErrorList* outErrors)
{
    return Parse(FilePath("<memory-buffer>"), source, outErrors);
}

ParseResult Parse(const SourceFile& sourceFile)
{
    return RunParse(sourceFile, nullptr);
}

} // namespace Hyperion::HMF
