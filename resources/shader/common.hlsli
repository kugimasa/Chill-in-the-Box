// ペイロード
struct HitInfo
{
    float4 color;
};

// レイヒット時のアトリビュート
struct Attributes
{
    float2 bary;
};

// シーンパラメーター
struct SceneCB{
    matrix viewMtx; // ビュー行列.
    matrix projMtx; // プロジェクション行列.
    matrix invViewMtx; // ビュー逆行列.
    matrix invProjMtx; // プロジェクション逆行列.
};

// グローバルルートシグネチャ
RaytracingAccelerationStructure gSceneBVH : register(t0);
ConstantBuffer<SceneCB> gSceneParam : register(b0);