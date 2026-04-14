module;

#include <string>
#include <vector>

export module LightingPass;

import RenderPassBase;

// TODO implement light class
export class Light
{

};

export class LightingPass : public RenderPassBase
{
public:
	explicit LightingPass(
		std::string InName,
		std::string InGBufferColorResourceName);

	void AddLight(Light* LightToAdd);

	void RemoveLight(Light* LightToRemove);

protected:
	virtual void BeginPass(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph) override;

	virtual void ExecuteMainLogic(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph) override;

	virtual void EndPass(vk::raii::CommandBuffer& Cmd, Rendergraph& Graph) override;

private:
	std::vector<Light*> Lights;

	std::string GBufferColorResourceName; // TODO: potentially optimize access to pointers
};