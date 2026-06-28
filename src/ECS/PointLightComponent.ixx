export module PointLightComponent;

import LightComponentBase;

export class PointLightComponent : public LightComponentBase
{
public:
    LightType GetType() const override { return LightType::Point; }

    void SetRange(float Range) { Range = Range; }
    float GetRange() const { return Range; }

private:
    float Range = 10.0f;
};