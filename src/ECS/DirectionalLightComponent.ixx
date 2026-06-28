export module DirectionalLightComponent;

import LightComponentBase;

export class DirectionalLightComponent : public LightComponentBase
{
public:
    LightType GetType() const override { return LightType::Directional; }
};