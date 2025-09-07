#include "common.hlsli"

[shader("miss")]
void Miss(inout HitInfo payload)
{
    float2 uv = CalcSphereUV(WorldRayDirection());
    // TODO: IBLのためのNEE実装
    float3 bgCol = gBgTex.SampleLevel(gSampler, uv, 0).rgb;
    bgCol /= (bgCol + 1.0f);
    bgCol = pow(bgCol, 1.0f / 2.2f);
    payload.color =  bgCol * payload.attenuation;
    payload.pathDepth = gSceneParam.maxPathDepth;
}

[shader("miss")]
void ShadowMiss(inout ShadowRayHitInfo payload)
{
    payload.occluded = false;
}
