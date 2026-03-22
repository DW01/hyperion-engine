/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <engine/EngineConfig.hpp>

#include <Core/utilities/StringUtil.hpp>

namespace Hyperion {

template <>
void CVar<int>::SetFromString(const String& value)
{
    m_value = StringUtil::Parse<int>(value);
}

template <>
String CVar<int>::ToString() const
{
    return String::ToString(m_value);
}

template <>
void CVar<float>::SetFromString(const String& value)
{
    m_value = StringUtil::Parse<float>(value);
}

template <>
String CVar<float>::ToString() const
{
    return HYP_FORMAT("{}", m_value);
}

template <>
void CVar<bool>::SetFromString(const String& value)
{
    m_value = (value == "true" || value == "1");
}

template <>
String CVar<bool>::ToString() const
{
    return m_value ? "true" : "false";
}

template <>
void CVar<String>::SetFromString(const String& value)
{
    m_value = value;
}

template <>
String CVar<String>::ToString() const
{
    return m_value;
}

#pragma region EngineConfig

EngineConfig& EngineConfig::GetInstance()
{
    static EngineConfig s_instance;
    return s_instance;
}

EngineConfig::EngineConfig()
    : m_snapshotIndex(0),
      m_nextVarId(0)
{
    m_snapshots[0].version = 1;
}

EngineConfig::~EngineConfig()
{
    for (auto& snapshot : m_snapshots)
    {
        for (int j = 0; j < snapshot.numVars; j++)
        {
            delete snapshot.vars[j];
        }
    }
}

template <typename T>
CVar<T> *EngineConfig::RegisterVar(const String& name, T defaultValue)
{
    Mutex::Guard lock(m_mutex);

    const uint32 snapshotIndex = m_snapshotIndex.Get(MemoryOrder::ACQUIRE);

    const StringHash nameHash = StringHash(name);

    int idx = FindVarIndex(nameHash);
    if (idx >= 0)
    {
        return static_cast<CVar<T> *>(m_snapshots[snapshotIndex].vars[idx]);
    }

    int id = m_nextVarId.Increment(1, MemoryOrder::ACQUIRE_RELEASE);
    EngineConfigVarType type = ECVT_INVALID;

    if constexpr (std::is_same_v<T, int>)
    {
        type = ECVT_INT;
    }
    else if constexpr (std::is_same_v<T, float>)
    {
        type = ECVT_FLOAT;
    }
    else if constexpr (std::is_same_v<T, bool>)
    {
        type = ECVT_BOOL;
    }
    else if constexpr (std::is_same_v<T, String>)
    {
        type = ECVT_STRING;
    }

    CVar<T>* var = new CVar<T>(CreateNameFromDynamicString(name), type, defaultValue);
    var->SetId(id);

    for (auto& snapshot : m_snapshots)
    {
        snapshot.vars[id] = var;
        snapshot.numVars = id + 1;
    }

    return var;
}

template <typename T>
void EngineConfig::SetVar(const String& name, T value)
{
    int idx = FindVarIndex(StringHash(name));
    if (idx < 0)
    {
        return;
    }

    const uint32 snapshotIndex = m_snapshotIndex.Get(MemoryOrder::ACQUIRE);

    CVar<T>* var = static_cast<CVar<T>*>(m_snapshots[snapshotIndex].vars[idx]);
    var->Set(value);
}

template <typename T>
T EngineConfig::GetVar(const String& name) const
{
    int idx = FindVarIndex(StringHash(name));
    if (idx < 0)
    {
        return T{};
    }

    const uint32 snapshotIndex = m_snapshotIndex.Get(MemoryOrder::ACQUIRE);

    CVar<T>* var = static_cast<CVar<T>*>(m_snapshots[snapshotIndex].vars[idx]);
    return var->Get();
}

void EngineConfig::Advance()
{
    const uint32 snapshotIndex = m_snapshotIndex.Get(MemoryOrder::ACQUIRE);

    Mutex::Guard lock(m_mutex);
    uint32 nextIdx = (snapshotIndex + 1) % RingBufferDepth;

    m_snapshots[nextIdx] = m_snapshots[snapshotIndex];
    m_snapshots[nextIdx].version++;
    m_snapshotIndex.Set(nextIdx, MemoryOrder::RELEASE);
}

uint32 EngineConfig::GetVersion() const
{
    return m_snapshots[m_snapshotIndex.Get(MemoryOrder::ACQUIRE)].version;
}

int EngineConfig::FindVarIndex(StringHash nameHash) const
{
    const auto& snapshot = m_snapshots[m_snapshotIndex.Get(MemoryOrder::ACQUIRE)];

    for (int i = 0; i < snapshot.numVars; i++)
    {
        if (snapshot.vars[i]->GetName() == nameHash)
        {
            return i;
        }
    }

    return -1;
}

#pragma endregion EngineConfig

#pragma region Explicit template instantiations

template CVar<int> *EngineConfig::RegisterVar<int>(const String&, int);
template CVar<float> *EngineConfig::RegisterVar<float>(const String&, float);
template CVar<bool> *EngineConfig::RegisterVar<bool>(const String&, bool);
template CVar<String> *EngineConfig::RegisterVar<String>(const String&, String);

template void EngineConfig::SetVar<int>(const String&, int);
template void EngineConfig::SetVar<float>(const String&, float);
template void EngineConfig::SetVar<bool>(const String&, bool);
template void EngineConfig::SetVar<String>(const String&, String);

template int EngineConfig::GetVar<int>(const String&) const;
template float EngineConfig::GetVar<float>(const String&) const;
template bool EngineConfig::GetVar<bool>(const String&) const;
template String EngineConfig::GetVar<String>(const String&) const;

#pragma endregion Explicit template instantiations

} // namespace Hyperion
