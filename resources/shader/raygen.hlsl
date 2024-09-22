#include "common.hlsli"

// ローカルルートシグネチャ
RWTexture2D<float4> gOutput : register(u0);

[shader("raygeneration")]
void RayGen()
{
    
    uint2 launchIndex = DispatchRaysIndex().xy;
    float2 dims = float2(DispatchRaysDimensions().xy);
    
    // ペイロードの初期化
    HitInfo payload;
    payload.color = float4(0.1, 0.2, 0.5, 1.0);
    
    // レイの初期化
    float2 d = (launchIndex.xy + 0.5) / dims.xy * 2.0 - 1.0;
    RayDesc rayDesc;
    rayDesc.Origin = float3(d.x, -d.y, 1);
    rayDesc.Direction = float3(0, 0, -1);
    rayDesc.TMin = 0;
    rayDesc.TMax = 10000;
    
    RAY_FLAG flags = RAY_FLAG_NONE;
    uint rayMask = 0xFF;
    
    // レイトレース
    TraceRay(
        gSceneBVH,
        flags,
        rayMask,
        0, // Ray Index
        1, // MultiplierForGeometryContrib
        0, // Miss Index
        rayDesc,
        payload
    );
    float3 col = payload.color.rgb;

    gOutput[launchIndex] = float4(col, 1.0);
}
