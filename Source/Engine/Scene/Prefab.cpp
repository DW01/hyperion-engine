/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/Prefab.hpp>
#include <Scene/Node.hpp>

#include <Prefab.generated.inl>

namespace Hyperion {

Prefab::Prefab()
    : Prefab(Name::Invalid())
{
}

Prefab::Prefab(Name name, const Handle<Node>& root)
    : AssetObject(name),
      m_root(root)
{
}

void Prefab::SetRoot(const Handle<Node>& root)
{
    if (root == m_root)
    {
        return;
    }

    m_root = root;
    MarkDirty();
}

} // namespace Hyperion
