#include "common.hlsli"

[shader("closesthit")]
void ClosestHit(inout HitInfo payload, Attributes attrib)
{
    payload.color = float4(1, 1, 0, RayTCurrent());
    float3 col = 0;
    col.xy = attrib.bary;
    col.z = 1.0 - col.x - col.y;
    payload.color = float4(col, RayTCurrent());
}