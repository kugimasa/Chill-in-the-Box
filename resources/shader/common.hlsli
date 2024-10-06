// パストレース用ペイロード
struct HitInfo
{
    float3 hitPos;
    float3 reflectDir;
    float3 color;
    float3 attenuation;
    uint pathDepth;
    uint seed;
};

// シャドウレイ用ペイロード
struct ShadowRayHitInfo
{
    bool occluded;
};

// レイヒット時のアトリビュート
struct Attributes
{
    float2 bary;
};

// ライト用のパラメータ
struct SphereLightParam
{
    float3 center;
    float radius;
    float3 color;
    float intensity;
};

// シーンパラメーター
struct SceneParam
{
    matrix viewMtx;      // ビュー行列
    matrix projMtx;      // プロジェクション行列
    matrix invViewMtx;   // ビュー逆行列
    matrix invProjMtx;   // プロジェクション逆行列
    uint frameIndex;     // 描画中のフレームインデックス
    uint currenFrameNum; // 現在のフレーム
    uint maxPathDepth;   // 最大反射回数
    uint maxSPP;         // Sample Per Pixel
    SphereLightParam light1; // Light1のパラメータ
    SphereLightParam light2; // Light2のパラメータ
    SphereLightParam light3; // Light3のパラメータ
};

// サンプリングされたライトの情報
struct SampledLightInfo
{
    float3 pos;
    float3 norm;
    float3 intensity;
};

// グローバルルートシグネチャ
RaytracingAccelerationStructure gSceneBVH : register(t0);
Texture2D<float4> gBgTex : register(t1);
ConstantBuffer<SceneParam> gSceneParam : register(b0);
SamplerState gSampler : register(s0);

#define PI 3.14159265359
#define INV_PI 0.318309886184
#define RAY_T_MIN 0
#define RAY_T_MAX 10000

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

inline float3 ApplyZToN(float3 dir, float3 norm)
{
    float3 up = abs(norm.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 tangent = normalize(cross(up, norm));
    float3 bitangent = cross(norm, tangent);
    return dir.x * tangent + dir.y * bitangent + dir.z * norm;
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

inline float3 SampleSphere(in uint seed, float3 center, float radius)
{
    float r1 = Rand(seed);
    float r2 = Rand(seed);
    float theta = 2.0 * PI * r1;
    float phi = acos(1.0 - 2.0 * r2);
    float x = sin(phi) * cos(theta);
    float y = sin(phi) * sin(theta);
    float z = cos(phi);
    return center + radius * float3(x, y, z);
}

inline SampledLightInfo SampleLightInfo(in uint seed)
{
    float r = Rand(seed);
    SampledLightInfo lightInfo;
    if (0 <= r && r < 0.3333333)
    {
        lightInfo.pos = SampleSphere(seed, gSceneParam.light1.center, gSceneParam.light1.radius);
        lightInfo.norm = normalize(lightInfo.pos - gSceneParam.light1.center);
        lightInfo.intensity = gSceneParam.light1.color * gSceneParam.light1.intensity;
    }
    else if (0.3333333 <= r && r < 0.6666666)
    {
        lightInfo.pos = SampleSphere(seed, gSceneParam.light2.center, gSceneParam.light2.radius);
        lightInfo.norm = normalize(lightInfo.pos - gSceneParam.light2.center);
        lightInfo.intensity = gSceneParam.light2.color * gSceneParam.light2.intensity;
    }
    else
    {
        lightInfo.pos = SampleSphere(seed, gSceneParam.light3.center, gSceneParam.light3.radius);
        lightInfo.norm = normalize(lightInfo.pos - gSceneParam.light3.center);
        lightInfo.intensity = gSceneParam.light3.color * gSceneParam.light3.intensity;
    }
    return lightInfo;
}

// TODO: マテリアルの実装
inline float CalcCos(float3 inDir, float3 outDir)
{
    float cos = dot(inDir, outDir);
    return (cos < 0.0) ? 0 : INV_PI * cos;
}

inline float HemisphereCosPdf(float3 dir, float3 norm)
{
    float cos = dot(normalize(dir), norm);
    return (cos <= 0.0) ? 1 : INV_PI * cos;
}

inline float AreaSpherePdf(float radius)
{
    return INV_PI / (4 * radius * radius);
}

inline float LightSamplingPdf()
{
    float light1Pdf = AreaSpherePdf(gSceneParam.light1.radius);
    float light2Pdf = AreaSpherePdf(gSceneParam.light2.radius);
    float light3Pdf = AreaSpherePdf(gSceneParam.light3.radius);
    return (light1Pdf + light2Pdf + light3Pdf) / 3.0f;
}
