#include <Core/DataProcessing/Shared/SourceStream.hpp>
#include <Core/Debug/Debug.hpp>

namespace Hyperion::DataProcessing {

SourceStream::SourceStream(const SourceFile* file)
    : m_file(file),
      m_position(0)
{
}

SourceStream::SourceStream(const SourceStream& other)
    : m_file(other.m_file),
      m_position(other.m_position)
{
}

utf::Char32 SourceStream::Peek() const
{
    size_t pos = m_position;
    if (pos >= m_file->GetSize())
    {
        return '\0';
    }

    char ch = static_cast<char>(m_file->GetBuffer()[pos]);

    utf::Char32 u32Ch = 0;
    char* bytes = utf::ToUtf8Chars(u32Ch);

    const unsigned char uc = static_cast<unsigned char>(ch);

    if (uc >= 0 && uc <= 127)
    {
        bytes[0] = ch;
    }
    else if ((uc & 0xE0) == 0xC0)
    {
        bytes[0] = ch;
        bytes[1] = static_cast<char>(m_file->GetBuffer()[pos + 1]);
    }
    else if ((uc & 0xF0) == 0xE0)
    {
        bytes[0] = ch;
        bytes[1] = static_cast<char>(m_file->GetBuffer()[pos + 1]);
        bytes[2] = static_cast<char>(m_file->GetBuffer()[pos + 2]);
    }
    else if ((uc & 0xF8) == 0xF0)
    {
        bytes[0] = ch;
        bytes[1] = static_cast<char>(m_file->GetBuffer()[pos + 1]);
        bytes[2] = static_cast<char>(m_file->GetBuffer()[pos + 2]);
        bytes[3] = static_cast<char>(m_file->GetBuffer()[pos + 3]);
    }
    else
    {
        u32Ch = utf::Char32('\0');
    }

    return u32Ch;
}

utf::Char32 SourceStream::Next()
{
    int tmp;
    return Next(tmp);
}

utf::Char32 SourceStream::Next(int& posChange)
{
    int posBefore = static_cast<int>(m_position);

    if (m_position >= m_file->GetSize())
    {
        return utf::Char32('\0');
    }

    char ch = static_cast<char>(m_file->GetBuffer()[m_position++]);

    utf::Char32 u32Ch = 0;
    char* bytes = utf::ToUtf8Chars(u32Ch);

    const unsigned char uc = static_cast<unsigned char>(ch);

    if (uc >= 0 && uc <= 127)
    {
        bytes[0] = ch;
    }
    else if ((uc & 0xE0) == 0xC0)
    {
        bytes[0] = ch;
        bytes[1] = static_cast<char>(m_file->GetBuffer()[m_position++]);
    }
    else if ((uc & 0xF0) == 0xE0)
    {
        bytes[0] = ch;
        bytes[1] = static_cast<char>(m_file->GetBuffer()[m_position++]);
        bytes[2] = static_cast<char>(m_file->GetBuffer()[m_position++]);
    }
    else if ((uc & 0xF8) == 0xF0)
    {
        bytes[0] = ch;
        bytes[1] = static_cast<char>(m_file->GetBuffer()[m_position++]);
        bytes[2] = static_cast<char>(m_file->GetBuffer()[m_position++]);
        bytes[3] = static_cast<char>(m_file->GetBuffer()[m_position++]);
    }
    else
    {
        u32Ch = utf::Char32('\0');
    }

    posChange = static_cast<int>(m_position) - posBefore;

    return u32Ch;
}

void SourceStream::GoBack(int n)
{
    HYP_CORE_ASSERT((static_cast<int>(m_position) - n) >= 0, "not large enough to go back");

    m_position -= n;
}

void SourceStream::Read(char* ptr, size_t numBytes)
{
    HYP_CORE_ASSERT(m_position + numBytes < m_file->GetSize(), "attempted to read past the limit");

    for (size_t i = 0; i < numBytes; i++)
    {
        ptr[i] = static_cast<char>(m_file->GetBuffer()[m_position++]);
    }
}

} // namespace Hyperion::DataProcessing
