/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Containers/ContainerBase.hpp>

#include <Core/Utilities/BitField.hpp>
#include <Core/Utilities/Pair.hpp>

#include <Core/Memory/Allocator/Allocator.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Defines.hpp>

namespace Hyperion {
namespace containers {

template <class T, class AllocatorType = DynamicAllocator, size_t BlockSize = 256>
class SparseArray : public ContainerBase<SparseArray<T, AllocatorType, BlockSize>, size_t>
{
    struct Page
    {
        ValueStorage<T> data[BlockSize];
        BitField<BlockSize> states;
    };

public:
    static constexpr bool isContiguous = false;

    using Base = ContainerBase<SparseArray<T, AllocatorType, BlockSize>, size_t>;
    using ValueType = T;

    template <bool IsConst>
    struct IteratorBase
    {
        using ArrayType = std::conditional_t<IsConst, const SparseArray, SparseArray>;

        using PointerType = std::conditional_t<IsConst, const T*, T*>;
        using ReferenceType = std::conditional_t<IsConst, const T&, T&>;

        struct BeginTag
        {
        };

        ArrayType* target;
        size_t pageIndex;
        size_t elemIndex;

        HYP_FORCE_INLINE IteratorBase()
            : target(nullptr),
              pageIndex(0),
              elemIndex(BlockSize)
        {
        }

        HYP_FORCE_INLINE IteratorBase(ArrayType* target, size_t pageIndex, size_t elemIndex)
            : target(target),
              pageIndex(pageIndex),
              elemIndex(elemIndex)
        {
        }

        HYP_FORCE_INLINE IteratorBase(ArrayType* target, BeginTag)
            : target(target),
              pageIndex(0),
              elemIndex(BlockSize)
        {
            if (target != nullptr)
            {
                Advance(0);
            }
        }

        HYP_FORCE_INLINE ReferenceType operator*() const
        {
            HYP_CORE_ASSERT(target != nullptr);
            HYP_CORE_ASSERT(pageIndex < target->m_numPages);
            HYP_CORE_ASSERT(target->m_pages[pageIndex].states.Test(elemIndex));

            return reinterpret_cast<ReferenceType>(target->m_pages[pageIndex].data[elemIndex]);
        }

        HYP_FORCE_INLINE PointerType operator->() const
        {
            return &(**this);
        }

        HYP_FORCE_INLINE IteratorBase& operator++()
        {
            Advance(elemIndex + 1);

            return *this;
        }

        HYP_FORCE_INLINE IteratorBase operator++(int)
        {
            IteratorBase tmp = *this;
            ++(*this);

            return tmp;
        }

        HYP_FORCE_INLINE bool operator==(const IteratorBase& other) const
        {
            return target == other.target
                && pageIndex == other.pageIndex
                && elemIndex == other.elemIndex;
        }

        HYP_FORCE_INLINE bool operator!=(const IteratorBase& other) const
        {
            return !(*this == other);
        }

        void Advance(size_t startElem)
        {
            HYP_CORE_ASSERT(target != nullptr);

            size_t p = pageIndex;
            size_t e = startElem;

            while (p < target->m_numPages)
            {
                if (e < BlockSize)
                {
                    const size_t next = target->m_pages[p].states.NextOneBit(e);

                    if (next != SIZE_MAX)
                    {
                        pageIndex = p;
                        elemIndex = next;

                        return;
                    }
                }

                ++p;
                e = 0;
            }

            pageIndex = target->m_numPages;
            elemIndex = BlockSize;
        }
    };

    struct ConstIterator : IteratorBase<true>
    {
        using IteratorBase<true>::IteratorBase;
    };

    struct Iterator : IteratorBase<false>
    {
        using IteratorBase<false>::IteratorBase;

        HYP_FORCE_INLINE operator ConstIterator() const
        {
            return ConstIterator(this->target, this->pageIndex, this->elemIndex);
        }
    };

    SparseArray()
        : m_pages(nullptr),
          m_numPages(0)
    {
    }

    SparseArray(const SparseArray& other) = delete;
    SparseArray& operator=(const SparseArray& other) = delete;

    SparseArray(SparseArray&& other) noexcept
        : m_pages(other.m_pages),
          m_numPages(other.m_numPages)
    {
        other.m_pages = nullptr;
        other.m_numPages = 0;
    }

    SparseArray& operator=(SparseArray&& other) noexcept
    {
        for (size_t pageIndex = 0; pageIndex < m_numPages; pageIndex++)
        {
            Page& page = m_pages[pageIndex];

            for (size_t bitIndex : page.states)
            {
                reinterpret_cast<T&>(page.data[bitIndex]).~T();
            }
        }

        m_pages = other.m_pages;
        m_numPages = other.m_numPages;

        other.m_pages = nullptr;
        other.m_numPages = 0;

        return *this;
    }

    ~SparseArray()
    {
        if (m_numPages == 0)
        {
            return;
        }

        for (size_t pageIndex = 0; pageIndex < m_numPages; pageIndex++)
        {
            Page& page = m_pages[pageIndex];

            for (size_t bitIndex : page.states)
            {
                reinterpret_cast<T&>(page.data[bitIndex]).~T();
            }
        }

        GetAllocator().Free(m_pages);
    }

    HYP_FORCE_INLINE T& Get(size_t index)
    {
        size_t pageIndex = (index / BlockSize);
        size_t elemIndex = (index % BlockSize);

        Page& page = *GetOrCreatePage(pageIndex);

        if (!page.states.Test(elemIndex))
        {
            new (&page.data[elemIndex]) T;
            page.states.Set(elemIndex, true);
        }

        return reinterpret_cast<T&>(page.data[elemIndex]);
    }

    HYP_FORCE_INLINE T* TryGet(size_t index)
    {
        size_t pageIndex = (index / BlockSize);
        size_t elemIndex = (index % BlockSize);

        Page* page = TryGetPage(pageIndex);

        if (!page)
        {
            return nullptr;
        }

        return (page->states.Test(elemIndex) ? reinterpret_cast<T*>(&page->data[elemIndex]) : nullptr);
    }

    HYP_FORCE_INLINE const T* TryGet(size_t index) const
    {
        return const_cast<SparseArray*>(this)->TryGet(index);
    }

    template <class... Args>
    HYP_FORCE_INLINE T& Emplace(size_t index, Args&&... args)
    {
        size_t pageIndex = (index / BlockSize);
        size_t elemIndex = (index % BlockSize);

        Page& page = *GetOrCreatePage(pageIndex);

        if (!page.states.Test(elemIndex))
        {
            new (&page.data[elemIndex]) T(std::forward<Args>(args)...);
            page.states.Set(elemIndex, true);
        }
        else
        {
            ((void)args, ...);
        }

        return reinterpret_cast<T&>(page.data[elemIndex]);
    }

    void Set(size_t index, const T& value)
    {
        size_t pageIndex = (index / BlockSize);
        size_t elemIndex = (index % BlockSize);

        Page& page = *GetOrCreatePage(pageIndex);

        if (page.states.Test(elemIndex))
        {
            reinterpret_cast<T&>(page.data[elemIndex]) = value;
        }
        else
        {
            new (&page.data[elemIndex]) T(value);
            page.states.Set(elemIndex, true);
        }
    }

    void Set(size_t index, T&& value)
    {
        size_t pageIndex = (index / BlockSize);
        size_t elemIndex = (index % BlockSize);

        Page& page = *GetOrCreatePage(pageIndex);

        if (page.states.Test(elemIndex))
        {
            reinterpret_cast<T&>(page.data[elemIndex]) = std::move(value);
        }
        else
        {
            new (&page.data[elemIndex]) T(std::move(value));
            page.states.Set(elemIndex, true);
        }
    }

    void Delete(size_t index)
    {
        size_t pageIndex = (index / BlockSize);
        size_t elemIndex = (index % BlockSize);

        Page* page = TryGetPage(pageIndex);

        if (page != nullptr && page->states.Test(elemIndex))
        {
            reinterpret_cast<T&>(page->data[elemIndex]).~T();
            page->states.Set(elemIndex, false);
        }
    }

    HYP_FORCE_INLINE bool Any() const
    {
        return m_numPages != 0;
    }

    bool HasIndex(size_t index) const
    {
        size_t pageIndex = (index / BlockSize);
        size_t elemIndex = (index % BlockSize);

        const Page* page = TryGetPage(pageIndex);

        if (!page)
        {
            return false;
        }

        return page->states.Test(elemIndex);
    }

    void Clear(bool freeMemory = true)
    {
        if (m_numPages == 0)
        {
            return;
        }

        for (size_t pageIndex = 0; pageIndex < m_numPages; pageIndex++)
        {
            Page& page = m_pages[pageIndex];

            for (size_t bitIndex : page.states)
            {
                reinterpret_cast<T&>(page.data[bitIndex]).~T();
            }

            page.states = BitField<BlockSize>();
        }

        if (freeMemory)
        {
            GetAllocator().Free(m_pages);
            m_numPages = 0;
        }
    }

    HYP_DEF_STL_BEGIN_END(
        Iterator(const_cast<SparseArray*>(this), typename Iterator::BeginTag()),
        Iterator(const_cast<SparseArray*>(this), m_numPages, BlockSize))

private:
    static HYP_FORCE_INLINE AllocatorType& GetAllocator()
    {
        return *GetDefaultAllocatorInstance<AllocatorType>();
    }

    Page* GetOrCreatePage(size_t pageIndex)
    {
        if (HYP_UNLIKELY(pageIndex >= m_numPages))
        {
            size_t newNumPages = MathUtil::NextPowerOf2(pageIndex + 1);

            Page* newPages = (Page*)GetAllocator().Allocate(sizeof(Page) * newNumPages, alignof(Page));
            HYP_CORE_ASSERT(newPages != nullptr);

            if (m_pages != nullptr)
            {
                if constexpr (std::is_trivial_v<T>)
                {
                    Memory::Move(newPages, m_pages, sizeof(Page) * m_numPages);
                }
                else
                {
                    for (size_t pageIndex = 0; pageIndex < m_numPages; pageIndex++)
                    {
                        Page& currPage = m_pages[pageIndex];
                        Page& newPage = newPages[pageIndex];
                        for (size_t bitIndex : currPage.states)
                        {
                            Memory::Construct<T>(reinterpret_cast<T*>(&newPage.data[bitIndex]), std::move(reinterpret_cast<T&&>(currPage.data[bitIndex])));
                            reinterpret_cast<T&>(currPage.data[bitIndex]).~T();
                        }

                        newPage.states = currPage.states;
                    }
                }

                Memory::Zero(newPages + m_numPages, sizeof(Page) * (newNumPages - m_numPages));

                GetAllocator().Free(m_pages);
            }
            else
            {
                Memory::Zero(newPages, sizeof(Page) * newNumPages);
            }

            m_pages = newPages;
            m_numPages = newNumPages;
        }

        return m_pages + pageIndex;
    }

    HYP_FORCE_INLINE Page* TryGetPage(size_t pageIndex)
    {
        return (pageIndex < m_numPages) ? m_pages + pageIndex : nullptr;
    }

    HYP_FORCE_INLINE const Page* TryGetPage(size_t pageIndex) const
    {
        return const_cast<SparseArray*>(this)->TryGetPage(pageIndex);
    }

    Page* m_pages;
    size_t m_numPages;
};

} // namespace containers

using containers::SparseArray;

} // namespace Hyperion
