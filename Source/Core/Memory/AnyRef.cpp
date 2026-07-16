/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/Memory/AnyRef.hpp>

#include <Core/Reflection/BoxedValue.hpp>
#include <Core/Reflection/TypeInfo.hpp>

namespace Hyperion {

bool AnyRef_IsUnderlyingEnumType(const TypeInfo* typeInfo, TypeId requestedTypeId)
{
    if (!typeInfo || !typeInfo->IsEnum())
    {
        return false;
    }

    const TypeInfo* underlying = typeInfo->GetUnderlyingType();
    return underlying && underlying->id == requestedTypeId;
}

namespace memory {

TypeId AnyRefBase::GetTypeId() const
{
    return m_typeInfo ? m_typeInfo->id : TypeId::Void();
}

const Class* AnyRefBase::GetClass() const
{
    return m_typeInfo ? m_typeInfo->GetClass() : nullptr;
}

} // namespace memory
} // namespace Hyperion
