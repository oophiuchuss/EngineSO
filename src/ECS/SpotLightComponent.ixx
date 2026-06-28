module;

#include <glm/glm.hpp>

export module SpotLightComponent;

import LightComponentBase;

export class SpotLightComponent : public LightComponentBase
{
public:
    LightType GetType() const override { return LightType::Spot; }

    void SetRange(float Range) { Range = Range; }
    void SetInnerConeAngle(float Angle) { InnerConeAngle = Angle; }
    void SetOuterConeAngle(float Angle) { OuterConeAngle = Angle; }

    float GetRange() const { return Range; }
    float GetInnerConeAngle() const { return InnerConeAngle; }
    float GetOuterConeAngle() const { return OuterConeAngle; }

private:
    float Range = 10.0f;
    float InnerConeAngle = glm::radians(20.0f);
    float OuterConeAngle = glm::radians(30.0f);
};