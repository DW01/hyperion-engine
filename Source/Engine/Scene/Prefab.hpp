/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Types.hpp>

#include <Asset/AssetObject.hpp>

namespace Hyperion {

class Node;

HYP_CLASS(AssetBucket = "Prefabs")
class ENGINE_API Prefab final : public AssetObject
{
    HYP_OBJECT_BODY(Prefab);

public:
    Prefab();
    explicit Prefab(Name name, const Handle<Node>& root = Handle<Node>::Null());

    Prefab(const Prefab&) = delete;
    Prefab& operator=(const Prefab&) = delete;

    ~Prefab() override = default;

    HYP_METHOD(Property = "Root", Serialize)
    const Handle<Node>& GetRoot() const
    {
        return m_root;
    }

    HYP_METHOD(Property = "Root", Serialize)
    void SetRoot(const Handle<Node>& root);

private:
    HYP_FIELD(Property = "Root", Serialize)
    Handle<Node> m_root;
};

} // namespace Hyperion
