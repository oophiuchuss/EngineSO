module;

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <stdexcept>

module ImGuiSystem;

import EventDispatcher;
import KeyEvent;
import MouseButtonEvent;

ImGuiSystem::ImGuiSystem(
	WindowSystem& InWindowSystem, 
	EventSystem& InEventSystem) :
	EventSystemRef(InEventSystem)
{
	IMGUI_CHECKVERSION();

	Context = ImGui::CreateContext();
	if (!Context)
	{
		throw std::runtime_error("Failed to create ImGui context");
	}

	ImGui::SetCurrentContext(Context);

	ImGuiIO& IO = ImGui::GetIO();
	IO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	ImGui::StyleColorsDark();

	if (!ImGui_ImplGlfw_InitForVulkan(InWindowSystem.GetHandle(), true))
	{
		ImGui::DestroyContext(Context);
		Context = nullptr;

		throw std::runtime_error("Failed to initialize ImGui GLFW backend");
	}

	EventSystemRef.AddListener(this, static_cast<int>(EventCategory::Input), 2);
}

ImGuiSystem::~ImGuiSystem()
{
	EventSystemRef.RemoveListener(this);

	if (Context)
	{
		ImGui::SetCurrentContext(Context);
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext(Context);
		Context = nullptr;
	}
}

void ImGuiSystem::BeginFrame()
{
	ImGui::SetCurrentContext(Context);

	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

void ImGuiSystem::BuildPanels()
{
	ImGui::SetCurrentContext(Context);

	ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);

	if (bShowDemoWindow)
	{
		ImGui::ShowDemoWindow(&bShowDemoWindow);
	}
}

void ImGuiSystem::EndFrame()
{
	ImGui::SetCurrentContext(Context);
	ImGui::Render();
}

EventReply ImGuiSystem::OnEvent(const EventBase& Event)
{
	ImGui::SetCurrentContext(Context);

	ImGuiIO& IO = ImGui::GetIO();
	EventDispatcher Dispatcher(Event);

	if (Event.IsInCategory(EventCategory::Keyboard))
	{
		bool bIsRelease = false;

		Dispatcher.Dispatch<KeyEvent>([&bIsRelease](const KeyEvent& E)
			{
				bIsRelease = E.GetAction() == KeyAction::Release;
			});

		if (bIsRelease)
		{
			return EventReply::Unhandled;
		}

		return IO.WantCaptureKeyboard ? EventReply::Handled : EventReply::Unhandled;
	}

	if (Event.IsInCategory(EventCategory::MouseButton))
	{
		bool bIsRelease = false;

		Dispatcher.Dispatch<MouseButtonEvent>([&bIsRelease](const MouseButtonEvent& E)
			{
				bIsRelease = E.GetAction() == MouseButtonAction::Release;
			});

		if (bIsRelease)
		{
			return EventReply::Unhandled;
		}

		return IO.WantCaptureMouse ? EventReply::Handled : EventReply::Unhandled;
	}

	if (Event.IsInCategory(EventCategory::Mouse))
	{
		return IO.WantCaptureMouse ? EventReply::Handled : EventReply::Unhandled;
	}

	return EventReply::Unhandled;
}

ImDrawData* ImGuiSystem::GetDrawData() const
{
	ImGui::SetCurrentContext(Context);
	return ImGui::GetDrawData();
}
