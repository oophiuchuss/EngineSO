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
#include <iterator>

module ImGuiSystem;

import EventDispatcher;
import KeyEvent;
import MouseButtonEvent;
import CursorCaptureRequestEvent;
import PostProcessSettingsChangedEvent;
import RenderDebugSettingsChangedEvent;
import TemporalAASettingsChangedEvent;
import DirectionalShadowSettingsChangedEvent;

import PerformanceStats;
import Scene;
import SceneEnvironment;
import RenderDebugSettings;
import DirectionalShadowSettings;

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

	if (bShowRenderDebugPanel)
	{
		BuildRenderDebugPanel();
	}

	if (bShowTemporalAAPanel)
	{
		BuildTemporalAAPanel();
	}

	if (bShowDirectionalShadowPanel)
	{
		BuildDirectionalShadowPanel();
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
		EventSystemRef.PublishEvent(PostProcessSettingsChangedEvent(EditablePostProcessSettings));
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

void ImGuiSystem::BuildRenderDebugPanel()
{
	bool bSettingsChanged = false;

	if (ImGui::Begin("Render Debug", &bShowRenderDebugPanel))
	{
		static constexpr const char* ViewNames[] =
		{
			"None",
			"Motion Vectors"
		};

		int SelectedView = static_cast<int>(EditableRenderDebugSettings.View);

		if (ImGui::Combo("View", &SelectedView, ViewNames, static_cast<int>(std::size(ViewNames))))
		{
			EditableRenderDebugSettings.View = static_cast<RenderDebugView>(SelectedView);

			bSettingsChanged = true;
		}

		bSettingsChanged |= ImGui::Checkbox(
			"Preview Projection Jitter",
			&EditableRenderDebugSettings.bPreviewProjectionJitter);

		if (EditableRenderDebugSettings.bPreviewProjectionJitter)
		{
			ImGui::TextDisabled("Temporal accumulation is bypassed so raw jitter remains visible.");
		}

		if (EditableRenderDebugSettings.View == RenderDebugView::MotionVectors)
		{
			bSettingsChanged |= ImGui::DragFloat(
				"Motion Vector Scale",
				&EditableRenderDebugSettings.MotionVectorScale,
				1.0f,
				1.0f,
				500.0f,
				"%.1f",
				ImGuiSliderFlags_AlwaysClamp);

			ImGui::Separator();

			ImGui::TextUnformatted("Neutral grey means zero velocity.");

			ImGui::TextUnformatted("Red channel represents horizontal motion.");

			ImGui::TextUnformatted("Green channel represents vertical motion.");
		}
	}

	ImGui::End();

	if (bSettingsChanged)
	{
		EventSystemRef.PublishEvent(RenderDebugSettingsChangedEvent(EditableRenderDebugSettings));
	}
}

void ImGuiSystem::BuildTemporalAAPanel()
{
	bool bSettingsChanged = false;

	if (ImGui::Begin("Temporal Anti-Aliasing", &bShowTemporalAAPanel))
	{
		bSettingsChanged |= ImGui::Checkbox(
			"Enabled",
			&EditableTemporalAASettings.bEnabled);

		bSettingsChanged |= ImGui::DragFloat(
			"History Weight",
			&EditableTemporalAASettings.HistoryWeight,
			0.01f,
			0.0f,
			0.99f,
			"%.2f",
			ImGuiSliderFlags_AlwaysClamp);

		bSettingsChanged |= ImGui::DragFloat(
			"Responsive History Weight",
			&EditableTemporalAASettings.ResponsiveHistoryWeight,
			0.01f,
			0.0f,
			0.99f,
			"%.2f",
			ImGuiSliderFlags_AlwaysClamp);

		if (EditableTemporalAASettings.ResponsiveHistoryWeight > EditableTemporalAASettings.HistoryWeight)
		{
			EditableTemporalAASettings.ResponsiveHistoryWeight = EditableTemporalAASettings.HistoryWeight;

			bSettingsChanged = true;
		}

		ImGui::TextDisabled("History Weight is used for stable pixels.");

		ImGui::TextDisabled("Responsive Weight is used for moving pixels whose history disagrees.");

		bSettingsChanged |= ImGui::DragFloat(
			"Depth Tolerance",
			&EditableTemporalAASettings.DepthTolerance,
			0.0001f,
			0.0f,
			0.02f,
			"%.4f",
			ImGuiSliderFlags_AlwaysClamp);

		ImGui::TextDisabled(
			"Depth tolerance is measured in device-depth space.");
	}

	ImGui::End();

	if (bSettingsChanged)
	{
		EventSystemRef.PublishEvent(TemporalAASettingsChangedEvent(EditableTemporalAASettings));
	}
}

void ImGuiSystem::BuildDirectionalShadowPanel()
{
	bool bSettingsChanged = false;

	if (ImGui::Begin("Directional Shadows", &bShowDirectionalShadowPanel))
	{
		bSettingsChanged |= ImGui::Checkbox(
			"Enabled",
			&EditableDirectionalShadowSettings.bEnabled);

		static constexpr uint32_t ResolutionValues[] =
		{
			1024,
			2048,
			4096
		};

		static constexpr const char* ResolutionNames[] =
		{
			"1024",
			"2048",
			"4096"
		};

		int SelectedResolution = 0;

		for (int Index = 0; Index < static_cast<int>(std::size(ResolutionValues)); Index++)
		{
			if (EditableDirectionalShadowSettings.Resolution == ResolutionValues[Index])
			{
				SelectedResolution = Index;
				break;
			}
		}

		if (ImGui::Combo("Resolution", &SelectedResolution, ResolutionNames, static_cast<int>(std::size(ResolutionNames))))
		{
			EditableDirectionalShadowSettings.Resolution = ResolutionValues[SelectedResolution];

			bSettingsChanged = true;
		}

		ImGui::TextDisabled("Changing resolution recreates shadow-map resources.");

		ImGui::Separator();

		ImGui::BeginDisabled(!EditableDirectionalShadowSettings.bEnabled);

		bSettingsChanged |= ImGui::Checkbox("3x3 PCF", &EditableDirectionalShadowSettings.bPCFEnabled);

		bSettingsChanged |= ImGui::DragFloat(
			"Shadow Distance",
			&EditableDirectionalShadowSettings.ShadowDistance,
			1.0f,
			1.0f,
			500.0f,
			"%.1f",
				ImGuiSliderFlags_AlwaysClamp);

		bSettingsChanged |= ImGui::DragFloat(
			"Depth Padding",
			&EditableDirectionalShadowSettings.DepthPadding,
			1.0f,
			0.0f,
			200.0f,
			"%.1f",
			ImGuiSliderFlags_AlwaysClamp);

		ImGui::SeparatorText("Bias");

		bSettingsChanged |= ImGui::DragFloat(
			"Raster Constant Bias",
			&EditableDirectionalShadowSettings.RasterConstantBias,
			0.05f,
			0.0f,
			10.0f,
			"%.2f",
			ImGuiSliderFlags_AlwaysClamp);

		bSettingsChanged |= ImGui::DragFloat(
			"Raster Slo pe Bias",
			&EditableDirectionalShadowSettings.RasterSlopeBias,
			0.05f,
			0.0f,
			10.0f,
			"%.2f",
			ImGuiSliderFlags_AlwaysClamp);

		bSettingsChanged |= ImGui::DragFloat(
			"Receiver Bias",
			&EditableDirectionalShadowSettings.ReceiverBias,
			0.00005f,
			0.0f,
			0.01f,
			"%.6f",
			ImGuiSliderFlags_AlwaysClamp);

		ImGui::SeparatorText("Debug");

		static constexpr const char* DebugViewNames[] =
		{
			"None",
			"Shadow Depth",
			"Visibility"
		};

		int SelectedDebugView = static_cast<int>(EditableDirectionalShadowSettings.DebugView);

		if (ImGui::Combo("Debug View", &SelectedDebugView, DebugViewNames, static_cast<int>(std::size(DebugViewNames))))
		{
			EditableDirectionalShadowSettings.DebugView = static_cast<DirectionalShadowDebugView>(SelectedDebugView);

			bSettingsChanged = true;
		}

		ImGui::EndDisabled();

		ImGui::Separator();

		ImGui::TextDisabled("Constant and slope bias affect shadow-map rasterization.");

		ImGui::TextDisabled("Receiver bias affects the lighting comparison.");
	}

	ImGui::End();

	if (bSettingsChanged)
	{
		EventSystemRef.PublishEvent(DirectionalShadowSettingsChangedEvent(EditableDirectionalShadowSettings));
	}
}
