/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <editor/ui/debug/EditorDebugOverlay.hpp>

#include <Core/math/Color.hpp>

#include <Core/containers/Array.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

class World;
class UIListView;

HYP_CLASS()
class HYP_API StatsOverlay : public EditorDebugOverlayBase
{
    HYP_OBJECT_BODY(StatsOverlay);

public:
    explicit StatsOverlay();
    virtual ~StatsOverlay() override;

protected:
    HYP_METHOD()
    virtual Handle<UIObject> CreateUIObject_Impl(UIObject* spawnParent) override;

    HYP_METHOD()
    virtual int GetPlacement_Impl() const override
    {
        return 0;
    }

    HYP_METHOD()
    virtual void Update_Impl(float delta) override;

    HYP_METHOD()
    virtual bool IsEnabled_Impl() const override
    {
        return true;
    }

private:
    Handle<UIListView> m_panel;
};

} // namespace Hyperion
