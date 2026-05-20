/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

namespace Hyperion {

class Entity;
struct ScriptComponent;
struct GameState;

namespace EntityScripting
{
void InitializeEntityScript(Entity* entity, ScriptComponent& scriptComponent, const GameState& gameState);
void ShutdownEntityScript(Entity* entity, ScriptComponent& scriptComponent, const GameState& gameState);

} // namespace EntityScripting

} // namespace Hyperion
