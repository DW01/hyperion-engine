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

namespace JSON {
// Fwd declaration for ConfigValue
class Value;
} // namespace JSON

namespace config {
class ConfigBase;
using ConfigValue = JSON::Value;
} // namespace config

using config::ConfigValue;
using config::ConfigBase;

class CVarManager;

using CVarSnapshotValue = Variant<
    int8, int16, int32, int64,
    uint8, uint16, uint32, uint64,
    float, double,
    bool,
    String>;

class CVarBase
{
public:
    friend class CVarManager;

protected:
    explicit CVarBase(UTF8StringView path);

public:
    Name name;
    int id;
    bool isHeapAllocated;

    virtual ~CVarBase() = default;

    virtual void SetFromConfig(const ConfigValue& cfgValue) = 0;
    
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

    const T& Get() const;

    void SetFromConfig(const ConfigValue& cfgValue) override;

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

#pragma region SetFromConfig specializations

template <> HYP_API void CVar<int8>::SetFromConfig(const ConfigValue& cfgValue);
template <> HYP_API void CVar<int16>::SetFromConfig(const ConfigValue& cfgValue);
template <> HYP_API void CVar<int32>::SetFromConfig(const ConfigValue& cfgValue);
template <> HYP_API void CVar<int64>::SetFromConfig(const ConfigValue& cfgValue);

template <> HYP_API void CVar<uint8>::SetFromConfig(const ConfigValue& cfgValue);
template <> HYP_API void CVar<uint16>::SetFromConfig(const ConfigValue& cfgValue);
template <> HYP_API void CVar<uint32>::SetFromConfig(const ConfigValue& cfgValue);
template <> HYP_API void CVar<uint64>::SetFromConfig(const ConfigValue& cfgValue);

template <> HYP_API void CVar<float>::SetFromConfig(const ConfigValue& cfgValue);
template <> HYP_API void CVar<double>::SetFromConfig(const ConfigValue& cfgValue);

template <> HYP_API void CVar<bool>::SetFromConfig(const ConfigValue& cfgValue);

template <> HYP_API void CVar<String>::SetFromConfig(const ConfigValue& cfgValue);

#pragma endregion SetFromConfig specializations

#pragma region Get specializations

template <> const int8& CVar<int8>::Get() const;
template <> const int16& CVar<int16>::Get() const;
template <> const int32& CVar<int32>::Get() const;
template <> const int64& CVar<int64>::Get() const;

template <> const uint8& CVar<uint8>::Get() const;
template <> const uint16& CVar<uint16>::Get() const;
template <> const uint32& CVar<uint32>::Get() const;
template <> const uint64& CVar<uint64>::Get() const;

template <> const float& CVar<float>::Get() const;
template <> const double& CVar<double>::Get() const;

template <> const bool& CVar<bool>::Get() const;

template <> const String& CVar<String>::Get() const;

#pragma endregion Get specializations

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
    static CVarManager& GetInstance();

    CVarManager();
    ~CVarManager();

    void InitFromConfig(const ConfigBase& config);

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
