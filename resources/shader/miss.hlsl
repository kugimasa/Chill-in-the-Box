#include "common.hlsli"

[shader("miss")]
void Miss(inout HitInfo payload)
{
    float2 uv = CalcSphereUV(WorldRayDirection());
    payload.color = gBgTex.SampleLevel(gSampler, uv, 0).rgb;
    payload.pathDepth = gSceneParam.maxPathDepth;
}