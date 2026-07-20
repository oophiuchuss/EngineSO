float NormalLengthToVariance(float FilteredNormalLength)
{
    float Length = saturate(FilteredNormalLength);

    return (1.0 - Length) / max(Length, 1.0e-4);
}

float ScaleNormalVariance(float NormalVariance, float NormalScale)
{
    return NormalVariance * NormalScale * NormalScale;
}

float NormalVarianceToLength(float NormalVariance)
{
    return 1.0 / (1.0 + max(NormalVariance, 0.0));
}

float ApplyNormalVarianceToRoughness(float Roughness, float NormalVariance)
{
    float BaseAlpha = Roughness * Roughness;

    float FilteredAlpha2 = saturate(BaseAlpha * BaseAlpha + NormalVariance);

    return sqrt(sqrt(FilteredAlpha2));
}