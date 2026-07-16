#include <Lang/SourceFile.hpp>

#include <Core/Debug/Debug.hpp>

#include <cstring>

namespace Hyperion {

SourceFile::SourceFile()
    : m_filepath(),
      m_position(0)
{
}

SourceFile::SourceFile(const FilePath& filepath, size_t size)
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
    if (&other == this)
    {
        return *this;
    }

    m_buffer = other.m_buffer;
    m_position = other.m_position;
    m_filepath = other.m_filepath;

    return *this;
}

SourceFile::~SourceFile() = default;

void SourceFile::ReadIntoBuffer(const ByteBuffer& inputBuffer)
{
    ReadIntoBuffer(inputBuffer.Data(), inputBuffer.Size());
}

void SourceFile::ReadIntoBuffer(const ubyte* data, size_t size)
{
    Assert(m_buffer.Size() >= size);

    // make sure we have enough space in the buffer
    if (m_buffer.Size() < m_position + size)
    {
        HYP_FAIL("not enough space in buffer");
    }

    Memory::Copy(m_buffer.Data() + m_position, data, size);
    m_position += size;
}

} // namespace Hyperion
