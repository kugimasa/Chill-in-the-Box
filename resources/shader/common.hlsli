// �y�C���[�h
struct HitInfo
{
    float4 color;
};

// ���C�q�b�g���̃A�g���r���[�g
struct Attributes
{
    float2 bary;
};

// �V�[���p�����[�^�[
struct SceneCB{
    matrix viewMtx; // �r���[�s��.
    matrix projMtx; // �v���W�F�N�V�����s��.
    matrix invViewMtx; // �r���[�t�s��.
    matrix invProjMtx; // �v���W�F�N�V�����t�s��.
};

// �O���[�o�����[�g�V�O�l�`��
RaytracingAccelerationStructure gSceneBVH : register(t0);
ConstantBuffer<SceneCB> gSceneParam : register(b0);