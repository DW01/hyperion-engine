#pragma once

#include <Core/DataProcessing/Shared/SourceFile.hpp>
#include <Core/Unicode.hpp>

namespace Hyperion::DataProcessing {

class SourceStream
{
public:
    SourceStream(const SourceFile* file);
    SourceStream(const SourceStream& other);

    const SourceFile* GetFile() const { return m_file; }
    size_t GetPosition() const { return m_position; }
    bool HasNext() const { return m_position < m_file->GetSize(); }

    utf::Char32 Peek() const;
    utf::Char32 Next();
    utf::Char32 Next(int& posChange);
    void GoBack(int n = 1);
    void Read(char* ptr, size_t numBytes);

private:
    const SourceFile* m_file;
    size_t m_position;
};

} // namespace Hyperion::DataProcessing
