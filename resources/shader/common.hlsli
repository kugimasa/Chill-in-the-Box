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

// �O���[�o�����[�g�V�O�l�`��
RaytracingAccelerationStructure gSceneBVH : register(t0);