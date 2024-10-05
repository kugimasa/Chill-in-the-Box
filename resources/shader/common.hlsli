// �y�C���[�h
struct HitInfo
{
    float3 hitPos;
    float3 reflectDir;
    float3 color;
    uint pathDepth;
};

// ���C�q�b�g���̃A�g���r���[�g
struct Attributes
{
    float2 bary;
};

// �V�[���p�����[�^�[
struct SceneCB {
    matrix viewMtx;    // �r���[�s��
    matrix projMtx;    // �v���W�F�N�V�����s��
    matrix invViewMtx; // �r���[�t�s��
    matrix invProjMtx; // �v���W�F�N�V�����t�s��
    uint frameIndex;   // �`�撆�̃t���[���C���f�b�N�X
    uint maxPathDepth; // �ő唽�ˉ�
};

// �O���[�o�����[�g�V�O�l�`��
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

