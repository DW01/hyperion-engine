/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <engine/CVarManager.hpp>

#include <Core/config/Config.hpp>

#include <Core/containers/Array.hpp>

#include <Core/utilities/StringUtil.hpp>

#include <Core/threading/AtomicVar.hpp>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Engine);

static AtomicVar<int> s_nextCVarId{0};
static CVarManager* s_pInstance = nullptr;

#pragma region CVar

template <>
void CVar<int>::SetFromConfig(const ConfigValue& cfgValue)
{
    m_value = cfgValue.ToInt32();
}

template <>
void CVar<float>::SetFromConfig(const ConfigValue& cfgValue)
{
    m_value = cfgValue.ToFloat();
}

template <>
void CVar<bool>::SetFromConfig(const ConfigValue& cfgValue)
{
    m_value = cfgValue.ToBool();
}

template <>
void CVar<String>::SetFromConfig(const ConfigValue& cfgValue)
{
    m_value = cfgValue.ToString();
}

template <typename T>
static const T& ReadCVarValue(const CVar<T>& cvar)
{
    if (!s_pInstance || cvar.id < 0)
    {
        return cvar.m_value;
    }

    const CVarSnapshot& snapshot = s_pInstance->GetCurrentSnapshot();

    if (cvar.id >= snapshot.numVars)
    {
        return cvar.m_value;
    }

    return snapshot.values[cvar.id].Get<T>();
}

template <> const int8& CVar<int8>::Get() const { return ReadCVarValue(*this); }
template <> const int16& CVar<int16>::Get() const { return ReadCVarValue(*this); }
template <> const int32& CVar<int32>::Get() const { return ReadCVarValue(*this); }
template <> const int64& CVar<int64>::Get() const { return ReadCVarValue(*this); }

template <> const uint8& CVar<uint8>::Get() const { return ReadCVarValue(*this); }
template <> const uint16& CVar<uint16>::Get() const { return ReadCVarValue(*this); }
template <> const uint32& CVar<uint32>::Get() const { return ReadCVarValue(*this); }
template <> const uint64& CVar<uint64>::Get() const { return ReadCVarValue(*this); }

template <> const float& CVar<float>::Get() const { return ReadCVarValue(*this); }
template <> const double& CVar<double>::Get() const { return ReadCVarValue(*this); }

template <> const bool& CVar<bool>::Get() const { return ReadCVarValue(*this); }

template <> const String& CVar<String>::Get() const { return ReadCVarValue(*this); }

#pragma endregion CVar

#pragma region CVarBase

struct DeferredInitCVar
{
    CVarBase* cvar;
    String path;
};

static Array<DeferredInitCVar>& GetDeferredInitCVars()
{
    static Array<DeferredInitCVar> s_deferredInitCVars;
    return s_deferredInitCVars;
}

static void InitCVar(CVarManager* manager, CVarBase* cvar, UTF8StringView path)
{
    AssertDebug(cvar != nullptr);

    if (!manager)
    {
        DeferredInitCVar& deferredCVar = GetDeferredInitCVars().EmplaceBack();
        deferredCVar.cvar = cvar;
        deferredCVar.path = path;

        return;
    }

    cvar->name = CreateNameFromDynamicString(path);
    cvar->id = s_nextCVarId.Increment(1, MemoryOrder::RELAXED);

    manager->vars[cvar->id] = cvar;
}

CVarBase::CVarBase(UTF8StringView path)
    : id(-1),
      isHeapAllocated(false)
{
    InitCVar(s_pInstance, this, path);
}

#pragma endregion CVarBase

#pragma region CVarManager

CVarManager& CVarManager::GetInstance()
{
    static CVarManager s_instance;
    return s_instance;
}

CVarManager::CVarManager()
    : vars {},
      m_snapshotIndex(0)
{
    s_pInstance = this;

    Array<DeferredInitCVar>& deferredInitCVars = GetDeferredInitCVars();

    if (deferredInitCVars.Any())
    {
        for (DeferredInitCVar& deferredCVar : deferredInitCVars)
        {
            InitCVar(this, deferredCVar.cvar, UTF8StringView(deferredCVar.path));
        }

        deferredInitCVars.Clear();
    }

    m_snapshots[0].version = 1;
}

CVarManager::~CVarManager()
{
    for (CVarBase* var : vars)
    {
        if (var && var->isHeapAllocated)
        {
            delete var;
        }
    }

    s_pInstance = nullptr;
}

void CVarManager::InitFromConfig(const ConfigBase& config)
{
    const int numVars = s_nextCVarId.Get(MemoryOrder::RELAXED);

    for (int i = 0; i < numVars; i++)
    {
        CVarBase* cvar = vars[i];

        if (!cvar)
        {
            continue;
        }

        const char* path = cvar->name.LookupString();

        if (!path || path[0] == '\0')
        {
            // invalid name, skip
            continue;
        }

        const ConfigValue& value = config.Get(path);

        if (value.IsNullOrUndefined())
        {
            continue;
        }

        cvar->SetFromConfig(value);
    }
}

CVarBase *CVarManager::FindVar(Name name) const
{
    for (uint32 i = 0; i < MaxCVars; i++)
    {
        if (vars[i] && vars[i]->name == name)
        {
            return vars[i];
        }
    }

    return nullptr;
}

template <typename T>
void CVarManager::SetVar(Name name, T value)
{
    int idx = FindVarIndex(name);

    if (idx < 0)
    {
        return;
    }

    static_cast<CVar<T>*>(vars[idx])->Set(value);
}

template <typename T>
T CVarManager::GetVar(StringHash nameHash) const
{
    int idx = FindVarIndex(nameHash);

    if (idx < 0)
    {
        return T {};
    }

    const uint32 snapshotIndex = m_snapshotIndex.Get(MemoryOrder::ACQUIRE);
    const CVarSnapshot& snapshot = m_snapshots[snapshotIndex];

    if (idx >= snapshot.numVars)
    {
        return T {};
    }

    return snapshot.values[idx].Get<T>();
}

void CVarManager::Advance()
{
    Mutex::Guard lock(m_mutex);

    const uint32 currentIdx = m_snapshotIndex.Get(MemoryOrder::ACQUIRE);
    const uint32 nextIdx = (currentIdx + 1) % RingBufferDepth;

    CVarSnapshot& next = m_snapshots[nextIdx];

    const int numVars = s_nextCVarId.Get(MemoryOrder::RELAXED);

    for (int i = 0; i < numVars; i++)
    {
        if (vars[i])
        {
            vars[i]->WriteToSnapshot(next.values[i]);
        }
    }

    next.numVars = numVars;
    next.version = m_snapshots[currentIdx].version + 1;

    m_snapshotIndex.Set(nextIdx, MemoryOrder::RELEASE);
}

uint32 CVarManager::GetVersion() const
{
    return m_snapshots[m_snapshotIndex.Get(MemoryOrder::ACQUIRE)].version;
}

const CVarSnapshot& CVarManager::GetCurrentSnapshot() const
{
    return m_snapshots[m_snapshotIndex.Get(MemoryOrder::ACQUIRE)];
}

int CVarManager::FindVarIndex(StringHash nameHash) const
{
    for (uint32 i = 0; i < MaxCVars; i++)
    {
        if (vars[i] && vars[i]->name == nameHash)
        {
            return int(i);
        }
    }

    return -1;
}

#pragma endregion CVarManager

#pragma region Explicit template instantiations

template void CVarManager::SetVar<int8>(Name, int8);
template void CVarManager::SetVar<int16>(Name, int16);
template void CVarManager::SetVar<int32>(Name, int32);
template void CVarManager::SetVar<int64>(Name, int64);

template void CVarManager::SetVar<uint8>(Name, uint8);
template void CVarManager::SetVar<uint16>(Name, uint16);
template void CVarManager::SetVar<uint32>(Name, uint32);
template void CVarManager::SetVar<uint64>(Name, uint64);

template void CVarManager::SetVar<float>(Name, float);
template void CVarManager::SetVar<double>(Name, double);

template void CVarManager::SetVar<bool>(Name, bool);

template void CVarManager::SetVar<String>(Name, String);

template int8 CVarManager::GetVar<int8>(StringHash) const;
template int16 CVarManager::GetVar<int16>(StringHash) const;
template int32 CVarManager::GetVar<int32>(StringHash) const;
template int64 CVarManager::GetVar<int64>(StringHash) const;

template uint8 CVarManager::GetVar<uint8>(StringHash) const;
template uint16 CVarManager::GetVar<uint16>(StringHash) const;
template uint32 CVarManager::GetVar<uint32>(StringHash) const;
template uint64 CVarManager::GetVar<uint64>(StringHash) const;

template float CVarManager::GetVar<float>(StringHash) const;
template double CVarManager::GetVar<double>(StringHash) const;

template bool CVarManager::GetVar<bool>(StringHash) const;
template String CVarManager::GetVar<String>(StringHash) const;

#pragma endregion Explicit template instantiations

} // namespace Hyperion
