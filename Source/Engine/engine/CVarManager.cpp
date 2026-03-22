/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <engine/CVarManager.hpp>

#include <Core/containers/Array.hpp>

#include <Core/utilities/StringUtil.hpp>

#include <Core/threading/AtomicVar.hpp>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Engine);

static AtomicVar<int> s_nextCVarId{0};
static CVarManager* s_instance = nullptr;

#pragma region CVar

template <>
void CVar<int>::SetFromString(const String &value)
{
    m_value = StringUtil::Parse<int>(value);
}

template <>
String CVar<int>::ToString() const
{
    return String::ToString(m_value);
}

template <>
void CVar<float>::SetFromString(const String &value)
{
    m_value = StringUtil::Parse<float>(value);
}

template <>
String CVar<float>::ToString() const
{
    return HYP_FORMAT("{}", m_value);
}

template <>
void CVar<bool>::SetFromString(const String &value)
{
    m_value = (value == "true" || value == "1");
}

template <>
String CVar<bool>::ToString() const
{
    return m_value ? "true" : "false";
}

template <>
void CVar<String>::SetFromString(const String &value)
{
    m_value = value;
}

template <>
String CVar<String>::ToString() const
{
    return m_value;
}

#pragma endregion CVar

#pragma region CVarBase

struct DeferredInitCVar
{
    CVarBase* cvar;
    String path;
};

static Array<DeferredInitCVar> &GetDeferredInitCVars()
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

CVarBase::CVarBase(Type type, UTF8StringView path)
    : id(-1),
      type(type),
      isHeapAllocated(false)
{
    InitCVar(s_instance, this, path);
}

#pragma endregion CVarBase

#pragma region CVarManager

CVarManager &CVarManager::GetInstance()
{
    static CVarManager instance;
    return instance;
}

CVarManager *CVarManager::GetInstancePtr()
{
    return s_instance;
}

CVarManager::CVarManager()
    : vars {},
      m_snapshotIndex(0)
{
    s_instance = this;

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
    for (CVarBase *var : vars)
    {
        if (var && var->isHeapAllocated)
        {
            delete var;
        }
    }

    s_instance = nullptr;
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
T CVarManager::GetVar(Name name) const
{
    const uint32 snapshotIndex = m_snapshotIndex.Get(MemoryOrder::ACQUIRE);
    const CVarSnapshot &snapshot = m_snapshots[snapshotIndex];

    for (int i = 0; i < snapshot.numVars; i++)
    {
        if (snapshot.vars[i] && snapshot.vars[i]->name == name)
        {
            return static_cast<CVar<T>*>(snapshot.vars[i])->Get();
        }
    }

    return T {};
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
        next.vars[i] = vars[i];
    }

    next.numVars = numVars;
    next.version = m_snapshots[currentIdx].version + 1;

    m_snapshotIndex.Set(nextIdx, MemoryOrder::RELEASE);
}

uint32 CVarManager::GetVersion() const
{
    return m_snapshots[m_snapshotIndex.Get(MemoryOrder::ACQUIRE)].version;
}

int CVarManager::FindVarIndex(Name name) const
{
    for (uint32 i = 0; i < MaxCVars; i++)
    {
        if (vars[i] && vars[i]->name == name)
        {
            return int(i);
        }
    }

    return -1;
}

#pragma endregion CVarManager

#pragma region Explicit template instantiations

template void CVarManager::SetVar<int>(Name, int);
template void CVarManager::SetVar<float>(Name, float);
template void CVarManager::SetVar<bool>(Name, bool);
template void CVarManager::SetVar<String>(Name, String);

template int CVarManager::GetVar<int>(Name) const;
template float CVarManager::GetVar<float>(Name) const;
template bool CVarManager::GetVar<bool>(Name) const;
template String CVarManager::GetVar<String>(Name) const;

#pragma endregion Explicit template instantiations

} // namespace Hyperion
