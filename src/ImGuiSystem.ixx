module;

#include <imgui.h>

export module ImGuiSystem;

import EventBase;
import EventListener;
import EventSystem;
import WindowSystem;
import PostProcessSettings;
import PerformanceStats;

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
    void BuildPanels(const PerformanceStats& Stats);
    void EndFrame();

    EventReply OnEvent(const EventBase& Event) override;

    ImDrawData* GetDrawData() const;

private:
    void BuildPostProcessPanel();

    void BuildPerformancePanel(const PerformanceStats& Stats);

    bool bShowPerformancePanel = true;

    PostProcessSettings EditablePostProcessSettings;
    bool bShowPostProcessPanel = true;

    EventSystem& EventSystemRef;
    ImGuiContext* Context = nullptr;

    bool bShowDemoWindow = false;
};