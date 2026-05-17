#include <HyperionPch.hpp>

#include <scripting/asset/ScriptAsset.hpp>

#include <asset/AssetRegistry.hpp>
#include <asset/BlobStorage.hpp>

#include <Core/logging/Logger.hpp>

#include <ScriptAsset.generated.inl>

namespace Hyperion {

ScriptAsset::~ScriptAsset()
{
    FreeBlobData(m_data);
}

void ScriptAsset::Init()
{
    AssetObject::Init();
}

void ScriptAsset::SetSourceCode(const String& sourceCode)
{
    FreeBlobData(m_data);
    AllocateBlobData(m_data, sourceCode.Data(), sourceCode.Length(), 1);

    MarkDirty();
}

String ScriptAsset::GetSourceCode() const
{
    // @TODO data will hold HypScript bytecode or .NET assembly binary instead of source text,
    // we'll need to load source from file in editor builds
    if (m_data.raw == nullptr || m_data.size == 0)
    {
        return String();
    }

    return String(reinterpret_cast<const char*>(m_data.raw));
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

    if (m_data.raw == nullptr
        && m_data.key
        && m_data.size != 0)
    {
        if (!blobStorage || !blobStorage->GetData(m_data.key, m_data.size, m_data.raw))
        {
#if HYP_EDITOR || HYP_ALLOW_INLINE_BLOBS
            FileByteReader stream { registry->GetRootPath() / AssetBuckets::Scripts.GetName() / (String(*GetName()) + ".SCR.raw.blob") };

            if (!stream.Eof())
            {
                ByteBuffer buffer = stream.Read(stream.Max());

                AllocateBlobData(m_data, buffer.Data(), buffer.Size(), 1);

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
            m_data.readOnly = true;
        }
    }
}

void ScriptAsset::UnpageBlobData()
{
    if (m_data.readOnly)
    {
        m_data.raw = nullptr;
    }
}

} // namespace Hyperion
