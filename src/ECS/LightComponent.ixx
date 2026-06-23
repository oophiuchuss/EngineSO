module;

#include <glm/glm.hpp>

export module LightComponent;

import Component;
import LightData;

export class LightComponent : public ComponentBase
{
public:
    LightComponent() = default;

    // Set all light properties at once
    void SetLightData(const LightData& InData) { Data = InData; }

    // Individual setters for convenience
    void SetDirection(const glm::vec3& Dir) { Data.Direction = glm::normalize(Dir); }
    void SetIntensity(float I) { Data.Intensity = I; }
    void SetColor(const glm::vec3& Col) { Data.Color = Col; }

    const LightData& GetLightData() const { return Data; }

private:
    LightData Data;
};