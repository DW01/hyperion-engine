#pragma once

#include <Core/DataProcessing/Shared/ErrorList.hpp>
#include <Core/DataProcessing/Shared/CompilerError.hpp>

namespace Hyperion::DataProcessing::JSON {

class CompilationUnit
{
public:
    CompilationUnit();
    CompilationUnit(const CompilationUnit& other) = delete;
    ~CompilationUnit();

    DataProcessing::ErrorList<DataProcessing::CompilerError>& GetErrorList()
    {
        return m_errorList;
    }

    const DataProcessing::ErrorList<DataProcessing::CompilerError>& GetErrorList() const
    {
        return m_errorList;
    }

private:
    DataProcessing::ErrorList<DataProcessing::CompilerError> m_errorList;
};

} // namespace Hyperion::DataProcessing::JSON
