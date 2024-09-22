#include "renderer.hpp"
#include "window.hpp"

#include "utils/shader_compiler.h"
#include "utils/math_util.h"

#ifdef _DEBUG
#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>
#endif // _DEBUG
#include <fpng.h>

using namespace DirectX;
using namespace fpng;

Renderer::Renderer(UINT width, UINT height, const std::wstring& title, int maxFrame) :
    m_width(width),
    m_height(height),
    m_currentFrame(0),
    m_maxFrame(maxFrame),
    m_title(title),
    m_dispatchRayDesc()
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

    // �O���[�o�����[�g�V�O�l�`���̗p��
    CreateGlobalRootSignature();

    // �X�e�[�g�I�u�W�F�N�g�̍\�z
    CreateStateObject();

    // �o�̓o�b�t�@�̍쐬
    CreateOutputBuffer();

    // �V�F�[�_�[�e�[�u���̍쐬
    CreateShaderTable();

    // �R�}���h���X�g�̗p��
    m_pCmdList = m_pDevice->CreateCommandList();
    m_pCmdList->Close();

#ifdef _DEBUG
    // ImGui�̏�����
    InitImGui();
#endif // _DEBUG

    // fpng�̏�����
    fpng_init();

}

void Renderer::OnUpdate()
{
#ifdef _DEBUG
    UpdateImGui();
#endif // _DEBUG
}

void Renderer::OnRender()
{
    // �Ō�̃t���[�����`�悳�ꂽ��I��
    if (m_maxFrame > 0 && m_currentFrame >= m_maxFrame)
    {
        // �I������
        Print(PrintInfoType::RTCAMP10, "======================");
#ifdef _DEBUG
        auto hwnd = Window::GetHWND();
        PostMessage(hwnd, WM_QUIT, 0, 0);
#else // _DEBUG
        PostQuitMessage(0);
#endif
        return;
    }

    Print(PrintInfoType::RTCAMP10, "Frame: ", m_currentFrame);
    auto d3d12Device = m_pDevice->GetDevice();
    auto renderTarget = m_pDevice->GetRenderTarget();
    auto allocator = m_pDevice->GetCurrentCommandAllocator();
    allocator->Reset();
    m_pCmdList->Reset(allocator.Get(), nullptr);

    ID3D12DescriptorHeap* descriptorHeaps[] = {
        m_pDevice->GetDescriptorHeap().Get(),
    };
    m_pCmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    m_pCmdList->SetComputeRootSignature(m_pGlobalRootSignature.Get());

    auto tlasGPUHandle = m_pTLASDescHeap.Get()->GetGPUDescriptorHandleForHeapStart();
    auto outputBufferGPUHandle = m_pOutputBufferDescHeap.Get()->GetGPUDescriptorHandleForHeapStart();
    outputBufferGPUHandle.ptr += d3d12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_pCmdList->SetComputeRootDescriptorTable(0, tlasGPUHandle);
    m_pCmdList->SetComputeRootDescriptorTable(1, outputBufferGPUHandle);

    // ���C�g���[�X���ʂ�UAV��
    auto barrierToUAV = CD3DX12_RESOURCE_BARRIER::Transition(
        m_pOutputBuffer.Get(),
        D3D12_RESOURCE_STATE_COPY_SOURCE,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS
    );
    m_pCmdList->ResourceBarrier(1, &barrierToUAV);

    // ���C�g���[�X
    m_pCmdList->SetPipelineState1(m_pRTStateObject.Get());
    m_pCmdList->DispatchRays(&m_dispatchRayDesc);

    // ���C�g���[�X���ʂ��o�b�N�o�b�t�@�փR�s�[
    D3D12_RESOURCE_BARRIER barriers[] = {
        CD3DX12_RESOURCE_BARRIER::Transition(
            m_pOutputBuffer.Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COPY_SOURCE
        ),
        CD3DX12_RESOURCE_BARRIER::Transition(
            renderTarget.Get(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_COPY_DEST
        ),
    };
    m_pCmdList->ResourceBarrier(_countof(barriers), barriers);
    m_pCmdList->CopyResource(renderTarget.Get(), m_pOutputBuffer.Get());

#ifdef _DEBUG
    // ImGui�`��p�̐ݒ�
    auto barrierToRT = CD3DX12_RESOURCE_BARRIER::Transition(
        renderTarget.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_RENDER_TARGET
    );
    m_pCmdList->ResourceBarrier(1, &barrierToRT);

    // ImGui�̕`��
    RenderImGui();

    // �����_�[�^�[�Q�b�g����Present����
    auto barrierToPresent = CD3DX12_RESOURCE_BARRIER::Transition(
        renderTarget.Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT
    );
#else // _DEBUG
    // Present�\�Ȃ悤�Ƀo���A���Z�b�g
    auto barrierToPresent = CD3DX12_RESOURCE_BARRIER::Transition(
        renderTarget.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PRESENT
    );
#endif

    m_pCmdList->ResourceBarrier(1, &barrierToPresent);
    m_pCmdList->Close();

    m_pDevice->ExecuteCommandList(m_pCmdList);
    m_pDevice->Present(1);

    // �ő�t���[���w�肪����ꍇ�ɂ̂݉摜�o��
    if (m_maxFrame > 0)
    {
        // �摜�p�̃o�b�t�@���쐬
        auto imageBuffer = m_pDevice->CreateImageBuffer(
            renderTarget,
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_PRESENT
        );
        // �摜�̏o��
        OutputImage(imageBuffer);
    }
    // �t���[���̍X�V
    m_currentFrame++;
}

void Renderer::OnDestroy()
{
#ifdef _DEBUG
    ImGui_ImplDX12_Shutdown();
#endif // _DEBUG

    if (m_pDevice)
    {
        m_pDevice->OnDestroy();
    }
    m_pDevice.reset();
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
    auto d3d12Device = m_pDevice->GetDevice();

    // FIXME: �����Ŕz�u�Ŗ��Ȃ�...?
    // MEMO: �����̃C���X�^���X������ꍇ�͕ʓr�g�����K�v�ɂȂ肻��
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
    m_pTLAS->SetName(L"TLAS");

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
    m_pTLASDescHeap = m_pDevice->GetDescriptorHeap();
    auto srvHandle = m_pTLASDescHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.RaytracingAccelerationStructure.Location = m_pTLAS->GetGPUVirtualAddress();
    d3d12Device->CreateShaderResourceView(nullptr, &srvDesc, srvHandle);

    // �R�}���h�̊�����ҋ@
    m_pDevice->WaitForGpu();

    Print(PrintInfoType::RTCAMP10, "TLAS�\�z ����");
}

/// <summary>
/// �O���[�o�����[�g�V�O�l�`���̍쐬
/// </summary>
void Renderer::CreateGlobalRootSignature()
{
    std::vector<D3D12_ROOT_PARAMETER> rootParamVec{};
    D3D12_ROOT_PARAMETER rootParam{};
    rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParam.DescriptorTable.NumDescriptorRanges = 1;

    // TLAS: t0
    D3D12_DESCRIPTOR_RANGE descRangeTLAS{};
    descRangeTLAS.BaseShaderRegister = 0;
    descRangeTLAS.NumDescriptors = 1;
    descRangeTLAS.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    rootParam.DescriptorTable.pDescriptorRanges = &descRangeTLAS;
    rootParamVec.push_back(rootParam);
    // UAV: u0
    D3D12_DESCRIPTOR_RANGE descRangeUAV{};
    descRangeUAV.BaseShaderRegister = 0;
    descRangeUAV.NumDescriptors = 1;
    descRangeUAV.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    rootParam.DescriptorTable.pDescriptorRanges = &descRangeUAV;
    rootParamVec.push_back(rootParam);

    D3D12_ROOT_SIGNATURE_DESC rootSigDesc{};
    rootSigDesc.NumParameters = UINT(rootParamVec.size());
    rootSigDesc.pParameters = rootParamVec.data();

    // �O���[�o�����[�g�V�O�l�`���̍쐬
    HRESULT hr;
    ComPtr<ID3DBlob> pSigBlob;
    ComPtr<ID3DBlob> pErrBlob;
    hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &pSigBlob, &pErrBlob);
    if (FAILED(hr))
    {
        Error(PrintInfoType::RTCAMP10, "�O���[�o�����[�g�V�O�l�`���̃V���A���C�Y�Ɏ��s");
    }
    auto d3d12Device = m_pDevice->GetDevice();
    d3d12Device->CreateRootSignature(
        0,
        pSigBlob->GetBufferPointer(),
        pSigBlob->GetBufferSize(),
        IID_PPV_ARGS(m_pGlobalRootSignature.ReleaseAndGetAddressOf())
    );
    if (FAILED(hr))
    {
        Error(PrintInfoType::RTCAMP10, "�O���[�o�����[�g�V�O�l�`���̍쐬�Ɏ��s");
    }
    m_pGlobalRootSignature->SetName(L"GlobalRootSignature");
    Print(PrintInfoType::RTCAMP10, "�O���[�o�����[�g�V�O�l�`���쐬 ����");
}

/// <summary>
/// �X�e�[�g�I�u�W�F�N�g�̍\�z
/// </summary>
void Renderer::CreateStateObject()
{

    // �X�e�[�g�I�u�W�F�N�g�ݒ�
    CD3DX12_STATE_OBJECT_DESC stateObjDesc;
    stateObjDesc.SetStateObjectType(D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE);

    // �V�F�[�_�o�^
    auto rayGenBin = SetupShader(L"raygen");
    D3D12_SHADER_BYTECODE raygenShader{ rayGenBin.data(), rayGenBin.size() };
    auto rayGenDXIL = stateObjDesc.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
    rayGenDXIL->SetDXILLibrary(&raygenShader);
    rayGenDXIL->DefineExport(L"RayGen");

    auto missBin = SetupShader(L"miss");
    D3D12_SHADER_BYTECODE missShader{ missBin.data(), missBin.size() };
    auto missDXIL = stateObjDesc.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
    missDXIL->SetDXILLibrary(&missShader);
    missDXIL->DefineExport(L"Miss");

    auto closestHitBin = SetupShader(L"ch_tri");
    D3D12_SHADER_BYTECODE closestHitShader{ closestHitBin.data(), closestHitBin.size() };
    auto closestHitDXIL = stateObjDesc.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
    closestHitDXIL->SetDXILLibrary(&closestHitShader);
    closestHitDXIL->DefineExport(L"ClosestHit");

    // �q�b�g�O���[�v�ݒ�
    auto hitGroup = stateObjDesc.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
    hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);
    hitGroup->SetClosestHitShaderImport(L"ClosestHit");
    hitGroup->SetHitGroupExport(L"DefaultHitGroup");

    // �O���[�o�����[�g�V�O�l�`���ݒ�
    auto globalRootSig = stateObjDesc.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
    globalRootSig->SetRootSignature(m_pGlobalRootSignature.Get());

    // TODO: ���[�J�����[�g�V�O�l�`���ݒ�
    //

    // ���C�g���[�V���O�p�C�v���C���p�ݒ�
    const UINT MaxPayloadSize = sizeof(XMFLOAT4);
    const UINT MaxAttributeSize = sizeof(XMFLOAT2);
    const UINT MaxRecursionDepth = 1;

    // �V�F�[�_�[�ݒ�
    auto rtShaderConfig = stateObjDesc.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
    rtShaderConfig->Config(MaxPayloadSize, MaxAttributeSize);

    // �p�C�v���C���ݒ�
    auto rtPipelineConfig = stateObjDesc.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
    rtPipelineConfig->Config(MaxRecursionDepth);

    auto d3d12Device = m_pDevice->GetDevice();
    HRESULT hr = d3d12Device->CreateStateObject(
        stateObjDesc,
        IID_PPV_ARGS(m_pRTStateObject.ReleaseAndGetAddressOf())
    ); 
    if (FAILED(hr))
    {
        Error(PrintInfoType::RTCAMP10, "�X�e�[�g�I�u�W�F�N�g�̍\�z�Ɏ��s���܂���: ", hr);
    }
    Print(PrintInfoType::RTCAMP10, "�X�e�[�g�I�u�W�F�N�g�̍\�z ����");
}

/// <summary>
/// ���C�g���[�V���O���ʂ̏������ݗp�o�b�t�@�̍쐬
/// </summary>
void Renderer::CreateOutputBuffer()
{
    auto width = GetWidth();
    auto height = GetHeight();

    // �������ݗp�o�b�t�@�̍쐬
    m_pOutputBuffer = m_pDevice->CreateTexture2D(
        width, height,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_COPY_SOURCE,
        D3D12_HEAP_TYPE_DEFAULT
    );

    // UAV�̍쐬(TLAS ���L)
    // MEMO: BuildTLAS����SRV�͍쐬�ς݂Ȃ̂Ńn���h���ʒu�����炷�K�v������
    m_pOutputBufferDescHeap = m_pDevice->GetDescriptorHeap();
    auto uavHandle = m_pOutputBufferDescHeap->GetCPUDescriptorHandleForHeapStart();
    auto d3d12Device = m_pDevice->GetDevice();
    uavHandle.ptr += d3d12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    d3d12Device->CreateUnorderedAccessView(m_pOutputBuffer.Get(), nullptr, &uavDesc, uavHandle);
    Print(PrintInfoType::RTCAMP10, "�o�͗p�o�b�t�@(UAV)�̍쐬 ����");
}

/// <summary>
/// �V�F�[�_�[�e�[�u���̍\�z
/// </summary>
void Renderer::CreateShaderTable()
{
    // �A���C�����g�����ۂ��A���R�[�h�T�C�Y��ݒ�
    UINT recordSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    recordSize = ROUND_UP(recordSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);

    // �V�F�[�_�[�e�[�u���T�C�Y���v�Z
    // FIXME: ���ꂼ��z��ŊǗ�����Ηǂ�����
    UINT rayGenSize = 1 * recordSize;
    UINT missSize = 1 * recordSize;
    UINT hitGroupSize = 1 * recordSize;

    // �e�e�[�u���ł̊J�n�ʒu�̃A���C�����g����
    auto tableAlign = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;
    UINT rayGenEntrySize = ROUND_UP(rayGenSize, tableAlign);
    UINT missEntrySize = ROUND_UP(missSize, tableAlign);
    UINT hitGroupEntrySize = ROUND_UP(hitGroupSize, tableAlign);
    
    // �V�F�[�_�[�e�[�u���̊m��
    auto tableSize = rayGenEntrySize + missEntrySize + hitGroupEntrySize;
    m_pShaderTable = m_pDevice->CreateBuffer(
        tableSize,
        D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        D3D12_HEAP_TYPE_UPLOAD
    );

    ComPtr<ID3D12StateObjectProperties> pRTStateObjectProps;
    if (m_pRTStateObject == nullptr)
    {
        Error(PrintInfoType::RTCAMP10, "�X�e�[�g�I�u�W�F�N�g�����݂��܂���");
    }
    m_pRTStateObject.As(&pRTStateObjectProps);

    // �e�V�F�[�_�[���R�[�h�̏�������
    void* mapped = nullptr;
    m_pShaderTable->Map(0, nullptr, &mapped);
    uint8_t* pStart = static_cast<uint8_t*>(mapped);

    // RayGen�V�F�[�_�[
    auto rayGenShaderStart = pStart;
    {
        uint8_t* p = rayGenShaderStart;
        std::wstring exportName = L"RayGen";
        auto id = pRTStateObjectProps->GetShaderIdentifier(exportName.c_str());
        if (id == nullptr)
        {
            auto message = L"�V�F�[�_�[ID��������܂���: " + exportName;
            Error(PrintInfoType::RTCAMP10, message);
        }
        memcpy(p, id, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
        p += D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    }

    // Miss�V�F�[�_�[
    auto missShaderStart = pStart + rayGenEntrySize;
    {
        uint8_t* p = missShaderStart;
        std::wstring exportName = L"Miss";
        auto id = pRTStateObjectProps->GetShaderIdentifier(exportName.c_str());
        if (id == nullptr)
        {
            auto message = L"�V�F�[�_�[ID��������܂���: " + exportName;
            Error(PrintInfoType::RTCAMP10, message);
        }
        memcpy(p, id, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
        p += D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    }

    // HitGroup
    auto hitGroupStart = pStart + rayGenEntrySize + missEntrySize;
    {
        uint8_t* p = hitGroupStart;
        std::wstring exportName = L"DefaultHitGroup";
        auto id = pRTStateObjectProps->GetShaderIdentifier(exportName.c_str());
        if (id == nullptr)
        {
            auto message = L"�V�F�[�_�[ID��������܂���: " + exportName;
            Error(PrintInfoType::RTCAMP10, message);
        }
        memcpy(p, id, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
        p += D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    }

    m_pShaderTable->Unmap(0, nullptr);
    Print(PrintInfoType::RTCAMP10, "�V�F�[�_�[�e�[�u���쐬 ����");

    // DispatchRays�p�̏����Z�b�g
    auto& dispatchRayDesc = m_dispatchRayDesc;
    auto startAddress = m_pShaderTable->GetGPUVirtualAddress();
    auto& rayGenShaderRecord = dispatchRayDesc.RayGenerationShaderRecord;
    rayGenShaderRecord.StartAddress = startAddress;
    rayGenShaderRecord.SizeInBytes = rayGenSize;
    startAddress += rayGenEntrySize;

    auto& missShaderTable = dispatchRayDesc.MissShaderTable;
    missShaderTable.StartAddress = startAddress;
    missShaderTable.SizeInBytes = missSize;
    missShaderTable.StrideInBytes = recordSize;
    startAddress += missEntrySize;

    auto& hitGroupTable = dispatchRayDesc.HitGroupTable;
    hitGroupTable.StartAddress = startAddress;
    hitGroupTable.SizeInBytes = hitGroupSize;
    hitGroupTable.StrideInBytes = recordSize;
    startAddress += hitGroupEntrySize;

    dispatchRayDesc.Width = GetWidth();
    dispatchRayDesc.Height = GetHeight();
    dispatchRayDesc.Depth = 1;
    Print(PrintInfoType::RTCAMP10, "DispatchRayDesc�ݒ� ����");
}

void Renderer::OutputImage(ComPtr<ID3D12Resource> imageBuffer)
{
    // CPU���ŉ摜�̏o��
    std::ostringstream sout;
    sout << std::setw(3) << std::setfill('0') << m_currentFrame;
    std::string filename = OUTPUT_DIR + sout.str() + ".png";
    void* pixel = nullptr;
    imageBuffer->Map(0, nullptr, &pixel);
    fpng_encode_image_to_file(filename.c_str(), pixel, m_width, m_height, 4, 0);
    imageBuffer->Unmap(0, nullptr);
}

#ifdef _DEBUG
void Renderer::InitImGui()
{
    auto heap = m_pDevice->GetDescriptorHeap();
    auto imguiCPUHandle = heap->GetCPUDescriptorHandleForHeapStart();
    auto imguiGPUHandle = heap->GetGPUDescriptorHandleForHeapStart();
    auto d3d12Device = m_pDevice->GetDevice();
    auto handleIncrementSize = d3d12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    // SRV/UAV���̃I�t�Z�b�g
    imguiCPUHandle.ptr += handleIncrementSize * 2;
    imguiGPUHandle.ptr += handleIncrementSize * 2;
    ImGui_ImplDX12_Init(
        d3d12Device.Get(),
        Device::BackBufferCount,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        heap.Get(),
        imguiCPUHandle,
        imguiGPUHandle
    );
}
void Renderer::UpdateImGui()
{
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    auto frameRate = ImGui::GetIO().Framerate;
    ImGui::Begin("Info");
    ImGui::Text("FPS %.3f ms", 1000.0f / frameRate);
    ImGui::End();
}

void Renderer::RenderImGui()
{
    auto rtvHandle = m_pDevice->GetCurrentRTVDesc();
    auto viewport = m_pDevice->GetViewport();
    m_pCmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
    m_pCmdList->RSSetViewports(1, &viewport);

    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_pCmdList.Get());
}
#endif // _DEBUG
