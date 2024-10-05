// ペイロード
struct HitInfo
{
    float3 hitPos;
    float3 reflectDir;
    float3 color;
    uint pathDepth;
};

// レイヒット時のアトリビュート
struct Attributes
{
    float2 bary;
};

// シーンパラメーター
struct SceneCB {
    matrix viewMtx;    // ビュー行列
    matrix projMtx;    // プロジェクション行列
    matrix invViewMtx; // ビュー逆行列
    matrix invProjMtx; // プロジェクション逆行列
    uint frameIndex;   // 描画中のフレームインデックス
    uint maxPathDepth; // 最大反射回数
};

// グローバルルートシグネチャ
RaytracingAccelerationStructure gSceneBVH : register(t0);
ConstantBuffer<SceneCB> gSceneParam : register(b0);
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

