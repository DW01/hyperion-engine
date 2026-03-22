/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>
#include <Core/Constants.hpp>
#include <Core/Types.hpp>

#include <Core/containers/FixedArray.hpp>
#include <Core/containers/String.hpp>

#include <Core/threading/AtomicVar.hpp>
#include <Core/threading/Mutex.hpp>

#include <Core/name/Name.hpp>

#include <type_traits>

namespace Hyperion {

static constexpr uint32 MaxCVars = 128;

class CVarManager;

class HYP_API CVarBase
{
public:
    enum Type : int
    {
        Type_Invalid = -1,
        Type_Int = 0,
        Type_Float,
        Type_Bool,
        Type_String,
        Type_Max
    };

protected:
    CVarBase(Type type, UTF8StringView path);

public:
    int id;
    Name name;
    Type type;
    bool isHeapAllocated;

    virtual ~CVarBase() = default;

    virtual void SetFromString(const String& value) = 0;
    virtual String ToString() const = 0;
};

template <typename T>
class CVar : public CVarBase
{
    static constexpr Type GetVarType()
    {
        if constexpr (std::is_same_v<T, int>) return Type_Int;
        else if constexpr (std::is_same_v<T, float>) return Type_Float;
        else if constexpr (std::is_same_v<T, bool>) return Type_Bool;
        else if constexpr (std::is_same_v<T, String>) return Type_String;
        else return Type_Invalid;
    }

public:
    explicit CVar(UTF8StringView path, T defaultValue = T {})
        : CVarBase(GetVarType(), path),
          m_value(defaultValue),
          m_defaultValue(defaultValue)
    {
    }

    HYP_FORCE_INLINE void Set(T value)
    {
        m_value = value;
    }

    HYP_FORCE_INLINE T Get() const
    {
        return m_value;
    }

    void SetFromString(const String& value) override;
    String ToString() const override;

private:
    T m_value;
    T m_defaultValue;
};

template <> HYP_API void CVar<int>::SetFromString(const String& value);
template <> HYP_API String CVar<int>::ToString() const;
template <> HYP_API void CVar<float>::SetFromString(const String& value);
template <> HYP_API String CVar<float>::ToString() const;
template <> HYP_API void CVar<bool>::SetFromString(const String& value);
template <> HYP_API String CVar<bool>::ToString() const;
template <> HYP_API void CVar<String>::SetFromString(const String& value);
template <> HYP_API String CVar<String>::ToString() const;

struct CVarSnapshot
{
    CVarBase *vars[MaxCVars];
    int numVars;
    uint32 version;

    CVarSnapshot()
        : vars {},
          numVars(0),
          version(0)
    {
    }
};

class CVarManager
{
public:
    static CVarManager &GetInstance();
    static CVarManager *GetInstancePtr();

    CVarManager();
    ~CVarManager();

    CVarBase *FindVar(Name name) const;

    template <typename T>
    void SetVar(Name name, T value);

    template <typename T>
    T GetVar(Name name) const;

    /*! \brief Publishes the cvar states so they're visible to other threads.
     *  Call once per frame at end of frame. */
    void Advance();

    uint32 GetVersion() const;

    FixedArray<CVarBase *, MaxCVars> vars;

private:
    int FindVarIndex(Name name) const;

    CVarSnapshot m_snapshots[RingBufferDepth];
    AtomicVar<uint32> m_snapshotIndex;
    Mutex m_mutex;
};

} // namespace Hyperion
