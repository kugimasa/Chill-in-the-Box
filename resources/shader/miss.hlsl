#include "common.hlsli"

[shader("miss")]
void Miss(inout HitInfo payload)
{
    float2 uv = CalcSphereUV(WorldRayDirection());
    // HDRであることを考慮した方が良さそう
    payload.color = gBgTex.SampleLevel(gSampler, uv, 0).rgb;
    payload.pathDepth = gSceneParam.maxPathDepth;
}