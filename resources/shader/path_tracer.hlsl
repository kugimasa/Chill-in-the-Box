RWTexture2D<float4> gOutput : register(u0);
RaytracingAccelerationStructure gSceneBVH : register(t0);

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

[shader("raygeneration")]
void RayGen()
{
    // �y�C���[�h�̏�����
    HitInfo payload;
    payload.color = float4(0.1, 0.2, 0.5, 1.0);
    
    uint2 launchIndex = DispatchRaysIndex();
    
    gOutput[launchIndex] = float4(payload.color.rgb, 1.0);
}

[shader("miss")]
void Miss(inout HitInfo payload)
{
    payload.color = float4(0.4, 0.8, 0.9, -1.0);
}

[shader("closesthit")]
void ClosestHit(inout HitInfo payload, Attributes attrib)
{
    payload.color = float4(1, 1, 0, RayTCurrent());
}