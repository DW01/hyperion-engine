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

enum class SparseArrayPolicy : uint8
{
    LinearPages = 0,      //!< Default, pages are linear, but pointers become invalidated when reszing pages.
    KeepPointersValid = 1 //!< Uses non-linear allocation (linked list) for pages, but keeps pointers to elements valid. More time spent looping to get a page (and worse cache performance) as a tradeoff.
};

template <class T, size_t NumElementsPerPage, class HasNextPointer>
struct SparseArrayPage;

template <class T, size_t NumElementsPerPage>
struct SparseArrayPage<T, NumElementsPerPage, std::true_type>
{
    ValueStorage<T> data[NumElementsPerPage];
    BitField<NumElementsPerPage> states;

    SparseArrayPage* next; // only used with KeepPointersValid
    bool ownsAllocation;   // true if this page owns the allocation (first in a contiguous block)
};

template <class T, size_t NumElementsPerPage>
struct SparseArrayPage<T, NumElementsPerPage, std::false_type>
{
    ValueStorage<T> data[NumElementsPerPage];
    BitField<NumElementsPerPage> states;
};

template <class T, class AllocatorType = DynamicAllocator, size_t NumElementsPerPage = 256, SparseArrayPolicy Policy = SparseArrayPolicy::LinearPages>
class SparseArray : public ContainerBase<SparseArray<T, AllocatorType, NumElementsPerPage, Policy>, size_t>
{
    using Page = SparseArrayPage<T, NumElementsPerPage, std::bool_constant<(Policy != SparseArrayPolicy::LinearPages)>>;

public:
    static constexpr bool isContiguous = false;

    using Base = ContainerBase<SparseArray<T, AllocatorType, NumElementsPerPage, Policy>, size_t>;
    using ValueType = T;

    // number of bits to shift to use when calculating absolute index for a given page index.
    // amounts to the same as (page * NumElementsPerPage)
    static constexpr size_t PageBitShiftCount = MathUtil::FastLog2_Pow2(NumElementsPerPage);

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
              elemIndex(NumElementsPerPage)
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
              elemIndex(NumElementsPerPage)
        {
            if (target != nullptr)
            {
                Advance(0);
            }
        }

        HYP_FORCE_INLINE ReferenceType operator*() const
        {
            HYP_CORE_ASSERT(target != nullptr);

            // auto is used because target may or may not be const
            auto* page = target->TryGetPage(pageIndex);

            HYP_CORE_ASSERT(page != nullptr);
            HYP_CORE_ASSERT(page->states.Test(elemIndex));

            return reinterpret_cast<ReferenceType>(page->data[elemIndex]);
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

            Page* currPage;

            if constexpr (Policy != SparseArrayPolicy::LinearPages)
            {
                currPage = const_cast<Page*>(target->TryGetPage(pageIndex));
            }

            for (size_t currPageIndex = pageIndex, currElemIndex = startElem; currPageIndex < target->m_numPages; currPageIndex++)
            {
                if (currElemIndex < NumElementsPerPage)
                {
                    size_t next;

                    if constexpr (Policy == SparseArrayPolicy::LinearPages)
                    {
                        currPage = const_cast<Page*>(&target->m_pages[currPageIndex]);
                    }

                    next = currPage->states.NextOneBit(currElemIndex);

                    if (next != SIZE_MAX)
                    {
                        pageIndex = currPageIndex;
                        elemIndex = next;

                        return;
                    }
                }

                if constexpr (Policy != SparseArrayPolicy::LinearPages)
                {
                    currPage = currPage->next;
                }

                currElemIndex = 0;
            }

            pageIndex = SIZE_MAX;
            elemIndex = NumElementsPerPage;
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
        [[maybe_unused]] Page* currPage = m_pages;

        if constexpr (!std::is_trivially_destructible_v<T> || Policy != SparseArrayPolicy::LinearPages)
        {
            for (size_t pageIndex = 0; pageIndex < m_numPages; pageIndex++)
            {
                if constexpr (Policy == SparseArrayPolicy::LinearPages)
                {
                    Page& page = m_pages[pageIndex];

                    for (size_t bitIndex : page.states)
                    {
                        reinterpret_cast<T&>(page.data[bitIndex]).~T();
                    }
                }
                else
                {
                    Page& page = *currPage;

                    if constexpr (!std::is_trivially_destructible_v<T>)
                    {
                        for (size_t bitIndex : page.states)
                        {
                            reinterpret_cast<T&>(page.data[bitIndex]).~T();
                        }
                    }

                    currPage = page.next;

                    if (page.ownsAllocation)
                    {
                        GetAllocator().Free(&page);
                    }
                }
            }
        }

        if constexpr (Policy == SparseArrayPolicy::LinearPages)
        {
            if (m_pages != nullptr)
            {
                GetAllocator().Free(m_pages);
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

        if constexpr (!std::is_trivially_destructible_v<T> || Policy != SparseArrayPolicy::LinearPages)
        {
            [[maybe_unused]] Page* currPage = m_pages;

            for (size_t pageIndex = 0; pageIndex < m_numPages; pageIndex++)
            {
                if constexpr (Policy == SparseArrayPolicy::LinearPages)
                {
                    Page& page = m_pages[pageIndex];

                    for (size_t bitIndex : page.states)
                    {
                        reinterpret_cast<T&>(page.data[bitIndex]).~T();
                    }
                }
                else
                {
                    Page& page = *currPage;

                    if constexpr (!std::is_trivially_destructible_v<T>)
                    {
                        for (size_t bitIndex : page.states)
                        {
                            reinterpret_cast<T&>(page.data[bitIndex]).~T();
                        }
                    }

                    currPage = page.next;

                    if (page.ownsAllocation)
                    {
                        GetAllocator().Free(&page);
                    }
                }
            }
        }

        if constexpr (Policy == SparseArrayPolicy::LinearPages)
        {
            if (m_pages != nullptr)
            {
                GetAllocator().Free(m_pages);
            }
        }
    }

    HYP_FORCE_INLINE T& Get(size_t index)
    {
        HYP_CORE_ASSERT(HasIndex(index), "Index {} is not initialized in SparseArray!", index);

        size_t pageIndex = (index / NumElementsPerPage);
        size_t elemIndex = (index % NumElementsPerPage);

        Page& page = *TryGetPage(pageIndex);

        return reinterpret_cast<T&>(page.data[elemIndex]);
    }

    HYP_FORCE_INLINE const T& Get(size_t index) const
    {
        return const_cast<SparseArray*>(this)->Get(index);
    }

    HYP_FORCE_INLINE T& GetOrCreate(size_t index)
    {
        size_t pageIndex = (index / NumElementsPerPage);
        size_t elemIndex = (index % NumElementsPerPage);

        Page& page = *GetOrCreatePage(pageIndex);

        if (!page.states.Test(elemIndex))
        {
            new (&page.data[elemIndex]) T;
            page.states.Set(elemIndex, true);
        }

        return reinterpret_cast<T&>(page.data[elemIndex]);
    }

    HYP_FORCE_INLINE T& GetUnchecked(size_t index)
    {
        size_t pageIndex = (index / NumElementsPerPage);
        size_t elemIndex = (index % NumElementsPerPage);

        Page& page = *TryGetPage(pageIndex);
        return reinterpret_cast<T&>(page.data[elemIndex]);
    }

    HYP_FORCE_INLINE const T& GetUnchecked(size_t index) const
    {
        return const_cast<SparseArray*>(this)->GetUnchecked(index);
    }

    HYP_FORCE_INLINE T* TryGet(size_t index)
    {
        size_t pageIndex = (index / NumElementsPerPage);
        size_t elemIndex = (index % NumElementsPerPage);

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
    T& Emplace(size_t index, Args&&... args)
    {
        size_t pageIndex = (index / NumElementsPerPage);
        size_t elemIndex = (index % NumElementsPerPage);

        Page& page = *GetOrCreatePage(pageIndex);

        if (page.states.Test(elemIndex))
        {
            if constexpr (std::is_move_assignable_v<T>)
            {
                // Follow std::vector's logic. construct at a different place then move into the location
                reinterpret_cast<T&>(page.data[elemIndex]) = T(std::forward<Args>(args)...);
            }
            else
            {
                reinterpret_cast<T&>(page.data[elemIndex]).~T();
                new (&page.data[elemIndex]) T(std::forward<Args>(args)...);
            }
        }
        else
        {
            new (&page.data[elemIndex]) T(std::forward<Args>(args)...);
            page.states.Set(elemIndex, true);
        }

        return reinterpret_cast<T&>(page.data[elemIndex]);
    }

    void Set(size_t index, const T& value)
    {
        size_t pageIndex = (index / NumElementsPerPage);
        size_t elemIndex = (index % NumElementsPerPage);

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
        size_t pageIndex = (index / NumElementsPerPage);
        size_t elemIndex = (index % NumElementsPerPage);

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

    /// Remove element at index \p{index} from the container.
    void Delete(size_t index)
    {
        size_t pageIndex = (index / NumElementsPerPage);
        size_t elemIndex = (index % NumElementsPerPage);

        Page* page = TryGetPage(pageIndex);

        if (page != nullptr && page->states.Test(elemIndex))
        {
            reinterpret_cast<T&>(page->data[elemIndex]).~T();
            page->states.Set(elemIndex, false);
        }
    }

    /// Finds the absolute index of the iterator, SIZE_MAX on failure.
    HYP_FORCE_INLINE size_t IndexOf(const ConstIterator& iter) const
    {
        if (iter.target != this || iter.elemIndex >= NumElementsPerPage || iter.pageIndex >= m_numPages)
        {
            return SIZE_MAX;
        }

        return (iter.pageIndex << PageBitShiftCount) + iter.elemIndex;
    }

    /// Finds the absolute index of element \p{value}. Returns SIZE_MAX on failure.
    /// More hefty than IndexOf() with an iterator param. Searches through pages to find a page with matching base
    /// address and calculates it from that.
    size_t IndexOf(const T& value) const
    {
        const uintptr_t elemAddress = reinterpret_cast<uintptr_t>(std::addressof(value));

        constexpr size_t pageStorageSizeBytes = (1ull << PageBitShiftCount) * sizeof(T);

        [[maybe_unused]] Page* currPage = m_pages;

        for (size_t currPageIndex = 0; currPageIndex < m_numPages; currPageIndex++)
        {
            if constexpr (Policy == SparseArrayPolicy::LinearPages)
            {
                currPage = &m_pages[currPageIndex];
            }

            const uintptr_t baseAddress = reinterpret_cast<uintptr_t>(&currPage->data[0]);

            if (elemAddress < baseAddress || elemAddress >= baseAddress + pageStorageSizeBytes)
            {
                if constexpr (Policy != SparseArrayPolicy::LinearPages)
                {
                    currPage = currPage->next;
                }

                continue; // pointer not in this page
            }

            return (currPageIndex << PageBitShiftCount) + ((elemAddress - baseAddress) / sizeof(T));
        }

        return SIZE_MAX;
    }

    bool Any() const
    {
        if (m_pages == 0)
        {
            return false;
        }

        [[maybe_unused]] Page* currPage = m_pages;

        for (size_t pageIndex = 0; pageIndex < m_numPages; pageIndex++)
        {
            if constexpr (Policy == SparseArrayPolicy::LinearPages)
            {
                Page& page = m_pages[pageIndex];

                if (page.states.CountOnes() != 0)
                {
                    return true;
                }
            }
            else
            {
                Page& page = *currPage;

                if (page.states.CountOnes() != 0)
                {
                    return true;
                }

                currPage = page.next;
            }
        }

        return false;
    }

    HYP_FORCE_INLINE bool Empty() const
    {
        return !Any();
    }

    /// Count valid elements
    size_t Count() const
    {
        size_t count = 0;

        Page* currPage = m_pages;

        for (size_t currPageIndex = 0; currPageIndex < m_numPages; currPageIndex++)
        {
            if constexpr (Policy == SparseArrayPolicy::LinearPages)
            {
                currPage = &m_pages[currPageIndex];
            }

            count += currPage->states.CountOnes();

            if constexpr (Policy != SparseArrayPolicy::LinearPages)
            {
                currPage = currPage->next;
            }
        }

        return count;
    }

    bool HasIndex(size_t index) const
    {
        size_t pageIndex = (index / NumElementsPerPage);
        size_t elemIndex = (index % NumElementsPerPage);

        const Page* page = TryGetPage(pageIndex);

        if (!page)
        {
            return false;
        }

        return page->states.Test(elemIndex);
    }

    Iterator Erase(const ConstIterator& iter)
    {
        if (HYP_UNLIKELY(iter == End()))
        {
            return End();
        }

        Iterator iter2 = reinterpret_cast<const Iterator&>(iter);
        ++iter2;

        const size_t absoluteIndex = (iter.pageIndex << PageBitShiftCount) + iter.elemIndex;

        Delete(absoluteIndex);

        return iter2;
    }

    void Clear(bool freeMemory = true)
    {
        if (m_numPages == 0)
        {
            return;
        }

        [[maybe_unused]] Page* currPage = m_pages;

        for (size_t pageIndex = 0; pageIndex < m_numPages; pageIndex++)
        {
            if constexpr (Policy == SparseArrayPolicy::LinearPages)
            {
                Page& page = m_pages[pageIndex];

                for (size_t bitIndex : page.states)
                {
                    reinterpret_cast<T&>(page.data[bitIndex]).~T();
                }

                Memory::Zero(&page.states, sizeof(page.states));
            }
            else
            {
                Page& page = *currPage;

                for (size_t bitIndex : page.states)
                {
                    reinterpret_cast<T&>(page.data[bitIndex]).~T();
                }

                Memory::Zero(&page.states, sizeof(page.states));

                currPage = page.next;

                if (freeMemory && page.ownsAllocation)
                {
                    GetAllocator().Free(&page);
                }
            }
        }

        if (freeMemory)
        {
            if constexpr (Policy == SparseArrayPolicy::LinearPages)
            {
                if (m_pages != nullptr)
                {
                    GetAllocator().Free(m_pages);
                }
            }

            m_pages = nullptr;
            m_numPages = 0;
        }
    }

    HYP_DEF_STL_BEGIN_END(
        Iterator(const_cast<SparseArray*>(this), typename Iterator::BeginTag()),
        Iterator(const_cast<SparseArray*>(this), SIZE_MAX, NumElementsPerPage))

private:
    static HYP_FORCE_INLINE AllocatorType& GetAllocator()
    {
        return *GetDefaultAllocatorInstance<AllocatorType>();
    }

    HYP_NODISCARD Page* GetOrCreatePage(size_t pageIndex)
    {
        if (HYP_UNLIKELY(pageIndex >= m_numPages))
        {
            if constexpr (Policy == SparseArrayPolicy::LinearPages)
            {
                size_t newNumPages = MathUtil::NextPowerOf2(pageIndex + 1);

                Page* newPages = (Page*)GetAllocator().Allocate(sizeof(Page) * newNumPages, alignof(Page));
                HYP_CORE_ASSERT(newPages != nullptr);

                Memory::Zero(newPages, sizeof(Page) * newNumPages);

                if (m_pages != nullptr)
                {
                    if constexpr (std::is_trivial_v<T>)
                    {
                        Memory::Move(newPages, m_pages, sizeof(Page) * m_numPages);
                    }
                    else
                    {
                        for (size_t currPageIndex = 0; currPageIndex < m_numPages; currPageIndex++)
                        {
                            Page& currPage = m_pages[currPageIndex];
                            Page& newPage = newPages[currPageIndex];

                            if (currPage.states.CountOnes() != 0)
                            {
                                for (size_t bitIndex : currPage.states)
                                {
                                    new (&newPage.data[bitIndex]) T(std::move(reinterpret_cast<T&&>(currPage.data[bitIndex])));
                                    reinterpret_cast<T&>(currPage.data[bitIndex]).~T();
                                }

                                newPage.states = currPage.states;
                            }
                        }
                    }

                    GetAllocator().Free(m_pages);
                }

                m_pages = newPages;
                m_numPages = newNumPages;
            }
            else
            {
                const size_t numNewPages = pageIndex - m_numPages + 1;

                Page* newPages = (Page*)GetAllocator().Allocate(sizeof(Page) * numNewPages, alignof(Page));
                HYP_CORE_ASSERT(newPages != nullptr);

                Memory::Zero(newPages, sizeof(Page) * numNewPages);

                newPages[0].ownsAllocation = true;

                for (size_t i = 0; i + 1 < numNewPages; i++)
                {
                    newPages[i].next = &newPages[i + 1];
                }

                if (m_pages == nullptr)
                {
                    m_pages = &newPages[0];
                }
                else
                {
                    Page* lastPage = m_pages;
                    while (lastPage->next != nullptr)
                    {
                        lastPage = lastPage->next;
                    }

                    lastPage->next = &newPages[0];
                }

                m_numPages += numNewPages;
            }
        }

        if constexpr (Policy == SparseArrayPolicy::LinearPages)
        {
            return m_pages + pageIndex;
        }
        else
        {
            Page* currPage = m_pages;
            for (size_t currPageIndex = 0; currPageIndex < pageIndex && currPage != nullptr; currPageIndex++)
            {
                currPage = currPage->next;
            }

            return currPage;
        }
    }

    Page* TryGetPage(size_t pageIndex)
    {
        if constexpr (Policy == SparseArrayPolicy::LinearPages)
        {
            return (pageIndex < m_numPages) ? m_pages + pageIndex : nullptr;
        }
        else
        {
            if (pageIndex < m_numPages)
            {
                Page* currPage = m_pages;
                for (size_t currPageIndex = 0; currPageIndex < pageIndex && currPage != nullptr; currPageIndex++)
                {
                    currPage = currPage->next;
                }

                return currPage;
            }

            return nullptr;
        }
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
using containers::SparseArrayPolicy;

} // namespace Hyperion
