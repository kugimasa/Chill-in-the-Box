#include "common.hlsli"

[shader("miss")]
void Miss(inout HitInfo payload)
{
    float2 uv = CalcSphereUV(WorldRayDirection());
    // TODO: IBL‚Ì‚½‚ß‚ÌNEEŽÀ‘•
    payload.color = gBgTex.SampleLevel(gSampler, uv, 0).rgb * payload.attenuation;
    payload.pathDepth = gSceneParam.maxPathDepth;
}

[shader("miss")]
void ShadowMiss(inout ShadowRayHitInfo payload)
{
    payload.occluded = false;
}
