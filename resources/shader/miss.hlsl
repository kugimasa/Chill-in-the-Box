#include "common.hlsli"

[shader("miss")]
void Miss(inout HitInfo payload)
{
    float2 uv = CalcSphereUV(WorldRayDirection());
    // HDR�ł��邱�Ƃ��l�����������ǂ�����
    payload.color = gBgTex.SampleLevel(gSampler, uv, 0).rgb;
    payload.pathDepth = gSceneParam.maxPathDepth;
}