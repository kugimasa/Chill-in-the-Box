// �y�C���[�h
struct HitInfo
{
    float4 color;
    int rayDepth;
};

// ���C�q�b�g���̃A�g���r���[�g
struct Attributes
{
    float2 bary;
};

// �V�[���p�����[�^�[
struct SceneCB {
    matrix viewMtx; // �r���[�s��
    matrix projMtx; // �v���W�F�N�V�����s��
    matrix invViewMtx; // �r���[�t�s��
    matrix invProjMtx; // �v���W�F�N�V�����t�s��
    uint frameIndex;
};

// �O���[�o�����[�g�V�O�l�`��
RaytracingAccelerationStructure gSceneBVH : register(t0);
ConstantBuffer<SceneCB> gSceneParam : register(b0);

// TODO: �R���p�C�����萔�Ƃ��Ĉ�������
//const int kMaxRayDepth = 15;
//const float4 kBlackColor = float4(0, 0, 0, 1);
//const float4 kWhiteColor = float4(1, 1, 1, 1);

// ���ˏ���̃`�F�b�N
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

