/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <HyperionPch.hpp>

#include <engine/resources/ResourceTracker.hpp>

#include <Core/threading/Threads.hpp>

#include <Core/memory/pool/Pool.hpp>

namespace Hyperion {

HYP_API const TypeInfo& Class_GetTypeInfo(const Class& cls)
{
    return *cls.GetTypeInfo();
}

} // namespace Hyperion
