/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#if defined(HYP_TESTS) && defined(HYP_DOTNET)

#include <Framework/Testing/EngineTest.hpp>

#include <DotNET/ManagedClass.hpp>

#include <Core/Reflection/TypeId.hpp>

namespace Hyperion {
namespace tests {

namespace {

// a hierarchy that's never loaded from an assembly, so nothing needs the .NET runtime
struct ManagedClassHierarchy
{
    dotnet::ManagedClass base { WeakPtr<dotnet::Assembly>(), "Base", 0, TypeId::ForType<int>(), nullptr, nullptr, ManagedClassFlags::CLASS_TYPE };
    dotnet::ManagedClass middle { WeakPtr<dotnet::Assembly>(), "Middle", 0, TypeId::ForType<int>(), nullptr, &base, ManagedClassFlags::CLASS_TYPE };
    dotnet::ManagedClass derived { WeakPtr<dotnet::Assembly>(), "Derived", 0, TypeId::ForType<int>(), nullptr, &middle, ManagedClassFlags::CLASS_TYPE };
    dotnet::ManagedClass unrelated { WeakPtr<dotnet::Assembly>(), "Unrelated", 0, TypeId::ForType<int>(), nullptr, nullptr, ManagedClassFlags::CLASS_TYPE };
};

} // namespace

HYP_ENGINE_TEST(ManagedClass, HasParentClassByPointer)
{
    ManagedClassHierarchy hierarchy;

    HYP_TEST_CHECK(hierarchy.derived.HasParentClass(&hierarchy.middle));
    HYP_TEST_CHECK(hierarchy.derived.HasParentClass(&hierarchy.base));
    HYP_TEST_CHECK(hierarchy.middle.HasParentClass(&hierarchy.base));

    HYP_TEST_CHECK(!hierarchy.derived.HasParentClass(&hierarchy.unrelated));
    HYP_TEST_CHECK(!hierarchy.base.HasParentClass(&hierarchy.derived));
    HYP_TEST_CHECK(!hierarchy.derived.HasParentClass(&hierarchy.derived));
    HYP_TEST_CHECK(!hierarchy.derived.HasParentClass(static_cast<const dotnet::ManagedClass*>(nullptr)));
}

HYP_ENGINE_TEST(ManagedClass, HasParentClassByName)
{
    ManagedClassHierarchy hierarchy;

    HYP_TEST_CHECK(hierarchy.derived.HasParentClass("Middle"));
    HYP_TEST_CHECK(hierarchy.derived.HasParentClass("Base"));

    HYP_TEST_CHECK(!hierarchy.derived.HasParentClass("Unrelated"));
    HYP_TEST_CHECK(!hierarchy.derived.HasParentClass("Derived"));
}

} // namespace tests
} // namespace Hyperion

#endif // defined(HYP_TESTS) && defined(HYP_DOTNET)
