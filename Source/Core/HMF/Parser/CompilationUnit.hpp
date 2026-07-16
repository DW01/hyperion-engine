/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/HMF/Parser/ErrorList.hpp>

namespace Hyperion::HMF {

class CompilationUnit
{
public:
    CompilationUnit();
    CompilationUnit(const CompilationUnit& other) = delete;
    ~CompilationUnit();

    ErrorList& GetErrorList()
    {
        return m_errorList;
    }

    const ErrorList& GetErrorList() const
    {
        return m_errorList;
    }

private:
    ErrorList m_errorList;
};

} // namespace Hyperion::HMF
