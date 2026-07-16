/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <cstdlib>

#include <Core/HMF/HMF.hpp>
#include <Core/HMF/Parser/SourceFile.hpp>
#include <Core/HMF/Parser/SourceStream.hpp>
#include <Core/HMF/Parser/TokenStream.hpp>
#include <Core/HMF/Parser/CompilationUnit.hpp>
#include <Core/HMF/Parser/Lexer.hpp>
#include <Core/HMF/Parser/Parser.hpp>

namespace Hyperion::HMF {

namespace {

ParseResult RunParse(const SourceFile& sourceFile, ErrorList* outErrors)
{
    CompilationUnit compilationUnit;
    SourceStream sourceStream(&sourceFile);
    TokenStream tokenStream(TokenStreamInfo(sourceFile.GetFilePath()));

    Lexer lexer(sourceStream, &tokenStream, &compilationUnit);
    lexer.Analyze();

    Parser parser(&tokenStream, &compilationUnit);

    BoxedValue value;
    const bool ok = parser.Parse(value);

    ParseResult result;
    result.ok = ok;
    result.value = std::move(value);

    if (!ok)
    {
        if (compilationUnit.GetErrorList().Size() != 0)
        {
            result.message = compilationUnit.GetErrorList()[0].GetText();
        }
        else
        {
            result.message = "Failed to parse HMF document";
        }
    }

    if (outErrors)
    {
        outErrors->Concatenate(compilationUnit.GetErrorList());
    }

    return result;
}

} // namespace anonymous

ParseResult Parse(const String& source, ErrorList* outErrors)
{
    SourceFile sourceFile("<string>", source.Size());
    sourceFile.ReadIntoBuffer(reinterpret_cast<const ubyte*>(source.Data()), source.Size());

    return RunParse(sourceFile, outErrors);
}

ParseResult Parse(const String& source)
{
    return Parse(source, nullptr);
}

ParseResult Parse(const SourceFile& sourceFile)
{
    return RunParse(sourceFile, nullptr);
}

} // namespace Hyperion::HMF
