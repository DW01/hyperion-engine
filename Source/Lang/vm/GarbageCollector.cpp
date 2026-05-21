#include <Lang/vm/GarbageCollector.hpp>

#include <Core/reflection/BoxedValue.hpp>
#include <Core/reflection/GenericArrayWrapper.hpp>

namespace Hyperion {

extern BoxedValue MakeValue(const ScriptObjectData& data);

static inline ScriptObjectData* GetVMData(BoxedValue& value)
{
    return reinterpret_cast<ScriptObjectData*>(value.TryGet<BoxedValue::InlineData>().TryGet());
}

GarbageCollector::GarbageCollector()
{
}

GarbageCollector::~GarbageCollector()
{
    for (auto it = m_trackedObjects.Begin(); it != m_trackedObjects.End(); ++it)
    {
        TrackedStorage& storage = *it;
        storage.Get().extData.gcIndex = INVALID_GC_INDEX;
        storage.Destruct();
    }

    m_trackedObjects.Clear();
    m_marks.Clear();
}

void GarbageCollector::MoveToTrackedMemory(BoxedValue& inOutRefValue)
{
    AssertDebug(inOutRefValue.extData.gcIndex == INVALID_GC_INDEX);
    AssertDebug(!IsRef(inOutRefValue));

    uint32 gcIndex = m_idGenerator.Next(); // starts at 1
    AssertDebug(gcIndex <= uint32(MAX_GC_INDEX), "Exceeded maximum number of tracked GC objects!");

    TrackedStorage& storage = m_trackedObjects[gcIndex];
    BoxedValue* ptr = storage.Construct(std::move(inOutRefValue));
    ptr->extData.gcIndex = GCIndex(gcIndex);

    // set `inOutRefValue` to be a reference to the tracked value
    ScriptObjectData newRefData {};
    newRefData.type = ScriptObjectData::Type::Reference;
    newRefData.valueRef = ptr;

    inOutRefValue = MakeValue(newRefData);
}

void GarbageCollector::ClearMarks()
{
    m_marks.Clear();
}

void GarbageCollector::MarkReachable(Span<BoxedValue> values)
{
    for (BoxedValue& value : values)
    {
        MarkReachable(value);
    }
}

void GarbageCollector::Collect()
{
    auto it = m_trackedObjects.Begin();
    while (it != m_trackedObjects.End())
    {
        const size_t gcIndex = m_trackedObjects.IndexOf(it);

        if (m_marks.Test(gcIndex))
        {
            m_marks.Set(gcIndex, false); // reset for next collection
            ++it;
        }
        else
        {
            TrackedStorage& storage = *it;
            storage.Get().extData.gcIndex = INVALID_GC_INDEX;
            storage.Destruct();

            it = m_trackedObjects.EraseAt(gcIndex);
            m_idGenerator.ReleaseId(uint32(gcIndex));
        }
    }
}

void GarbageCollector::Collect(Span<BoxedValue> roots)
{
    ClearMarks();
    MarkReachable(roots);
    Collect();
}

void GarbageCollector::MarkReachable(BoxedValue& value)
{
    BoxedValue* deref = Deref(value);
    if (deref == nullptr || deref->extData.gcIndex == INVALID_GC_INDEX)
    {
        return;
    }

    const uint32 gcIndex = uint32(deref->extData.gcIndex);

    if (m_marks.Test(gcIndex))
    {
        return; // already marked
    }

    m_marks.Set(gcIndex, true);

    TrackedStorage* storagePtr = m_trackedObjects.TryGet(gcIndex);
    if (storagePtr == nullptr)
    {
        return;
    }

    BoxedValue& tracked = storagePtr->Get();

    // Follow references from this tracked object
    if (ScriptObjectData* data = GetVMData(tracked))
    {
        if (data->type == ScriptObjectData::Type::Reference && data->valueRef != nullptr)
        {
            MarkReachable(*data->valueRef);
        }
    }

    // Mark array elements
    if (GenericArrayWrapper* array = tracked.TryGet<GenericArrayWrapper>().TryGet())
    {
        if (array->CanGetElementByIndex())
        {
            for (size_t j = 0; j < array->Size(); j++)
            {
                AnyRef element = array->GetElementAt(j);
                if (element.HasValue())
                {
                    if (element.GetTypeId() == TypeId::ForType<BoxedValue>())
                    {
                        MarkReachable(element.Get<BoxedValue>());
                    }
                }
            }
        }
    }
}

} // namespace Hyperion
