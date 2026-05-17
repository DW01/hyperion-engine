#include <HyperionPch.hpp>

#include <scripting/asset/ScriptAsset.hpp>

#include <asset/AssetRegistry.hpp>
#include <asset/BlobStorage.hpp>

#include <Core/logging/Logger.hpp>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Assets);

ScriptAsset::~ScriptAsset()
{
    FreeBlobData(m_sourceData);
}

void ScriptAsset::Init()
{
    AssetObject::Init();
}

void ScriptAsset::SetSourceCode(const String& sourceCode)
{
    FreeBlobData(m_sourceData);
    AllocateBlobData(m_sourceData, sourceCode.Data(), sourceCode.Length(), 1);

    MarkDirty();
}

String ScriptAsset::GetSourceCode() const
{
    if (m_sourceData.raw == nullptr || m_sourceData.size == 0)
    {
        return String();
    }

    return String(reinterpret_cast<const char*>(m_sourceData.raw));
}

void ScriptAsset::PageBlobData()
{
    if (IsTransient() || !IsRegistered())
    {
        return;
    }

    Handle<AssetRegistry> registry = GetAssetRegistry();
    AssertDebug(registry.IsValid());

    if (!registry.IsValid())
    {
        return;
    }

    BlobStorage* blobStorage = registry->HasBlobStorage() ? &registry->GetBlobStorage() : nullptr;

    if (m_sourceData.raw == nullptr
        && m_sourceData.key
        && m_sourceData.size != 0)
    {
        if (!blobStorage || !blobStorage->GetData(m_sourceData.key, m_sourceData.size, m_sourceData.raw))
        {
#if HYP_EDITOR || HYP_ALLOW_INLINE_BLOBS
            FileByteReader stream { registry->GetRootPath() / AssetBuckets::Scripts.GetName() / (String(*GetName()) + ".SCR.raw.blob") };

            if (!stream.Eof())
            {
                ByteBuffer buffer = stream.Read(stream.Max());

                AllocateBlobData(m_sourceData, buffer.Data(), buffer.Size(), 1);

                MarkDirty();

                Result saveResult = SaveBlobData(blobStorage);

                if (saveResult.HasError())
                {
                    HYP_LOG(Assets, Error, "Failed to save script blob data: {}", saveResult.GetError().GetMessage());
                }

                return;
            }
#endif
        }
        else
        {
            m_sourceData.readOnly = true;
        }
    }
}

void ScriptAsset::UnpageBlobData()
{
    if (m_sourceData.readOnly)
    {
        m_sourceData.raw = nullptr;
    }
}

} // namespace Hyperion
