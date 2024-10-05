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
    
    // TODO: テスト用ライト判定(ゆくゆくはMeshParamCBから取得)
    uint instanceID = InstanceID();
    if (instanceID == 1)
    {
        // 光源にヒット、トレースを終了
        payload.color += float3(1, 1, 1);
        payload.pathDepth = gSceneParam.maxPathDepth;
    }
    else
    {
        float3 sampleDir = SampleHemisphereCos(payload.seed);
        float3 reflectDir = normalize(ApplyZToN(sampleDir, worldNorm));
        payload.reflectDir = reflectDir;
        float3 reflectance = GetAlbedo(vtx.texcoord) * SampleBrdf(reflectDir, worldNorm);
        payload.attenuation = reflectance / HemispherCosPdf(reflectDir, worldNorm);
    }
}