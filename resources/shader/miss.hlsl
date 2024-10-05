#include "common.hlsli"

[shader("miss")]
void Miss(inout HitInfo payload)
{
    payload.color = float3(0.2, 0.2, 0.2);
    payload.pathDepth = gSceneParam.maxPathDepth;
}