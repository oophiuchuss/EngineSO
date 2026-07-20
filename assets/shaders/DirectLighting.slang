import GPULightData;
import PBRBRDF;

static const float MIN_LOCAL_LIGHT_DISTANCE_SQUARED = 1.0e-8;

bool TryGetLocalLightGeometry(
    float3 LightPosition,
    float3 WorldPosition,
    float LightRange,
    out float3 LightDir,
    out float Distance)
{
    LightDir = float3(0.0);
    Distance = 0.0;

    if (LightRange <= 0.0)
    {
        return false;
    }

    float3 LightVector = LightPosition - WorldPosition;

    float DistanceSquared = dot(LightVector, LightVector);

    if (DistanceSquared <= MIN_LOCAL_LIGHT_DISTANCE_SQUARED)
    {
        return false;
    }

    Distance = sqrt(DistanceSquared);
    LightDir = LightVector / Distance;

    return true;
}

float3 EvaluateDirectLightContribution(
    GPULightData Light,
    float3 LightDir,
    float3 Normal,
    float3 Albedo,
    float Metallic,
    float Roughness,
    float3 ViewDir,
    float Attenuation)
{
    float NdotL = max(dot(Normal, LightDir), 0.0);

    float3 BRDF =
        EvaluateCookTorranceBRDF(
            LightDir,
            Normal,
            Albedo,
            Metallic,
            Roughness,
            ViewDir,
            NdotL);

    float3 IncomingRadiance = Light.Color_Intensity.rgb * Light.Color_Intensity.a * Attenuation;

    // Rendering equation includes the cosine between the surface normal
    // and incoming light direction.
    return BRDF * IncomingRadiance * NdotL;
}

float3 ApplyDirectionalLight(
    GPULightData Light,
    float3 Normal,
    float3 Albedo,
    float Metallic,
    float Roughness,
    float3 ViewDir)
{
    float3 LightDir = normalize(-Light.Direction.xyz);

    return EvaluateDirectLightContribution(
        Light,
        LightDir,
        Normal,
        Albedo,
        Metallic,
        Roughness,
        ViewDir,
        1.0);
}

float3 ApplyPointLight(
    GPULightData Light,
    float3 WorldPos,
    float3 Normal,
    float3 Albedo,
    float Metallic,
    float Roughness,
    float3 ViewDir)
{
    float3 LightDir;
    float Distance;

    if (!TryGetLocalLightGeometry(Light.Position.xyz, WorldPos, Light.Params.x, LightDir, Distance))
    {
        return float3(0.0);
    }

    float Attenuation = max(1.0 - Distance / Light.Params.x, 0.0);

    Attenuation *= Attenuation;

    return EvaluateDirectLightContribution(
        Light,
        LightDir,
        Normal,
        Albedo,
        Metallic,
        Roughness,
        ViewDir,
        Attenuation);
}

float3 ApplySpotLight(
    GPULightData Light,
    float3 WorldPos,
    float3 Normal,
    float3 Albedo,
    float Metallic,
    float Roughness,
    float3 ViewDir)
{
    float3 LightDir;
    float Distance;

    if (!TryGetLocalLightGeometry(Light.Position.xyz, WorldPos, Light.Params.x, LightDir, Distance))
    {
        return float3(0.0);
    }

    float DistanceAttenuation = max(1.0 - Distance / Light.Params.x, 0.0);

    DistanceAttenuation *= DistanceAttenuation;

    float CosTheta = dot(LightDir, -Light.Direction.xyz);

    float ConeAttenuation = smoothstep(Light.Params.z, Light.Params.y, CosTheta);

    return EvaluateDirectLightContribution(
        Light,
        LightDir,
        Normal,
        Albedo,
        Metallic,
        Roughness,
        ViewDir,
        DistanceAttenuation * ConeAttenuation);
}