export module DirectionalLightComponent;

import LightComponentBase;

export class DirectionalLightComponent : public LightComponentBase
{
public:
    LightType GetType() const override
    {
        return LightType::Directional;
    }

    void SetCastsShadows(bool bInCastsShadows)
    {
        bCastsShadows = bInCastsShadows;
    }

    bool CastsShadows() const
    {
        return bCastsShadows;
    }

private:
    // Explicitly enabled by the scene so multiple directional lights
    // do not accidentally compete for the one shadow map.
    bool bCastsShadows = false;
};