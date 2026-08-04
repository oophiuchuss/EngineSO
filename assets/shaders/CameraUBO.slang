struct CameraUBO
{
    float4x4    ViewProj;
    float4x4    InverseViewProj;
    float4x4    PreviousViewProj;
    float4      CameraPos;
    float4      PreviousCameraPos;
    
    // xy = current jitter UV
    // zw = previous submitted jitter UV
    float4 TemporalJitterUV;
};
