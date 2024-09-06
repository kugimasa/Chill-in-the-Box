#include "renderer.hpp"
#include "window.hpp"

using namespace DirectX;

Renderer::Renderer(UINT width, UINT height, const std::wstring& title) :
    m_width(width),
    m_height(height),
    m_title(title)
{
}

void Renderer::OnInit()
{
    Print(PrintInfoType::RTCAMP10, "=======RTCAMP10=======");
    // �O���t�B�b�N�f�o�C�X�̏�����
    if (!InitGraphicDevice(Window::GetHWND())) return;

    // BLAS�̍\�z
    BuildBLAS();
    // FIXME: BLAS�ATLAS�̍\�z�������\�Ȍ���܂Ƃ߂���
    BuildTLAS();
}

bool Renderer::InitGraphicDevice(HWND hwnd)
{
    m_pDevice = std::make_unique<Device>();
    // �O���t�B�b�N�f�o�C�X�̏�����
    if (!m_pDevice->OnInit())
    {
        Error(PrintInfoType::RTCAMP10, "�O���t�B�b�N�f�o�C�X�̏������Ɏ��s���܂���");
        return false;
    }
    // �X���b�v�`�F�C���̍쐬
    if (!m_pDevice->CreateSwapChain(GetWidth(), GetHeight(), hwnd))
    {
        Error(PrintInfoType::RTCAMP10, "�X���b�v�`�F�C���̍쐬�Ɏ��s���܂���");
        return false;
    }
    return true;
}

/// <summary>
/// BLAS�̍\�z
/// </summary>
void Renderer::BuildBLAS()
{
    Print(PrintInfoType::RTCAMP10, "BLAS�\�z �J�n");
    auto d3d12Device = m_pDevice->GetDevice();

    // ���_�o�b�t�@�̗p��
    Vertex vertices[] = {
        XMFLOAT3(-0.5f, -0.5f, 0.0f),
        XMFLOAT3(0.5f, -0.5f, 0.0f),
        XMFLOAT3(0.0f, 0.75f, 0.0f),
    };

    // ���_�o�b�t�@�̐���
    auto vbSize = sizeof(vertices);
    m_pVertexBuffer = m_pDevice->CreateBuffer(
        vbSize,
        D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        D3D12_HEAP_TYPE_UPLOAD
    );
    if (m_pVertexBuffer == nullptr)
    {
        Error(PrintInfoType::RTCAMP10, "���_�o�b�t�@�̍쐬�Ɏ��s���܂���");
    }
    m_pDevice->WriteBuffer(m_pVertexBuffer, vertices, vbSize);

    // BLAS���쐬
    D3D12_RAYTRACING_GEOMETRY_DESC geomDesc{};
    geomDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    geomDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
    geomDesc.Triangles.VertexBuffer.StartAddress = m_pVertexBuffer->GetGPUVirtualAddress();
    geomDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
    geomDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
    geomDesc.Triangles.VertexCount = _countof(vertices);

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildASDesc{};
    auto& inputs = buildASDesc.Inputs; // D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
    inputs.NumDescs = 1;
    inputs.pGeometryDescs = &geomDesc;

    // �K�v�ȃ��������Z�o
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO blasPrebuild{};
    d3d12Device->GetRaytracingAccelerationStructurePrebuildInfo(
        &inputs, &blasPrebuild
    );
    // �X�N���b�`�o�b�t�@�̊m��
    ComPtr<ID3D12Resource> blasScratch;
    blasScratch = m_pDevice->CreateBuffer(
        blasPrebuild.ScratchDataSizeInBytes,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_HEAP_TYPE_DEFAULT
    );
    // BLAS�p�̃o�b�t�@���m��
    m_pBLAS = m_pDevice->CreateBuffer(
        blasPrebuild.ResultDataMaxSizeInBytes,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
        D3D12_HEAP_TYPE_DEFAULT
    );
    if (m_pBLAS == nullptr || blasScratch == nullptr)
    {
        Error(PrintInfoType::RTCAMP10, "BLAS�̍\�z�Ɏ��s���܂���");
    }
    m_pBLAS->SetName(L"BLAS");

    // Acceleration Structure ���\�z
    buildASDesc.ScratchAccelerationStructureData = blasScratch->GetGPUVirtualAddress();
    buildASDesc.DestAccelerationStructureData = m_pBLAS->GetGPUVirtualAddress();
    // �R�}���h������
    auto commad = m_pDevice->CreateCommandList();
    commad->BuildRaytracingAccelerationStructure(
        &buildASDesc, 0, nullptr
    );
    // ���\�[�X�o���A�̐ݒ�
    D3D12_RESOURCE_BARRIER uavBarrier{};
    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.UAV.pResource = m_pBLAS.Get();
    commad->ResourceBarrier(1, &uavBarrier);
    commad->Close();
    // �R�}���h�����s - BLAS�\�z
    m_pDevice->ExecuteCommandList(commad);
    // �R�}���h�̊�����ҋ@
    m_pDevice->WaitForGpu();
    Print(PrintInfoType::RTCAMP10, "BLAS�\�z ����");
}


/// <summary>
/// TLAS�̍\�z
/// </summary>
void Renderer::BuildTLAS()
{
    Print(PrintInfoType::RTCAMP10, "TLAS�\�z �J�n");
    auto d3d12Device = m_pDevice->GetDevice();

    // FIXME: �����Ŕz�u�Ŗ��Ȃ�...?
    D3D12_RAYTRACING_INSTANCE_DESC instanceDesc{};
    XMStoreFloat3x4(
        reinterpret_cast<XMFLOAT3X4*>(&instanceDesc.Transform),
        XMMatrixIdentity()
    );
    instanceDesc.InstanceID = 0;
    instanceDesc.InstanceMask = 0xFF;
    instanceDesc.InstanceContributionToHitGroupIndex = 0;
    instanceDesc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
    instanceDesc.AccelerationStructure = m_pBLAS->GetGPUVirtualAddress();

    // �C���X�^���X���p�̃o�b�t�@���m��
    m_pRTInstanceBuffer = m_pDevice->CreateBuffer(
        sizeof(instanceDesc),
        D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        D3D12_HEAP_TYPE_UPLOAD
    );
    m_pDevice->WriteBuffer(m_pRTInstanceBuffer, &instanceDesc, sizeof(instanceDesc));

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildASDesc{};
    auto& inputs = buildASDesc.Inputs; // D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
    inputs.NumDescs = 1;

    // �K�v�ȃ��������Z�o
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO tlasPrebuild{};
    d3d12Device->GetRaytracingAccelerationStructurePrebuildInfo(
        &inputs, &tlasPrebuild
    );
    // �X�N���b�`�o�b�t�@�̊m��
    ComPtr<ID3D12Resource> tlasScratch;
    tlasScratch = m_pDevice->CreateBuffer(
        tlasPrebuild.ScratchDataSizeInBytes,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_HEAP_TYPE_DEFAULT
    );
    // TLAS�p�̃o�b�t�@���m��
    m_pTLAS = m_pDevice->CreateBuffer(
        tlasPrebuild.ResultDataMaxSizeInBytes,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
        D3D12_HEAP_TYPE_DEFAULT
    );
    if (m_pTLAS == nullptr || tlasScratch == nullptr)
    {
        Error(PrintInfoType::RTCAMP10, "TLAS�̍\�z�Ɏ��s���܂���");
    }
    m_pBLAS->SetName(L"TLAS");

    // Acceleration Structure �\�z.
    buildASDesc.Inputs.InstanceDescs = m_pRTInstanceBuffer->GetGPUVirtualAddress(); // TLAS �̏ꍇ�̂�
    buildASDesc.ScratchAccelerationStructureData = tlasScratch->GetGPUVirtualAddress();
    buildASDesc.DestAccelerationStructureData = m_pTLAS->GetGPUVirtualAddress();
    // �R�}���h������
    auto commad = m_pDevice->CreateCommandList();
    commad->BuildRaytracingAccelerationStructure(
        &buildASDesc, 0, nullptr
    );
    // ���\�[�X�o���A�̐ݒ�
    D3D12_RESOURCE_BARRIER uavBarrier{};
    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.UAV.pResource = m_pTLAS.Get();
    commad->ResourceBarrier(1, &uavBarrier);
    commad->Close();

    // �R�}���h�����s - TLAS�\�z
    m_pDevice->ExecuteCommandList(commad);

    // SRV�̍쐬(TLAS ���L)
    auto heap = m_pDevice->GetDescriptorHeap();
    auto srvHandle = heap->GetCPUDescriptorHandleForHeapStart();
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.RaytracingAccelerationStructure.Location = m_pTLAS->GetGPUVirtualAddress();
    d3d12Device->CreateShaderResourceView(nullptr, &srvDesc, srvHandle);

    // �R�}���h�̊�����ҋ@
    m_pDevice->WaitForGpu();

    Print(PrintInfoType::RTCAMP10, "TLAS�\�z ����");
}
