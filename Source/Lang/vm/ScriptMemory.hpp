#pragma once

#include <Core/memory/pool/Pool.hpp>

namespace Hyperion {

extern Pool* g_scriptPool;
using ScriptAllocator = AllocatorInstance<Pool, &g_scriptPool>;

static inline void* ScriptAlloc(size_t size, size_t alignment = 1)
{
    return g_scriptPool->Allocate(size, alignment);
}

static inline void ScriptFree(void* ptr)
{
    g_scriptPool->Free(ptr);
}

} // namespace Hyperion
