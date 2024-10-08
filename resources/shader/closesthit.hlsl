#include "common.hlsli"

struct VertexAttrib
{
    float3 position;
    float3 normal;
    float2 texcoord;
};

struct MeshParam
{
    float4 diffuse;
    int matrixBuffStride;
    int meshGroupIndex;
};

// ローカルルートシグネチャ
StructuredBuffer<uint> m_pIndexBuffer : register(t0, space1);
StructuredBuffer<float3> m_vertexAttribPosition : register(t1, space1);
StructuredBuffer<float3> m_vertexAtrribNormal : register(t2, space1);
StructuredBuffer<float2> m_vertexAtrribTexcoord : register(t3, space1);
StructuredBuffer<float4> m_pBLASMatrices : register(t4, space1);

Texture2D<float4> m_textures : register(t0, space2);
ConstantBuffer<MeshParam> m_pMeshParamCB : register(b0, space2);


float4x4 GetBLASMtx4x4()
{
    int mtxRange = m_pMeshParamCB.matrixBuffStride;
    int frameOffset = mtxRange * gSceneParam.frameIndex;
    int index = m_pMeshParamCB.meshGroupIndex * 3;
    index += frameOffset;
    
    float4 m0 = m_pBLASMatrices[index + 0];
    float4 m1 = m_pBLASMatrices[index + 1];
    float4 m2 = m_pBLASMatrices[index + 2];
    
    float4x4 mtx;
    mtx[0] = m0;
    mtx[1] = m1;
    mtx[2] = m2;
    mtx[3] = float4(0, 0, 0, 1);
    return transpose(mtx);
}

float4x4 GetTLASMtx4x4()
{
    float4x4 tlasMtx;
    float4x3 m = ObjectToWorld4x3();
    tlasMtx[0] = float4(m[0], 0);
    tlasMtx[1] = float4(m[1], 0);
    tlasMtx[2] = float4(m[2], 0);
    tlasMtx[3] = float4(m[3], 1);
    return tlasMtx;
}

float3 GetAlbedo(float2 uv)
{
    float3 diffuse = m_pMeshParamCB.diffuse.rgb;
    diffuse *= m_textures.SampleLevel(gSampler, uv, 0).rgb;
    return diffuse;
}

VertexAttrib GetHitVertexAttrib(Attributes attrib)
{
    VertexAttrib v = (VertexAttrib) 0;
    uint idxStart = PrimitiveIndex() * 3;
    
    float3 pos[3];
    float3 norm[3];
    float3 texcoords[3];
    for (int i = 0; i < 3; ++i)
    {
        uint idx = m_pIndexBuffer[idxStart + i];
        pos[i] = m_vertexAttribPosition[idx];
        norm[i] = m_vertexAtrribNormal[idx];
        texcoords[i] = float3(m_vertexAtrribTexcoord[idx], 1);
    }

    v.position = CalcHitAttrib(pos, attrib.bary);
    v.normal = normalize(CalcHitAttrib(norm, attrib.bary));
    v.texcoord = CalcHitAttrib(texcoords, attrib.bary).xy;
    return v;
}

bool TraceShadowRay(in float3 origin, in float3 direction, in float lightDist, in uint seed)
{
    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = normalize(direction);
    ray.TMin = 0.00001f;
    ray.TMax = lightDist - 0.00001f;
    
    RAY_FLAG flags = RAY_FLAG_NONE;
    flags |= RAY_FLAG_SKIP_CLOSEST_HIT_SHADER;
    uint rayMask = ~(0x08); // ライトは除外
    uint rayIdx = 0;
    uint geoMulVal = 1;
    uint missIdx = 1;
    ShadowRayHitInfo payload;
    payload.occluded = true;
    TraceRay(gSceneBVH, flags, rayMask, rayIdx, geoMulVal, missIdx, ray, payload);
    return payload.occluded;
}

[shader("closesthit")]
void ClosestHit(inout HitInfo payload, Attributes attrib)
{
    VertexAttrib vtx = GetHitVertexAttrib(attrib);
    
    float4x4 blasMtx = GetBLASMtx4x4();
    float4x4 tlasMtx = GetTLASMtx4x4();
    float4x4 worldMtx = mul(blasMtx, tlasMtx);
    
    float3 worldPos = mul(float4(vtx.position, 1), worldMtx).xyz;
    float3 worldNorm = normalize(mul(vtx.normal, (float3x3) worldMtx));
    payload.hitPos = worldPos;
    
    uint instanceID = InstanceID();
    // TODO: ゆくゆくはMeshParamCBから取得
    // 光源にヒットした場合はトレースを終了
    if (instanceID == 1)
    {
        if (payload.pathDepth == 0)
        {
            // TODO: 光源の表現をシェーダー芸するならここ
            // カメラ方向が必要かも
            payload.color = gSceneParam.light1.color;
        }
        payload.pathDepth = gSceneParam.maxPathDepth;
        return;
    }
    else if (instanceID == 2)
    {
        if (payload.pathDepth == 0)
        {
            payload.color = gSceneParam.light2.color;
        }
        payload.pathDepth = gSceneParam.maxPathDepth;
        return;
    }
    else if (instanceID == 3)
    {
        if (payload.pathDepth == 0)
        {
            payload.color = gSceneParam.light3.color;
        }
        payload.pathDepth = gSceneParam.maxPathDepth;
        return;
    }
    // 光源サンプリング
    SampledLightInfo lightInfo = SampleLightInfo(payload.seed);
    float3 lightDir = normalize(lightInfo.pos - worldPos);
    float lightDist = length(lightInfo.pos - worldPos);
    // 光源方向へレイトレースして、光源と接続できた場合に寄与の計算
    if (!TraceShadowRay(worldPos, lightDir, lightDist, payload.seed))
    {
        // 幾何項の計算
        float cos1 = abs(dot(worldNorm, lightDir));
        float cos2 = abs(dot(lightInfo.norm, -lightDir));
        float G = (cos1 * cos2) / (lightDist * lightDist);
        float3 wi = normalize(ApplyZToN(-WorldRayDirection(), worldNorm));
        float3 wo = normalize(ApplyZToN(lightDir, worldNorm));
        payload.color += (payload.attenuation * CalcCos(wi, wo) * G / LightSamplingPdf(lightInfo.radius)) * lightInfo.intensity;
    }
    // 方向をサンプリング
    float3 sampleDir = SampleHemisphereCos(payload.seed);
    float3 reflectDir = normalize(ApplyZToN(sampleDir, worldNorm));
    payload.reflectDir = reflectDir;
    float3 reflectance = GetAlbedo(vtx.texcoord) * CalcCos(worldNorm, reflectDir);
    payload.attenuation *= (reflectance / HemisphereCosPdf(worldNorm, reflectDir));
}