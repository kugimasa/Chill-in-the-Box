#include "scene/actor.hpp"
#include "utils/dxr_util.h"

Actor::ActorNode::ActorNode()
{
    m_trans = ZeroVector();
    m_rot = IdentityQuat();
    m_scale = ZeroVector();
    m_localMtx = IdentityMtx();
    m_worldMtx = IdentityMtx();
}

Actor::ActorNode::~ActorNode()
{
    m_children.clear();
}

void Actor::ActorNode::UpdateLocalMatrix()
{
    auto transMtx = XMMatrixTranslationFromVector(m_trans);
    auto rotMtx = XMMatrixRotationQuaternion(m_rot);
    auto scaleMtx = XMMatrixScalingFromVector(m_scale);
    m_localMtx = scaleMtx * rotMtx * transMtx;
}

void Actor::ActorNode::UpdateWorldMatrix(Matrix parentMtx)
{
    m_worldMtx = m_localMtx * parentMtx;
}

void Actor::ActorNode::UpdateMatrix(Matrix parentMtx)
{
    UpdateLocalMatrix();
    UpdateWorldMatrix(parentMtx);
    for (auto& child : m_children)
    {
        child->UpdateMatrix(m_worldMtx);
    }
}

Actor::ActorMaterial::ActorMaterial(std::unique_ptr<Device>& device, const Model::Material& srcMat) :
    m_pDevice(device)
{
    m_name = srcMat.GetName();
    auto diffuse = srcMat.GetDiffuseColor();
    m_materialParam.diffuse.x = diffuse.x;
    m_materialParam.diffuse.y = diffuse.y;
    m_materialParam.diffuse.z = diffuse.z;
    m_materialParam.diffuse.w = 1.0f;

    // バックバッファ枚数分作成
    auto cbSize = UINT(ROUND_UP(sizeof(MaterialParam), 256));
    if (device->CreateConstantBuffer(m_pMatrialCB, cbSize, L"MaterialCB"))
    {
        auto backBufferCount = device->BackBufferCount;
        m_cbv.resize(backBufferCount);
        for (size_t i = 0; i < backBufferCount; i++)
        {
            auto matCB = m_pMatrialCB[i];
            device->WriteBuffer(matCB, &m_materialParam, cbSize);
            D3D12_CONSTANT_BUFFER_VIEW_DESC cbDesc{};
            cbDesc.BufferLocation = matCB->GetGPUVirtualAddress();
            cbDesc.SizeInBytes = cbSize;
            m_cbv[i] = device->AllocateDescriptorHeap();
            device->GetDevice()->CreateConstantBufferView(&cbDesc, m_cbv[i].cpuHandle);
        }
    }

}

Actor::ActorMaterial::~ActorMaterial()
{
    if (m_pDevice)
    {
        for (auto& cbv : m_cbv)
        {
            m_pDevice->DeallocateDescriptorHeap(cbv);
        }
    }
}

void Actor::ActorMaterial::SetTexture(TextureResource& texRes)
{
    m_texture = texRes;
}

Actor::Actor(std::unique_ptr<Device>& device, const Model* model) :
    m_pDevice(device),
    m_modelRef(model),
    m_worldMtx(IdentityMtx()),
    m_worldPos(Float3(0, 0, 0))
{
}

Actor::~Actor()
{
    m_nodes.clear();
}

/// <summary>
/// 回転処理
/// </summary>
void Actor::Rotate(float deltaTime, float speed, Float3 up)
{
    float theta = deltaTime * speed * XM_2PI;
    auto rotMtx = XMMatrixRotationAxis(XMLoadFloat3(&up), theta);
    auto transMtx = XMMatrixTranslation(m_worldPos.x, m_worldPos.y, m_worldPos.z);
    m_worldMtx = rotMtx * transMtx;
}

void Actor::MoveAnimInCubic(float currentTime, float startTime, float endTime, Float3 startPos, Float3 endPos)
{
    float t = (currentTime - startTime) / (endTime - startTime);
    float s = EaseInCubic(t);
    Float3 pos = Lerp(startPos, endPos, t);
    SetWorldPos(pos);
}

void Actor::SetRotaion(float degree, Float3 up)
{
    float radian = XMConvertToRadians(degree);
    auto rotMtx = XMMatrixRotationAxis(XMLoadFloat3(&up), radian);
    auto transMtx = XMMatrixTranslation(m_worldPos.x, m_worldPos.y, m_worldPos.z);
    m_worldMtx = rotMtx * transMtx;
}

void Actor::SetWorldPos(Float3 worldPos)
{
    m_worldPos = worldPos;
    auto transMtx = XMMatrixTranslation(m_worldPos.x, m_worldPos.y, m_worldPos.z);
    m_worldMtx = transMtx;
}

void Actor::SetMaterialHitGroup(const std::wstring& hitGroupName)
{
    for (auto& material : m_materials)
    {
        material->SetHitGroupStr(hitGroupName.c_str());
    }
}

void Actor::UpdateMatrices()
{
    for (auto& node : m_nodes)
    {
        node->UpdateMatrix(m_worldMtx);
    }
}

// BLASの更新
// コマンドを積むまで
void Actor::UpdateBLAS(ComPtr<ID3D12GraphicsCommandList4> cmdList)
{
    std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> rtGeoDesc;
    CreateRTGeoDesc(rtGeoDesc);

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildASDesc{};
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& inputs = buildASDesc.Inputs;
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.NumDescs = UINT(rtGeoDesc.size());
    inputs.pGeometryDescs = rtGeoDesc.data();
    // BLAS更新のためフラグを設定
    inputs.Flags =
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE |
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;

    auto frameIndex = m_pDevice->GetCurrentFrameIndex();
    // BLASを直接更新
    auto updateBuffer = m_pBLASUpdateBuffer;
    buildASDesc.DestAccelerationStructureData = m_pBLAS->GetGPUVirtualAddress();
    buildASDesc.SourceAccelerationStructureData = m_pBLAS->GetGPUVirtualAddress();
    buildASDesc.ScratchAccelerationStructureData = updateBuffer->GetGPUVirtualAddress();

    // BLAS再構築
    cmdList->BuildRaytracingAccelerationStructure(&buildASDesc, 0, nullptr);
    auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(m_pBLAS.Get());
    cmdList->ResourceBarrier(1, &barrier);
}

void Actor::UpdateTransform()
{
    auto frameIndex = m_pDevice->GetCurrentFrameIndex();

    std::vector<Mtx3x4> blasMatrices;
    auto groupCount = UINT(m_meshGroups.size());
    blasMatrices.resize(groupCount);
    // TLASで設定した行列成分の打消し
    for (UINT i = 0; i < groupCount; ++i)
    {
        auto node = m_meshGroups[i].m_node;
        auto invRoot = XMMatrixInverse(nullptr, m_worldMtx);
        XMStoreFloat3x4(&blasMatrices[i], node->GetWorldMatrix() * invRoot);
    }

    auto bufferBytes = UINT(sizeof(Mtx3x4) * groupCount);
    void* mapped = nullptr;
    D3D12_RANGE range;
    range.Begin = bufferBytes * frameIndex;
    range.End = range.Begin + bufferBytes;
    m_pBLASMatrices->Map(0, &range, &mapped);
    if (mapped)
    {
        auto p = static_cast<uint8_t*>(mapped) + range.Begin;
        memcpy(p, blasMatrices.data(), bufferBytes);
        m_pBLASMatrices->Unmap(0, & range);
    }
}

uint8_t* Actor::WriteHitGroupShaderRecord(uint8_t* dst, UINT hitGroupRecordSize, ComPtr<ID3D12StateObjectProperties> rtStateObjectProps)
{
    for (UINT group = 0; group < GetMeshGroupCount(); ++group)
    {
        for (UINT meshIdx = 0; meshIdx < GetMeshCount(group); ++meshIdx)
        {
            const auto& mesh = GetMesh(group, meshIdx);
            auto material = mesh.GetMaterial();
            auto shader = material->GetHitGroupStr();
            auto id = rtStateObjectProps->GetShaderIdentifier(shader.c_str());
            if (id == nullptr)
            {
                Error(PrintInfoType::RTCAMP10, L"ShaderIdが設定されていません");
            }
            // MEMO: ローカルルートシグネチャの順番と合わせる
            auto recordStart = dst;
            dst += WriteShaderId(dst, id);
            dst += WriteGPUDescriptorHeap(dst, mesh.GetIndexBuffer());
            dst += WriteGPUDescriptorHeap(dst, mesh.GetPosition());
            dst += WriteGPUDescriptorHeap(dst, mesh.GetNormal());
            dst += WriteGPUDescriptorHeap(dst, mesh.GetTexcoord());
            dst += WriteGPUDescriptorHeap(dst, GetBLASMatrixDescriptor());
            dst += WriteGPUDescriptorHeap(dst, material->GetTextureDescriptor());
            dst += WriteGPUResourceAddress(dst, mesh.GetMeshParamCB());
            dst = recordStart + hitGroupRecordSize;
        }
    }
    return dst;
}

UINT Actor::GetMeshGroupCount() const
{
    return UINT(m_meshGroups.size());
}

UINT Actor::GetMeshCount(int groupIndex) const
{
    return UINT(m_meshGroups[groupIndex].m_meshes.size());
}

const Actor::ActorMesh& Actor::GetMesh(int groupIndex, int meshIndex) const
{
    return m_meshGroups[groupIndex].m_meshes[meshIndex];
}

UINT Actor::GetTotalMeshCount() const
{
    UINT meshCount = 0;
    for (auto& group : m_meshGroups)
    {
        meshCount += UINT(group.m_meshes.size());
    }
    return meshCount;
}

UINT Actor::GetMaterialCount() const
{
    return UINT(m_materials.size());
}

std::shared_ptr<Actor::ActorMaterial> Actor::GetMaterial(UINT idx) const
{
    return m_materials[idx];
}

void Actor::CreateMatrixBufferBLAS(UINT mtxCount)
{
    const auto mtxStride = UINT(sizeof(Mtx3x4));
    const auto mtxCountAll = mtxCount * m_pDevice->BackBufferCount;
    auto buffSize = mtxStride * mtxCountAll;

    // BLAS用の行列バッファを確保
    m_pBLASMatrices = m_pDevice->CreateBuffer(
        buffSize,
        D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        D3D12_HEAP_TYPE_UPLOAD,
        L"MatrixBuffer(BLAS)"
    );
    
    auto numElements = mtxCountAll * 3;
    // SRVの作成
    m_blasMatrixDescriptor = m_pDevice->CreateSRV(m_pBLASMatrices.Get(), numElements, 0, sizeof(Float4));
}

void Actor::CreateBLAS()
{
    std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> rtGeoDesc;
    CreateRTGeoDesc(rtGeoDesc);

    // 動的に更新を行うBLAS
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildASDesc{};
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& inputs = buildASDesc.Inputs;
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.NumDescs = UINT(rtGeoDesc.size());
    inputs.pGeometryDescs = rtGeoDesc.data();
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;

    auto cmd = m_pDevice->CreateCommandList();

    // BLAS関連のバッファを作成
    auto blas = CreateASBuffers(m_pDevice, buildASDesc, m_modelRef->GetName());
    m_pBLAS = blas.asBuffer;
    m_pBLASUpdateBuffer = blas.updateBuffer;

    buildASDesc.ScratchAccelerationStructureData = blas.scratchBuffer->GetGPUVirtualAddress();
    buildASDesc.DestAccelerationStructureData = blas.asBuffer->GetGPUVirtualAddress();

    // BLAS構築
    cmd->BuildRaytracingAccelerationStructure(&buildASDesc, 0, nullptr);
    auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(m_pBLAS.Get());
    cmd->ResourceBarrier(1, &barrier);
    cmd->Close();
    m_pDevice->ExecuteCommandList(cmd);

    // 構築の完了を待機
    m_pDevice->WaitForGpu();
}

void Actor::CreateRTGeoDesc(std::vector<D3D12_RAYTRACING_GEOMETRY_DESC>& rtGeoDesc)
{
    // 書き込み開始位置の調整
    auto frameIndex = m_pDevice->GetCurrentFrameIndex();
    const auto mtxSize = sizeof(Mtx3x4);
    const auto mtxBuffSize = m_meshGroups.size() * mtxSize;
    auto addressBase = m_pBLASMatrices->GetGPUVirtualAddress();
    addressBase += mtxBuffSize * frameIndex;

    ComPtr<ID3D12Resource> posBuffer;
    posBuffer = m_modelRef->GetPositionBuffer();

    UINT mtxIndex = 0;
    for (const auto& meshGroup : m_meshGroups)
    {
        auto indexBuffer = m_modelRef->GetIndexBuffer();
        for (const auto& mesh : meshGroup.m_meshes)
        {
            rtGeoDesc.emplace_back(D3D12_RAYTRACING_GEOMETRY_DESC{});
            auto& desc = rtGeoDesc.back();
            desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
            desc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
            auto& triangles = desc.Triangles;
            // マトリックス情報
            triangles.Transform3x4 = addressBase + mtxIndex * mtxSize;
            // 頂点情報
            triangles.VertexBuffer.StrideInBytes = sizeof(Float3);
            triangles.VertexBuffer.StartAddress = posBuffer->GetGPUVirtualAddress();
            triangles.VertexBuffer.StartAddress += mesh.GetVertexStart() * sizeof(Float3);
            triangles.VertexCount = mesh.GetVertexCount();
            triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
            // インデックス情報
            triangles.IndexBuffer = indexBuffer->GetGPUVirtualAddress();
            triangles.IndexBuffer += mesh.GetIndexStart() * sizeof(UINT);
            triangles.IndexCount = mesh.GetIndexCount();
            triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
        }
        mtxIndex++;
    }
}
