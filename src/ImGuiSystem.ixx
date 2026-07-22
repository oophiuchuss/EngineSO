module;

#include <imgui.h>

export module ImGuiSystem;

import EventBase;
import EventListener;
import EventSystem;
import WindowSystem;

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
    void BuildPanels();
    void EndFrame();

    EventReply OnEvent(const EventBase& Event) override;

    ImDrawData* GetDrawData() const;

private:
    EventSystem& EventSystemRef;
    ImGuiContext* Context = nullptr;

    bool bShowDemoWindow = true;
};