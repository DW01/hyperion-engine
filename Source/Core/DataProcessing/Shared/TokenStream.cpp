#include <Core/DataProcessing/Shared/TokenStream.hpp>

namespace Hyperion::DataProcessing {

TokenStream::TokenStream(const TokenStreamInfo& info)
    : m_tokens(),
      m_position(0),
      m_info(info)
{
}

} // namespace Hyperion::DataProcessing
