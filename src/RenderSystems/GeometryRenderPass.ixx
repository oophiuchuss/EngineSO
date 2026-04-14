module;

#include <string>

export module GeometryRenderPass;

import RenderPassBase;
import CullingSystem;

export class GeometryRenderPass : public RenderPassBase
{
public:
	explicit GeometryRenderPass(
		std::string InName,
		CullingSystem* InCulling,
		std::string InGBufferColorResourceName,
		std::string InGBufferDepthResourceName);

protected:
	virtual void BeginPass(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph) override;
	
	virtual void ExecuteMainLogic(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph) override;
	
	virtual void EndPass(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph) override;

private:
	CullingSystem* CullingSystemPtr;

	std::string GBufferColorResourceName; // TODO: potentially optimize access to pointers
	std::string GBufferDepthResourceName; // TODO: potentially optimize access to pointers
};