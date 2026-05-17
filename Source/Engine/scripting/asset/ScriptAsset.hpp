/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <asset/AssetObject.hpp>

#include <scripting/Script.hpp>

namespace Hyperion {

HYP_CLASS(AssetBucket = "Scripts")
class ScriptAsset : public AssetObject
{
    HYP_OBJECT_BODY(ScriptAsset);

public:
    ScriptAsset()
        : AssetObject()
    {
    }

    explicit ScriptAsset(Name name)
        : AssetObject(name)
    {
    }

    ScriptAsset(Name name, const ScriptDesc& scriptDesc)
        : AssetObject(name),
          m_scriptDesc(scriptDesc)
    {
    }

    ScriptAsset(const ScriptAsset& other) = delete;
    ScriptAsset& operator=(const ScriptAsset& other) = delete;

    ScriptAsset(ScriptAsset&& other) noexcept = delete;
    ScriptAsset& operator=(ScriptAsset&& other) noexcept = delete;

    ~ScriptAsset();

    HYP_FORCE_INLINE ScriptDesc& GetScriptDesc()
    {
        return m_scriptDesc;
    }

    HYP_FORCE_INLINE const ScriptDesc& GetScriptDesc() const
    {
        return m_scriptDesc;
    }

    void SetSourceCode(const String& sourceCode);
    String GetSourceCode() const;

protected:
    void Init() override;

    void PageBlobData() override;
    void UnpageBlobData() override;

    void CollectBlobDataReferences(Array<Tuple<const char*, uint16, BlobDataReference*>>& outReferences) override
    {
        outReferences.EmplaceBack("SCR", 1, &m_sourceData);
    }

private:
    ScriptDesc m_scriptDesc;

    HYP_FIELD(Serialize)
    BlobDataReference m_sourceData;
};

} // namespace Hyperion
