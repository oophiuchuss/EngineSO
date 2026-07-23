export module PostProcessSettings;

export struct PostProcessSettings
{
    float Exposure = 1.0f;

    bool bToneMapping = true;
    bool bGammaCorrection = true;
    bool bDithering = true;
};