/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/Memory/Allocator/ThreadAllocator.hpp>

#include <Core/Memory/Pool/Pool.hpp>

#include <Core/Threading/Util/ThreadId.hpp>

namespace Hyperion {
namespace memory {

static constexpr size_t ThreadAllocatorPoolSize = 1024 * 1024 * 4; // 4 MB per thread for thread allocator pool

static thread_local void* s_currentThreadAllocatorRaw;

CORE_API void** CurrentThreadAllocatorRaw()
{
    return &s_currentThreadAllocatorRaw;
}

void InitThreadAllocatorPool(void* p)
{
    new (p) Pool(ThreadAllocatorPoolSize, PF_NONE, ThreadId::Current());
}

} // namespace memory
} // namespace Hyperion
