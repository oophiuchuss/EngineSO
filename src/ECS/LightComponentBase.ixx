module;

#include <glm/glm.hpp>

export module LightComponentBase;

import Component;

export enum class LightType : uint32_t
{
    Directional = 0,
    Point = 1,
    Spot = 2
};

export class LightComponentBase : public ComponentBase
{
public:
    virtual LightType GetType() const = 0;

    void SetColor(const glm::vec3& Col) { Color = Col; }
    void SetIntensity(float I) { Intensity = I; }

    const glm::vec3& GetColor() const { return Color; }
    float GetIntensity() const { return Intensity; }

protected:
    glm::vec3 Color = glm::vec3(1.0f);
    float Intensity = 1.0f;
};