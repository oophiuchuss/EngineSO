module;

#include <string>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

export module RenderPassBase;

import FrameData;

export class Rendergraph;

// Base render pass representation within the graph structure
// Each pass represents an operation with distinct inputs and outputs
export class RenderPassBase
{
public:
	explicit RenderPassBase(std::string InName) : Name(InName) {}
	virtual ~RenderPassBase() = default;

	inline const std::string& GetName() const { return Name; } 

	void AddInput(const std::string& NewInput) { Inputs.push_back(NewInput); }

	inline const std::vector<std::string>& GetInputs() const { return Inputs; }

	void AddOutput(const std::string& NewOutput) { Outputs.push_back(NewOutput); }
	
	inline const std::vector<std::string>& GetOutputs() const { return Outputs; }

	void SetEnabled(bool bNewEnabled) { bIsEnabled = bNewEnabled; }
	
	inline bool IsEnabled() const { return bIsEnabled; }

	virtual void Execute(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& CurrentFrameData)
	{
		if (!bIsEnabled)
		{
			return;
		}

		BeginPass(Cmd, Graph, CurrentFrameData);
		ExecuteMainLogic(Cmd, Graph, CurrentFrameData);
		EndPass(Cmd, Graph, CurrentFrameData);
	}

protected:
	virtual void BeginPass(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& CurrentFrameData) = 0;
	virtual void ExecuteMainLogic(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& CurrentFrameData) = 0;
	virtual void EndPass(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& CurrentFrameData) = 0;

private:
	std::string Name;					// Human-readable name
	std::vector<std::string> Inputs;	// Resources this pass will read (dependencies)
	std::vector<std::string> Outputs;	// Resources this pass will write to (products)
	bool bIsEnabled = true;				// Is current pass enabled
};