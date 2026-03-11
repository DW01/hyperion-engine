/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <EditorPch.hpp>

#include <editor/ui/debug/StatsOverlay.hpp>

#include <ui/UIListView.hpp>
#include <ui/UIText.hpp>

#include <scene/World.hpp>

#include <engine/EngineStats.hpp>

#include <StatsOverlay.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

#pragma region StatsOverlay

StatsOverlay::StatsOverlay()
{
}

StatsOverlay::~StatsOverlay() = default;

Handle<UIObject> StatsOverlay::CreateUIObject_Impl(UIObject* spawnParent)
{
    HYP_SCOPE;

    Handle<UIPanel> panelBackdrop = spawnParent->CreateUIObject<UIPanel>(
        NAME_FMT("StatsOverlay_PanelBackdrop"),
        Vec2i(5, 5),
        UIObjectSize({ 250, UIObjectSize::PIXEL }, { 0, UIObjectSize::AUTO }));

    panelBackdrop->SetPadding(Vec2i(1, 1));
    panelBackdrop->SetBackgroundColor(Color(0.7f, 0.7f, 0.7f, 0.5f));
    panelBackdrop->SetBorderRadius(5);

    m_panel = spawnParent->CreateUIObject<UIListView>(
        NAME_FMT("StatsOverlay_Panel"),
        Vec2i::Zero(),
        UIObjectSize(0, UIObjectSize::AUTO));

    m_panel->SetBackgroundColor(Color(0.0f, 0.0f, 0.0f, 0.5f));
    m_panel->SetPadding(Vec2i(10, 10));
    m_panel->SetTextSize(14.0f);
    m_panel->SetTextColor(Color(0.7f, 0.7f, 0.7f, 1.0f));

    Handle<UIText> titleText = m_panel->CreateUIObject<UIText>(
        NAME_FMT("StatsOverlay_Panel_Title"),
        Vec2i::Zero(),
        UIObjectSize(Vec2i::Zero(), UIObjectSize::AUTO));
    titleText->SetText("Stats");
    m_panel->AddChildUIObject(titleText);

    panelBackdrop->AddChildUIObject(m_panel);

    return panelBackdrop;
}

HYP_DISABLE_OPTIMIZATION;

void StatsOverlay::Update_Impl(float delta)
{
    HYP_SCOPE;

    const Handle<EngineStats>& engineStats = EngineStats::GetInstance();

    static auto AddStatElemToGroup = [](UIObject& parentUIObject, const EngineStatsSnapshot& snapshot, EngineStatBase& stat)
    {
        Handle<UIObject> statTextElement = parentUIObject.FindChildUIObject(stat.name);

        if (!statTextElement)
        {
            statTextElement = parentUIObject.CreateUIObject<UIText>(
                stat.name,
                Vec2i::Zero(),
                UIObjectSize(Vec2i::Zero(), UIObjectSize::AUTO));

            statTextElement->SetPadding(Vec2i(1, 1));

            parentUIObject.AddChildUIObject(statTextElement);
        }

        statTextElement->SetText(HYP_FORMAT("{}: {}", stat.name, snapshot[stat].value));
    };

    using IterateGroupFunctorRef = ProcRef<void(UIListView& parentUIObject, const EngineStatGroup& group, const EngineStatsSnapshot& snapshot)>;
    IterateGroupFunctorRef IterateGroup = nullptr;

    auto IterateGroupImpl = [&IterateGroup](UIListView& parentUIObject, const EngineStatGroup& group, const EngineStatsSnapshot& snapshot)
    {
        for (EngineStatBase* stat : group.stats)
        {
            if (stat->type == EST_GROUP)
            {
                Handle<UIObject> groupUIObject = parentUIObject.FindChildUIObject(stat->name);
                if (!groupUIObject)
                {
                    groupUIObject = parentUIObject.CreateUIObject<UIListView>(
                        stat->name,
                        Vec2i::Zero(),
                        UIObjectSize(Vec2i::Zero(), UIObjectSize::AUTO));

                    UIListView* groupListView = static_cast<UIListView*>(groupUIObject.Get());
                    groupListView->SetBackgroundColor(Color(0.0f, 0.0f, 0.0f, 0.0f));
                    groupListView->SetOrientation(UIListViewOrientation::VERTICAL);

                    Handle<UIText> headingText = parentUIObject.CreateUIObject<UIText>();
                    headingText->SetTextColor(Color(0.8f, 0.8f, 0.8f, 1.0f));
                    headingText->SetText(*stat->name);
                    groupUIObject->AddChildUIObject(headingText);

                    parentUIObject.AddChildUIObject(groupUIObject);
                }

                IterateGroup(static_cast<UIListView&>(*groupUIObject), *static_cast<EngineStatGroup*>(stat), snapshot);
            }
            else
            {
                AddStatElemToGroup(parentUIObject, snapshot, *stat);
            }
        }
    };

    IterateGroup = IterateGroupImpl;

    IterateGroup(*m_panel, *engineStats->root, engineStats->GetCurrentSnapshot());
}

HYP_ENABLE_OPTIMIZATION;

#pragma endregion StatsOverlay

} // namespace Hyperion
