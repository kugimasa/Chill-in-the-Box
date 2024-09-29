// ペイロード
struct HitInfo
{
    float4 color;
    int rayDepth;
};

// レイヒット時のアトリビュート
struct Attributes
{
    float2 bary;
};

// シーンパラメーター
struct SceneCB {
    matrix viewMtx; // ビュー行列
    matrix projMtx; // プロジェクション行列
    matrix invViewMtx; // ビュー逆行列
    matrix invProjMtx; // プロジェクション逆行列
    uint frameIndex;
};

// グローバルルートシグネチャ
RaytracingAccelerationStructure gSceneBVH : register(t0);
ConstantBuffer<SceneCB> gSceneParam : register(b0);

// TODO: コンパイル時定数として扱いたい
//const int kMaxRayDepth = 15;
//const float4 kBlackColor = float4(0, 0, 0, 1);
//const float4 kWhiteColor = float4(1, 1, 1, 1);

// 反射上限のチェック
inline bool IsMaxRayDepth(inout HitInfo payload)
{
    payload.rayDepth++;
    if (payload.rayDepth >= 15)
    {
        payload.color = float4(0, 0, 0, 1);
        return true;
    }
    return false;
}

inline float3 CalcHitAttrib(float3 vtxAttr[3], float2 bary)
{
    float3 ret;
    ret = vtxAttr[0];
    ret += bary.x * (vtxAttr[1] - vtxAttr[0]);
    ret += bary.y * (vtxAttr[2] - vtxAttr[0]);
    return ret;
}

