RWTexture2D<float4> gOutput : register(u0);
RaytracingAccelerationStructure gSceneBVH : register(t0);

// ペイロード
struct HitInfo
{
    float4 color;
};

// レイヒット時のアトリビュート
struct Attributes
{
    float2 bary;
};

[shader("raygeneration")]
void RayGen()
{
    // ペイロードの初期化
    HitInfo payload;
    payload.color = float4(0.1, 0.2, 0.5, 1.0);
    
    uint2 launchIndex = DispatchRaysIndex();
    
    gOutput[launchIndex] = float4(payload.color.rgb, 1.0);
}

[shader("miss")]
void Miss(inout HitInfo payload)
{
    payload.color = float4(0.4, 0.8, 0.9, -1.0);
}

[shader("closesthit")]
void ClosestHit(inout HitInfo payload, Attributes attrib)
{
    payload.color = float4(1, 1, 0, RayTCurrent());
}