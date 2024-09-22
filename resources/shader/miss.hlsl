#include "common.hlsli"

[shader("miss")]
void Miss(inout HitInfo payload)
{
    payload.color = float4(0.2, 0.2, 0.2, -1.0);
}