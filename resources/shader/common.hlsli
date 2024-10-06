// ペイロード
struct HitInfo
{
    float3 hitPos;
    float3 reflectDir;
    float3 color;
    float3 attenuation;
    uint pathDepth;
    uint seed;
};

// レイヒット時のアトリビュート
struct Attributes
{
    float2 bary;
};

// シーンパラメーター
struct SceneParam {
    matrix viewMtx;      // ビュー行列
    matrix projMtx;      // プロジェクション行列
    matrix invViewMtx;   // ビュー逆行列
    matrix invProjMtx;   // プロジェクション逆行列
    uint frameIndex;     // 描画中のフレームインデックス
    uint currenFrameNum; // 現在のフレーム
    uint maxPathDepth;   // 最大反射回数
    uint maxSPP;         // Sample Per Pixel
};

// グローバルルートシグネチャ
RaytracingAccelerationStructure gSceneBVH : register(t0);
Texture2D<float4> gBgTex : register(t1);
ConstantBuffer<SceneParam> gSceneParam : register(b0);
SamplerState gSampler : register(s0);

#define PI 3.14159265359
#define INV_PI 0.318309886184

inline float3 CalcHitAttrib(float3 vtxAttr[3], float2 bary)
{
    float3 ret;
    ret = vtxAttr[0];
    ret += bary.x * (vtxAttr[1] - vtxAttr[0]);
    ret += bary.y * (vtxAttr[2] - vtxAttr[0]);
    return ret;
}

inline float2 CalcSphereUV(float3 dir)
{
    dir = normalize(dir);
    float theta = atan2(dir.z, dir.x);
    float phi = acos(dir.y);
    float u = (theta + PI) * INV_PI * 0.5;
    float v = phi * INV_PI;
    return float2(u, v);
}

// https://en.wikipedia.org/wiki/Xorshift
inline float Rand(inout uint seed)
{
    uint rnd = seed;
    rnd ^= rnd << 13;
    rnd ^= rnd >> 7;
    rnd ^= rnd << 5;
    seed = rnd;
    return (float) (rnd & 0x00FFFFFF) / (float) 0x01000000;
}

inline float3 SampleHemisphereCos(in uint seed)
{
    float r1 = Rand(seed);
    float r2 = Rand(seed);
    float phi = 2.0 * PI * r1;
    float x = cos(phi) * sqrt(r2);
    float y = sin(phi) * sqrt(r2);
    float z = sqrt(max(0.0, 1.0 - r2));
    return float3(x, y, z);
}

inline float3 ApplyZToN(float3 dir, float3 norm)
{
    float3 up = abs(norm.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 tangent = normalize(cross(up, norm));
    float3 bitangent = cross(norm, tangent);
    return dir.x * tangent + dir.y * bitangent + dir.z * norm;
}

// TODO: マテリアルの実装
inline float SampleBrdf(float3 dir, float3 norm)
{
    float cos = dot(normalize(dir), norm);
    return (cos < 0.0) ? 0 : INV_PI * cos;
}

inline float HemispherCosPdf(float3 dir, float3 norm)
{
    float cos = dot(normalize(dir), norm);
    return (cos <= 0.0) ? 0 : INV_PI * cos;
}
