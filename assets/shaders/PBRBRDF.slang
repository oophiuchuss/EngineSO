static const float PBR_PI = 3.14159265;

// Evaluates the material's Cook-Torrance BRDF for one incoming light direction.
//
// LightDir, Normal and ViewDir are expected to be normalized.
// The returned value does not include incoming radiance or NdotL.
float3 EvaluateCookTorranceBRDF(
    float3 LightDir,
    float3 Normal,
    float3 Albedo,
    float Metallic,
    float Roughness,
    float3 ViewDir,
    float NdotL)
{
    // H = normalize(L + V) becomes undefined when L and V are almost opposite. Keep the result finite and disable NdotH in that case.
    float3 HalfVectorSum = LightDir + ViewDir;
    float HalfVectorLengthSq = dot(HalfVectorSum, HalfVectorSum);

    float HalfVectorValid = step(1.0e-6, HalfVectorLengthSq);

    float3 HalfVec = HalfVectorSum * rsqrt(max(HalfVectorLengthSq, 1.0e-6));

    float3 F0 = lerp(float3(0.04), Albedo, Metallic);

    float NdotV = max(dot(Normal, ViewDir), 0.0);

    float NdotH = max(dot(Normal, HalfVec), 0.0) * HalfVectorValid;

    // For normalized L and V:
    // dot(V, H) = sqrt((1 + dot(L, V)) / 2).
    float VdotH = sqrt(saturate(0.5 + 0.5 * dot(LightDir, ViewDir)));

    // Fresnel-Schlick approximation.
    float3 Fresnel = F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);

    // GGX / Trowbridge-Reitz normal distribution.
    float Alpha = Roughness * Roughness;

    float Alpha2 = Alpha * Alpha;

    float DistributionDenominator = NdotH * NdotH * (Alpha2 - 1.0) + 1.0;

    float Distribution = Alpha2 / (PBR_PI * DistributionDenominator * DistributionDenominator);

    // Schlick-GGX geometry approximation for direct lighting.
    float K = (Roughness + 1.0) * (Roughness + 1.0) / 8.0;

    float GeometryView = NdotV / (NdotV * (1.0 - K) + K);

    float GeometryLight = NdotL / (NdotL * (1.0 - K) + K);

    float Geometry = GeometryView * GeometryLight;

    float3 Specular = (Distribution * Geometry * Fresnel) / max(4.0 * NdotV * NdotL, 0.001);

    // Metals have no diffuse component in this metallic workflow.
    float3 DiffuseWeight = (1.0 - Fresnel) * (1.0 - Metallic);

    float3 Diffuse = DiffuseWeight * Albedo / PBR_PI;

    return Diffuse + Specular;
}