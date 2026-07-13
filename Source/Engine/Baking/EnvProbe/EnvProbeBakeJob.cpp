/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Baking/EnvProbe/EnvProbeBakeJob.hpp>

#include <Scene/Scene.hpp>
#include <Scene/EnvProbe.hpp>

namespace Hyperion {
namespace Baking {

BakeJob<EnvProbe>::~BakeJob()
{
}

void BakeJob<EnvProbe>::Start_Internal()
{
}

void BakeJob<EnvProbe>::Process_Internal(bool* outIsReadyToProcess)
{
    if (outIsReadyToProcess)
    {
        *outIsReadyToProcess = true;
    }
}

} // namespace Baking
} // namespace Hyperion
