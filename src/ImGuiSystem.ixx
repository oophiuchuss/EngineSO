module;

#include <imgui.h>

export module ImGuiSystem;

import EventBase;
import EventListener;
import EventSystem;
import WindowSystem;
import Scene;

import PerformanceStats;

import PostProcessSettings;
import RenderDebugSettings;
import TemporalAASettings;
import DirectionalShadowSettings;

export class ImGuiSystem final : public EventListener
{
public:
    ImGuiSystem(
        WindowSystem& InWindowSystem,
        EventSystem& InEventSystem);

    ~ImGuiSystem() override;

    ImGuiSystem(const ImGuiSystem&) = delete;
    ImGuiSystem& operator=(const ImGuiSystem&) = delete;

    void BeginFrame();
    void BuildPanels(const PerformanceStats& Stats, Scene& CurrentScene);
    void EndFrame();

    EventReply OnEvent(const EventBase& Event) override;

    ImDrawData* GetDrawData() const;

private:
    void BuildPostProcessPanel();

    void BuildPerformancePanel(const PerformanceStats& Stats);
    void BuildEnvironmentPanel(Scene& CurrentScene);

    void BuildRenderDebugPanel();

    void BuildTemporalAAPanel();

    void BuildDirectionalShadowPanel();

    bool bShowPerformancePanel = true;

    PostProcessSettings EditablePostProcessSettings;
    bool bShowPostProcessPanel = true;

    bool bShowEnvironmentPanel = true;

    RenderDebugSettings EditableRenderDebugSettings;
    bool bShowRenderDebugPanel = true;

    TemporalAASettings EditableTemporalAASettings;
    bool bShowTemporalAAPanel = true;

    DirectionalShadowSettings EditableDirectionalShadowSettings;
    bool bShowDirectionalShadowPanel = true;

    EventSystem& EventSystemRef;
    ImGuiContext* Context = nullptr;

    bool bShowDemoWindow = false;
};