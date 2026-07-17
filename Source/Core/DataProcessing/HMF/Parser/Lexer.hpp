/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/DataProcessing/Shared/SourceStream.hpp>
#include <Core/DataProcessing/HMF/Parser/TokenStream.hpp>
#include <Core/DataProcessing/Shared/SourceLocation.hpp>
#include <Core/DataProcessing/HMF/Parser/ErrorList.hpp>

#include <Core/Unicode.hpp>

namespace Hyperion::HMF {

class CORE_API Lexer
{
public:
    Lexer(
        const SourceStream& sourceStream,
        TokenStream* tokenStream,
        ErrorList* errorList);
    Lexer(const Lexer& other);

    /** Forms the given TokenStream from the given SourceStream */
    void Analyze();
    /** Reads the next token and returns it */
    Token NextToken();
    /** Reads two characters that make up an escape code and returns actual value */
    utf::Char32 ReadEscapeCode();
    /** Reads a string literal and returns the token */
    Token ReadStringLiteral(TokenClass tokenClass = TK_STRING);
    /** Reads an asset-path literal @"..." and returns the token */
    Token ReadAssetPathLiteral();
    /** Reads a number literal and returns the token */
    Token ReadNumberLiteral();
    /** Reads a hex number literal and returns the token */
    Token ReadHexNumberLiteral();
    /** Reads a single-line comment */
    Token ReadLineComment();
    /** Reads a multi-line block comment */
    Token ReadBlockComment();
    /** Reads the name, and returns the identifier token */
    Token ReadIdentifier();

private:
    SourceStream m_sourceStream;
    TokenStream* m_tokenStream;
    ErrorList* m_errorList;
    SourceLocation m_sourceLocation;

    /** Adds an end-of-file error if at the end, returns true if not */
    bool HasNext();
    /** Reads until there is no more whitespace.
        Returns true if a newline character was encountered.
    */
    bool SkipWhitespace();
};

} // namespace Hyperion::HMF
