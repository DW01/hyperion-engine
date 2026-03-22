/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>
#include <Core/Constants.hpp>
#include <Core/Types.hpp>

#include <Core/memory/Pimpl.hpp>

#include <Core/containers/Array.hpp>
#include <Core/containers/FixedArray.hpp>
#include <Core/containers/String.hpp>

#include <Core/threading/AtomicVar.hpp>
#include <Core/threading/Mutex.hpp>

#include <Core/name/Name.hpp>

#include <cstdint>
#include <type_traits>

namespace Hyperion {

static constexpr uint32 EngineConfigMaxVars = 128;

enum EngineConfigVarType : int
{
    ECVT_INVALID = -1,
    ECVT_INT = 0,
    ECVT_FLOAT,
    ECVT_BOOL,
    ECVT_STRING,
    ECVT_MAX
};

struct CVarDesc
{
    int id;
    Name name;
    EngineConfigVarType type;
};

class CVarBase
{
public:
    CVarBase(Name name, EngineConfigVarType type)
        : m_name(name),
          m_type(type),
            m_id(-1)
    {
    }

    virtual ~CVarBase() = default;
    
    HYP_FORCE_INLINE int GetId() const
    {
        return m_id;
    }

    HYP_FORCE_INLINE Name GetName() const
    {
        return m_name;
    }

    HYP_FORCE_INLINE EngineConfigVarType GetType() const
    {
        return m_type;
    }

    void SetId(int id)
    {
        m_id = id;
    }

    virtual void SetFromString(const String& value) = 0;
    virtual String ToString() const = 0;

protected:
    Name m_name;
    EngineConfigVarType m_type;
    int m_id;
};

template <typename T>
class CVar : public CVarBase
{
public:
    CVar(Name name, EngineConfigVarType type, T defaultValue)
        : CVarBase(name, type),
          m_defaultValue(defaultValue)
    {
    }

    void Set(T value)
    {
        m_value = value;
    }

    T Get() const
    {
        return m_value; 
    }

    void SetFromString(const String& value) override;
    String ToString() const override;

private:
    T m_value;
    T m_defaultValue;
};

struct EngineConfigSnapshot
{
    CVarBase* vars[EngineConfigMaxVars];
    int numVars;
    uint32 version;

    EngineConfigSnapshot() :
        vars { },
        numVars(0),
        version(0)
    {
    }
};

class EngineConfig
{
public:
    static EngineConfig& GetInstance();

    template <typename T>
    CVar<T>* RegisterVar(const String& name, T defaultValue);

    template <typename T>
    void SetVar(const String& name, T value);

    template <typename T>
    T GetVar(const String& name) const;

    /// Publishes the cvar states so they're visible to other threads. Should be called once per frame.
    void Advance();

    uint32 GetVersion() const;

private:
    EngineConfig();
    ~EngineConfig();

    void PublishSnapshot();
    int FindVarIndex(StringHash nameHash) const;

    EngineConfigSnapshot m_snapshots[RingBufferDepth];
    AtomicVar<uint32> m_snapshotIndex;
    AtomicVar<int> m_nextVarId;
    Mutex m_mutex; // For registration/mutation only
};

} // namespace Hyperion
