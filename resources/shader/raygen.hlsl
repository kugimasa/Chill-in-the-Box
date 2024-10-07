#include "common.hlsli"

// ローカルルートシグネチャ
RWTexture2D<float4> gOutput : register(u0);

float3 PathTrace(in float3 origin, in float3 direction, in uint seed)
{
    // ペイロードの初期化
    HitInfo payload;
    payload.color = float3(0.0, 0.0, 0.0);
    payload.pathDepth = 0u;
    payload.seed = seed;
    payload.color = 0.0f;
    payload.attenuation = 1.0f;

    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = normalize(direction);
    ray.TMin = RAY_T_MIN;
    ray.TMax = RAY_T_MAX;

    RAY_FLAG flags = RAY_FLAG_NONE;
    flags |= RAY_FLAG_CULL_BACK_FACING_TRIANGLES;
    uint rayMask = 0xFF;
    uint rayIdx = 0;
    uint geoMulVal = 1;
    uint missIdx = 0;

    while (payload.pathDepth < gSceneParam.maxPathDepth)
    {
        float3 attenuation = payload.attenuation;
        // ロシアンルーレット
        float r = Rand(payload.seed);
        float p = min(max(max(attenuation.x, attenuation.y), attenuation.z), 1.0f);
        if (r > p)
        {
            payload.pathDepth = gSceneParam.maxPathDepth;
        }
        payload.attenuation /= p;
        TraceRay(gSceneBVH, flags, rayMask, rayIdx, geoMulVal, missIdx, ray, payload);
        // レイの更新
        payload.pathDepth++;
        ray.Origin = payload.hitPos;
        ray.Direction = payload.reflectDir;
    }
    return payload.color;
}

[shader("raygeneration")]
void RayGen()
{
    uint2 launchIdx = DispatchRaysIndex().xy;
    float2 dims = float2(DispatchRaysDimensions().xy);

    // 乱数の初期化
    uint bufferOffset = launchIdx.x + launchIdx.y * dims.x;
    uint seed = bufferOffset * (gSceneParam.currenFrameNum + 1);
    
    float3 col = 0;
    // パストレース
    for (uint i = 0; i < gSceneParam.maxSPP; ++i)
    {
        // レイの初期化
        float2 screenUV = float2(launchIdx) + float2(Rand(seed), Rand(seed));
        float2 d = (screenUV.xy + 0.5) / dims.xy * 2.0 - 1.0;
        matrix invViewMtx = gSceneParam.invViewMtx;
        matrix invProjMtx = gSceneParam.invProjMtx;
        float3 origin = mul(invViewMtx, float4(0, 0, 0, 1)).xyz;
        float3 target = mul(invProjMtx, float4(d.x, -d.y, 1, 1)).xyz;
        float3 direction = mul(invViewMtx, float4(target, 0)).xyz;
        col += max(PathTrace(origin, direction, seed), 0);
    }
    col /= float(gSceneParam.maxSPP);

    gOutput[launchIdx] = float4(pow(col, 2.2f), 1.0);
}
