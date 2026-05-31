module;

#include <string>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

export module PostProcessPass;

import RenderPassBase;
import FrameData;

export class PostProcessEffect
{
public:
	virtual ~PostProcessEffect() = default;
	virtual void Apply(vk::raii::CommandBuffer& Cmd) = 0;
};

export class PostProcessPass : public RenderPassBase
{
public:
	explicit PostProcessPass(
		std::string InName,
		std::string InGBufferColorResourceName);

	void AddPostProcessEffect(PostProcessEffect* EffectToAdd);

	void RemovePostProcessEffect(PostProcessEffect* EffectToRemove);

protected:
	virtual void BeginPass(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& CurrentFrameData) override;

	virtual void ExecuteMainLogic(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& CurrentFrameData) override;

	virtual void EndPass(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph, FrameData& CurrentFrameData) override;

private:
	std::vector<PostProcessEffect*> Effects;

	std::string GBufferColorResourceName; // TODO: potentially optimize access to pointers
};