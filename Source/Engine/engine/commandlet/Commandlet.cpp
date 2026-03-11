/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <HyperionPch.hpp>

#include <engine/commandlet/Commandlet.hpp>

#include <Core/cli/CommandLine.hpp>

#include <Commandlet.generated.inl>

namespace Hyperion {

const CommandLineArgumentDefinitions& CommandletBase::GetArgumentDefinitions()
{
    static CommandLineArgumentDefinitions s_definitions;
    return s_definitions;
}

} // namespace Hyperion
