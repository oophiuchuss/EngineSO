module;

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <stdexcept>
#include <optional>
#include <string>
#include <utility>

module ImGuiSystem;

import EventDispatcher;
import KeyEvent;
import MouseButtonEvent;
import CursorCaptureRequestEvent;
import PostProcessSettingsChangedEvent;
import PerformanceStats;
import Scene;
import SceneEnvironment;

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

	EventSystemRef.AddListener(
		this,
		static_cast<int>(EventCategory::Input) |
		static_cast<int>(EventCategory::Window),
		2);
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

void ImGuiSystem::BuildPanels(const PerformanceStats& Stats, Scene& CurrentScene)
{
	ImGui::SetCurrentContext(Context);

	ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

	if (bShowDemoWindow)
	{
		ImGui::ShowDemoWindow(&bShowDemoWindow);
	}

	if (bShowPerformancePanel)
	{
		BuildPerformancePanel(Stats);
	}

	if (bShowPostProcessPanel)
	{
		BuildPostProcessPanel();
	}

	if (bShowEnvironmentPanel)
	{
		BuildEnvironmentPanel(CurrentScene);
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

	bool bCursorCaptureDispatched = Dispatcher.Dispatch<CursorCaptureRequestEvent>([](const CursorCaptureRequestEvent& E)
		{
			if (E.ShouldCapture())
			{
				ImGui::SetWindowFocus(nullptr);
			}
		});

	if (bCursorCaptureDispatched)
	{
		return EventReply::Unhandled;
	}

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

void ImGuiSystem::BuildPostProcessPanel()
{
	bool bSettingsChanged = false;

	if (ImGui::Begin("Post Processing", &bShowPostProcessPanel))
	{
		bSettingsChanged |= ImGui::DragFloat(
			"Exposure",
			&EditablePostProcessSettings.Exposure,
			0.05f,
			0.0f,
			20.0f,
			"%.2f",
			ImGuiSliderFlags_AlwaysClamp);

		bSettingsChanged |= ImGui::Checkbox(
			"Tone Mapping",
			&EditablePostProcessSettings.bToneMapping);

		bSettingsChanged |= ImGui::Checkbox(
			"Gamma Correction",
			&EditablePostProcessSettings.bGammaCorrection);

		bSettingsChanged |= ImGui::Checkbox(
			"Dithering",
			&EditablePostProcessSettings.bDithering);
	}

	ImGui::End();

	if (bSettingsChanged)
	{
		EventSystemRef.PublishEvent(
			PostProcessSettingsChangedEvent(EditablePostProcessSettings));
	}
}

void ImGuiSystem::BuildPerformancePanel(const PerformanceStats& Stats)
{
	ImGuiTableFlags TableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp;

	if (ImGui::Begin("Performance", &bShowPerformancePanel))
	{
		ImGui::Text("FPS: %.1f", Stats.FramesPerSecond);

		ImGui::Text("CPU frame: %.3f ms", Stats.CPUFrameMilliseconds);

		ImGui::Text("GPU frame: %.3f ms", Stats.TotalGPUFrameMilliseconds);

		ImGui::SeparatorText("GPU scopes");

		if (Stats.GPUScopeTimings.empty())
		{
			ImGui::TextDisabled("Waiting for GPU timing data...");
		}
		else if (ImGui::BeginTable("GPU timing table", 2, TableFlags))
		{
			ImGui::TableSetupColumn("Scope");
			ImGui::TableSetupColumn("Time (ms)", ImGuiTableColumnFlags_WidthFixed);

			ImGui::TableHeadersRow();

			for (const GPUScopeTiming& Scope : Stats.GPUScopeTimings)
			{
				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				ImGui::TextUnformatted(Scope.Label.c_str());

				ImGui::TableSetColumnIndex(1);
				ImGui::Text("%.3f", Scope.Milliseconds);
			}

			ImGui::EndTable();
		}
	}

	ImGui::End();
}

void ImGuiSystem::BuildEnvironmentPanel(Scene& CurrentScene)
{
	if (ImGui::Begin("Environment", &bShowEnvironmentPanel))
	{
		const std::optional<SceneEnvironment>& Environment = CurrentScene.GetEnvironment();

		if (!Environment.has_value())
		{
			ImGui::TextDisabled("No environment assigned.");
		}
		else
		{
			SceneEnvironment UpdatedEnvironment = *Environment;

			const std::string& ResourceID = UpdatedEnvironment.Cubemap.GetResourceID();

			ImGui::TextUnformatted("Cubemap");
			ImGui::SameLine();

			ImGui::TextDisabled(
				"%s",
				ResourceID.empty()
				? "<invalid>"
				: ResourceID.c_str());

			glm::quat Orientation = UpdatedEnvironment.Orientation;

			constexpr float MinOrientationLengthSquared = 0.000001f;

			if (glm::dot(Orientation, Orientation) <= MinOrientationLengthSquared)
			{
				Orientation = glm::quat(
					1.0f,
					0.0f,
					0.0f,
					0.0f);
			}
			else
			{
				Orientation = glm::normalize(Orientation);
			}

			float Intensity = UpdatedEnvironment.Intensity;

			glm::vec3 RotationDegrees = glm::degrees(glm::eulerAngles(Orientation));

			bool bIntensityChanged = ImGui::DragFloat(
				"Intensity",
				&Intensity,
				0.05f,
				0.0f,
				50.0f,
				"%.2f",
				ImGuiSliderFlags_AlwaysClamp);

			bool bRotationChanged = ImGui::DragFloat3(
				"Rotation",
				&RotationDegrees.x,
				0.25f,
				-180.0f,
				180.0f,
				"%.1f deg",
				ImGuiSliderFlags_AlwaysClamp);

			if (ImGui::Button("Reset Intensity"))
			{
				Intensity = 1.0f;
				bIntensityChanged = true;
			}

			ImGui::SameLine();

			if (ImGui::Button("Reset Rotation"))
			{
				RotationDegrees = glm::vec3(0.0f);
				bRotationChanged = true;
			}

			if (bIntensityChanged || bRotationChanged)
			{
				if (bIntensityChanged)
				{
					UpdatedEnvironment.Intensity = Intensity;
				}

				if (bRotationChanged)
				{
					UpdatedEnvironment.Orientation = glm::quat(glm::radians(RotationDegrees));
				}

				CurrentScene.SetEnvironment(std::move(UpdatedEnvironment));
			}
		}
	}

	ImGui::End();
}
