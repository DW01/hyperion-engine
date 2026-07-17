#include <Core/DataProcessing/Shared/SourceFile.hpp>
#include <Core/Memory/Memory.hpp>

namespace Hyperion::DataProcessing {

SourceFile::SourceFile()
    : m_filepath("??"),
      m_position(0)
{
}

SourceFile::SourceFile(const String& filepath, size_t size)
    : m_filepath(filepath),
      m_position(0)
{
    m_buffer.SetSize(size);
}

SourceFile::SourceFile(const SourceFile& other)
    : m_filepath(other.m_filepath),
      m_buffer(other.m_buffer),
      m_position(other.m_position)
{
}

SourceFile& SourceFile::operator=(const SourceFile& other)
{
    if (this != &other)
    {
        m_filepath = other.m_filepath;
        m_buffer = other.m_buffer;
        m_position = other.m_position;
    }

    return *this;
}

SourceFile::~SourceFile() = default;

void SourceFile::ReadIntoBuffer(const ByteBuffer& inputBuffer)
{
    m_buffer = inputBuffer;
}

void SourceFile::ReadIntoBuffer(const ubyte* data, size_t size)
{
    m_buffer.SetSize(size);
    Memory::Copy(m_buffer.Data(), data, size);
}

} // namespace Hyperion::DataProcessing
