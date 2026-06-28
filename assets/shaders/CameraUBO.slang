struct CameraUBO
{
    float4x4    ViewProj;
    float4x4    InverseViewProj;
    float4      CameraPos;
};