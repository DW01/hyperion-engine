/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/HMF/Parser/SourceFile.hpp>

#include <Core/Debug/Debug.hpp>

#include <cstring>

namespace Hyperion::HMF {

SourceFile::SourceFile()
    : m_filePath(),
      m_position(0)
{
}

SourceFile::SourceFile(const FilePath& filePath, size_t size)
    : m_filePath(filePath),
      m_buffer(size),
      m_position(0)
{
}

SourceFile::SourceFile(const SourceFile& other)
    : m_filePath(other.m_filePath),
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
    m_filePath = other.m_filePath;

    return *this;
}

SourceFile::~SourceFile() = default;

void SourceFile::ReadIntoBuffer(const ByteBuffer& inputBuffer)
{
    ReadIntoBuffer(inputBuffer.Data(), inputBuffer.Size());
}

void SourceFile::ReadIntoBuffer(const ubyte* data, size_t size)
{
    HYP_CORE_ASSERT(m_buffer.Size() >= size);

    // make sure we have enough space in the buffer
    if (m_buffer.Size() < m_position + size)
    {
        HYP_FAIL("not enough space in buffer");
    }

    Memory::Copy(m_buffer.Data() + m_position, data, size);
    m_position += size;
}

} // namespace Hyperion::HMF
