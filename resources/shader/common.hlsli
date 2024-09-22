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

// グローバルルートシグネチャ
RaytracingAccelerationStructure gSceneBVH : register(t0);