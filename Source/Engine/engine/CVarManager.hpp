/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>
#include <Core/Constants.hpp>
#include <Core/Types.hpp>
#include <Core/Util.hpp>

#include <Core/containers/FixedArray.hpp>
#include <Core/containers/String.hpp>

#include <Core/threading/AtomicVar.hpp>
#include <Core/threading/Mutex.hpp>

#include <Core/name/Name.hpp>

#include <type_traits>

namespace Hyperion {

static constexpr uint32 MaxCVars = 128;

class CVarManager;

using CVarSnapshotValue = Variant<int, float, bool, String>;

class HYP_API CVarBase
{
public:
    friend class CVarManager;

protected:
    explicit CVarBase(UTF8StringView path);

public:
    int id;
    Name name;
    bool isHeapAllocated;

    virtual ~CVarBase() = default;

    virtual void SetFromString(const String& value) = 0;
    virtual String ToString() const = 0;
    
protected:
    virtual void WriteToSnapshot(CVarSnapshotValue& snapshotValue) const = 0;
};

template <typename T>
class CVar final : public CVarBase
{
public:
    explicit CVar(UTF8StringView path, T defaultValue = T {})
        : CVarBase(path),
          m_value(defaultValue)
    {
    }

    HYP_FORCE_INLINE void Set(T value)
    {
        m_value = value;
    }

    T Get() const;

    void SetFromString(const String& value) override;
    String ToString() const override;

protected:
    void WriteToSnapshot(CVarSnapshotValue& snapshotValue) const override
    {
        snapshotValue.Set(m_value);
    }

private:
    template <typename U>
    friend const U& ReadCVarValue(const CVar<U>& cvar);

    T m_value;
};

template <> HYP_API void CVar<int>::SetFromString(const String& value);
template <> HYP_API String CVar<int>::ToString() const;
template <> HYP_API void CVar<float>::SetFromString(const String& value);
template <> HYP_API String CVar<float>::ToString() const;
template <> HYP_API void CVar<bool>::SetFromString(const String& value);
template <> HYP_API String CVar<bool>::ToString() const;
template <> HYP_API void CVar<String>::SetFromString(const String& value);
template <> HYP_API String CVar<String>::ToString() const;

template <> HYP_API int CVar<int>::Get() const;
template <> HYP_API float CVar<float>::Get() const;
template <> HYP_API bool CVar<bool>::Get() const;
template <> HYP_API String CVar<String>::Get() const;

struct CVarSnapshot
{
    CVarSnapshotValue values[MaxCVars];
    int numVars;
    uint32 version;

    CVarSnapshot()
        : values {},
          numVars(0),
          version(0)
    {
    }
};

class CVarManager
{
public:
    static CVarManager &GetInstance();

    CVarManager();
    ~CVarManager();

    CVarBase *FindVar(Name name) const;

    template <typename T>
    void SetVar(Name name, T value);

    template <typename T>
    T GetVar(StringHash nameHash) const;

    /*! \brief Publishes the cvar states so they're visible to other threads.
     *  Call once per frame at end of frame. */
    void Advance();

    uint32 GetVersion() const;

    const CVarSnapshot& GetCurrentSnapshot() const;

    FixedArray<CVarBase *, MaxCVars> vars;

private:
    int FindVarIndex(StringHash nameHash) const;

    CVarSnapshot m_snapshots[RingBufferDepth];
    AtomicVar<uint32> m_snapshotIndex;
    Mutex m_mutex;
};

} // namespace Hyperion
