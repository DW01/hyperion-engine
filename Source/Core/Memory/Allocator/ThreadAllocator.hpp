/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Memory/Allocator/Allocator.hpp>

namespace Hyperion {

namespace threading {

class ThreadBase;
class ThreadLocalStorage;

CORE_API extern ThreadBase* CurrentThreadObject();

} // namespace threading

namespace memory {

class Pool;

CORE_API extern void** CurrentThreadAllocatorRaw();

template <class InnerAllocator, void (*InitInnerAllocatorFunction)(void*)>
struct TThreadAllocator : Allocator<TThreadAllocator<InnerAllocator, InitInnerAllocatorFunction>>
{
    static constexpr uint32 maxAlign = InnerAllocator::maxAlign;

    template <class T>
    struct Allocation : DynamicAllocationBase<T>
    {
    };

    HYP_FORCE_INLINE static InnerAllocator* GetThisThreadAllocator()
    {
        void** ppCurrentThreadAllocator = CurrentThreadAllocatorRaw();

        if (HYP_UNLIKELY(*ppCurrentThreadAllocator == nullptr))
        {
            *ppCurrentThreadAllocator = ThreadLocalAlloc<threading::ThreadBase, InnerAllocator>([]()
                {
                    void** ppCurrentThreadAllocator = CurrentThreadAllocatorRaw();

                    if (*ppCurrentThreadAllocator != nullptr)
                    {
                        static_cast<InnerAllocator*>(*ppCurrentThreadAllocator)->~InnerAllocator();

                        Memory::FreeAligned(*ppCurrentThreadAllocator);

                        *ppCurrentThreadAllocator = nullptr;
                    }
                });

            HYP_CORE_ASSERT(*ppCurrentThreadAllocator != nullptr);

            InitInnerAllocatorFunction(*ppCurrentThreadAllocator);
        }

        return static_cast<InnerAllocator*>(*ppCurrentThreadAllocator);
    }

    HYP_FORCE_INLINE void* Allocate(size_t size, size_t alignment)
    {
        return GetThisThreadAllocator()->Allocate(size, alignment);
    }

    HYP_FORCE_INLINE void Free(void* ptr)
    {
        GetThisThreadAllocator()->Free(ptr);
    }

private:
    template <class TThreadBase, class T>
    static T* ThreadLocalAlloc(void (*freeFunction)(void))
    {
        TThreadBase* currentThread = reinterpret_cast<TThreadBase*>(threading::CurrentThreadObject());
        HYP_CORE_ASSERT(currentThread != nullptr);

        T* ptr = static_cast<T*>(Memory::AllocateAligned(sizeof(T), alignof(T)));

        if (ptr)
        {
            if (freeFunction)
            {
                currentThread->AddOnExitCallback(freeFunction);
            }

            return ptr;
        }

        return nullptr;
    }
};

CORE_API extern void InitThreadAllocatorPool(void*);
using ThreadAllocator = TThreadAllocator<memory::Pool, &InitThreadAllocatorPool>;

} // namespace memory

using memory::ThreadAllocator;
using memory::TThreadAllocator;

} // namespace Hyperion
