// �y�C���[�h
struct HitInfo
{
    float3 hitPos;
    float3 reflectDir;
    float3 color;
    float3 attenuation;
    uint pathDepth;
    uint seed;
};

// ���C�q�b�g���̃A�g���r���[�g
struct Attributes
{
    float2 bary;
};

// �V�[���p�����[�^�[
struct SceneParam {
    matrix viewMtx;      // �r���[�s��
    matrix projMtx;      // �v���W�F�N�V�����s��
    matrix invViewMtx;   // �r���[�t�s��
    matrix invProjMtx;   // �v���W�F�N�V�����t�s��
    uint frameIndex;     // �`�撆�̃t���[���C���f�b�N�X
    uint currenFrameNum; // ���݂̃t���[��
    uint maxPathDepth;   // �ő唽�ˉ�
    uint maxSPP;         // Sample Per Pixel
};

// �O���[�o�����[�g�V�O�l�`��
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

// (c) 2014 M.E. O'Neill / pcg-random.org
// Licensed under Apache License 2.0
inline uint PCGRandom(inout uint seed)
{
    uint oldstate = seed;
    seed = oldstate * 6364136223846793005u + 3u;
    uint xorshifted = uint((oldstate >> 18u) ^ oldstate >> 27u);
    uint rot = oldstate >> 59u;
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31u));
}

inline void InitPCG(inout uint seed)
{
    uint initState = 0u;
    PCGRandom(initState);
    seed += seed;
    PCGRandom(seed);
}

inline float Rand(inout uint seed)
{
    return float(PCGRandom(seed)) / 4294967296.0;
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

// TODO: �}�e���A���̎���
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
