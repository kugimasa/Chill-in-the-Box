#include "common.hlsli"

[shader("miss")]
void Miss(inout HitInfo payload)
{
    float2 uv = CalcSphereUV(WorldRayDirection());
    // HDR‚Å‚ ‚é‚±‚Æ‚ğl—¶‚µ‚½•û‚ª—Ç‚³‚»‚¤
    payload.color = gBgTex.SampleLevel(gSampler, uv, 0).rgb;
    payload.pathDepth = gSceneParam.maxPathDepth;
}