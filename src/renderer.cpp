#include "renderer.hpp"
#include "window.hpp"

#include "utils/dxr_util.h"
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
    m_isRunning(false),
    m_width(width),
    m_height(height),
    m_currentFrame(0),
    m_maxFrame(maxFrame),
    m_title(title),
#ifdef _DEBUG
    m_imGuiParam(),
#endif // _DEBUG
    m_dispatchRayDesc()
{
}

void Renderer::OnInit()
{
    Print(PrintInfoType::RTCAMP10, L"=======RTCAMP10=======");
    m_isRunning = true;
    // �A�v���P�[�V�����̎��Ԍv���J�n
    m_startTime = std::chrono::system_clock::now();

    // �O���t�B�b�N�f�o�C�X�̏�����
    if (!InitGraphicDevice(Window::GetHWND())) return;

    // �V�[���̏�����
    // �������֐�����BLAS�̍\�z
    m_pScene = std::shared_ptr<Scene>(new Scene(m_pDevice));
    m_pScene->OnInit(GetAspect());

    // TLAS�̍\�z
    BuildTLAS();

    // �O���[�o�����[�g�V�O�l�`���̗p��
    CreateGlobalRootSignature();

    // ���[�J�����[�g�V�O�l�`���̗p��
    CreateLocalRootSignature();

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
    // �V�[���̍X�V����
    m_pScene->OnUpdate(m_currentFrame, m_maxFrame);

#ifdef _DEBUG
    UpdateImGui();
#endif // _DEBUG
}

void Renderer::OnRender()
{
    // �Ō�̃t���[�����`�悳�ꂽ��I��
    if (m_maxFrame > 0 && m_currentFrame >= m_maxFrame)
    {
        // �A�v���P�[�V�����̎��Ԍv���J�n
        m_endTime = std::chrono::system_clock::now();
        // �o�ߎ��Ԃ̎Z�o
        double elapsed = (double)std::chrono::duration_cast<std::chrono::milliseconds>(m_endTime - m_startTime).count();
        std::wstringstream timeWSS;
        timeWSS << L"Total time: " << elapsed * 0.001 << L"(sec)";
        Print(PrintInfoType::RTCAMP10, timeWSS.str());
        // �I������
        Print(PrintInfoType::RTCAMP10, L"======================");
#ifdef _DEBUG
        auto hwnd = Window::GetHWND();
        PostMessage(hwnd, WM_QUIT, 0, 0);
#else // _DEBUG
        PostQuitMessage(0);
#endif
        return;
    }
    // chrono�ϐ�
    std::chrono::system_clock::time_point start, end;
    // ���Ԍv���J�n
    start = std::chrono::system_clock::now();

    auto d3d12Device = m_pDevice->GetDevice();
    auto renderTarget = m_pDevice->GetRenderTarget();
    auto allocator = m_pDevice->GetCurrentCommandAllocator();
    allocator->Reset();
    m_pCmdList->Reset(allocator.Get(), nullptr);

    ID3D12DescriptorHeap* descriptorHeaps[] = {
        m_pDevice->GetDescriptorHeap().Get(),
    };
    m_pCmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    // BLAS�̍X�V
    m_pScene->UpdateBLAS(m_pCmdList);

    // TLAS�̍X�V
    UpdateTLAS();

    m_pCmdList->SetComputeRootSignature(m_pGlobalRootSignature.Get());
    m_pCmdList->SetComputeRootDescriptorTable(0, m_tlasDescHeap.gpuHandle);
    // �w�i�e�N�X�`��
    m_pCmdList->SetComputeRootDescriptorTable(1, m_pScene->GetBackgroundTex().srv.gpuHandle);
    // �萔�o�b�t�@�̐ݒ�
    m_pCmdList->SetComputeRootConstantBufferView(2, m_pScene->GetConstantBuffer()->GetGPUVirtualAddress());

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
    // ���Ԍv���I��
    end = std::chrono::system_clock::now();
    // �o�ߎ��Ԃ̎Z�o
    double elapsed = (double)std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::ostringstream timeOSS;
    timeOSS << "Frame: " << std::setw(3) << std::setfill('0') << m_currentFrame << " | " << elapsed * 0.001 << "(sec)";
    Print(PrintInfoType::RTCAMP10, StrToWStr(timeOSS.str()).c_str());
    // �t���[���̍X�V
    m_currentFrame++;
}

void Renderer::OnDestroy()
{
#ifdef _DEBUG
    ImGui_ImplDX12_Shutdown();
    m_pDevice->DeallocateDescriptorHeap(m_imguiDescHeap);
#endif // _DEBUG
    m_pScene->OnDestroy();
    m_pScene.reset();
    if (m_pDevice)
    {
        m_pDevice->DeallocateDescriptorHeap(m_tlasDescHeap);
        m_pDevice->DeallocateDescriptorHeap(m_outputBufferDescHeap);
        m_pDevice->OnDestroy();
    }
    m_pDevice.reset();
    m_isRunning = false;
}

bool Renderer::InitGraphicDevice(HWND hwnd)
{
    m_pDevice = std::make_unique<Device>();
    // �O���t�B�b�N�f�o�C�X�̏�����
    if (!m_pDevice->OnInit())
    {
        Error(PrintInfoType::RTCAMP10, L"�O���t�B�b�N�f�o�C�X�̏������Ɏ��s���܂���");
        return false;
    }
    // �X���b�v�`�F�C���̍쐬
    if (!m_pDevice->CreateSwapChain(GetWidth(), GetHeight(), hwnd))
    {
        Error(PrintInfoType::RTCAMP10, L"�X���b�v�`�F�C���̍쐬�Ɏ��s���܂���");
        return false;
    }
    Print(PrintInfoType::RTCAMP10, L"�f�o�C�X�̏����� ����");
    return true;
}

/// <summary>
/// TLAS�̍\�z
/// </summary>
void Renderer::BuildTLAS()
{
    auto d3d12Device = m_pDevice->GetDevice();

    std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instanceDescs;
    m_pScene->CreateRTInstanceDesc(instanceDescs);

    // �C���X�^���X���p�̃o�b�t�@���m��
    m_pRTInstanceBuffers.resize(m_pDevice->BackBufferCount);
    auto instanceDescSize = UINT(ROUND_UP(instanceDescs.size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC), 256));
    for (auto& rtInstanceBuff : m_pRTInstanceBuffers)
    {
        rtInstanceBuff = m_pDevice->CreateBuffer(
            instanceDescSize,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            D3D12_HEAP_TYPE_UPLOAD,
            L"RTInstanceDescBuffer"
        );
        m_pDevice->WriteBuffer(rtInstanceBuff, instanceDescs.data(), instanceDescSize);
    }

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildASDesc{};
    auto& inputs = buildASDesc.Inputs;
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
    inputs.NumDescs = UINT(instanceDescs.size());
    inputs.InstanceDescs = m_pRTInstanceBuffers.front()->GetGPUVirtualAddress();

    // TLAS�֘A�̃o�b�t�@���m��
    auto tlas = CreateASBuffers(m_pDevice, buildASDesc, L"TLAS");
    auto tlasScratch = tlas.scratchBuffer;
    m_pTLAS = tlas.asBuffer;
    m_pTLASUpdate = tlas.updateBuffer;

    // SRV�̍쐬
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.RaytracingAccelerationStructure.Location = m_pTLAS->GetGPUVirtualAddress();
    m_tlasDescHeap = m_pDevice->CreateSRV(nullptr, &srvDesc);

    // Acceleration Structure �\�z
    buildASDesc.ScratchAccelerationStructureData = tlasScratch->GetGPUVirtualAddress();
    buildASDesc.DestAccelerationStructureData = m_pTLAS->GetGPUVirtualAddress();

    // �R�}���h������
    auto cmdList = m_pDevice->CreateCommandList();
    cmdList->BuildRaytracingAccelerationStructure(&buildASDesc, 0, nullptr);
    // ���\�[�X�o���A�̐ݒ�
    auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(m_pTLAS.Get());
    cmdList->ResourceBarrier(1, &barrier);
    cmdList->Close();
    // �R�}���h�����s - TLAS�\�z
    m_pDevice->ExecuteCommandList(cmdList);

    // �R�}���h�̊�����ҋ@
    m_pDevice->WaitForGpu();

    Print(PrintInfoType::RTCAMP10, L"TLAS�\�z ����");
}

void Renderer::UpdateTLAS()
{
    auto d3d12Device = m_pDevice->GetDevice();
    auto frameIdx = m_pDevice->GetCurrentFrameIndex();

    std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instanceDescs;
    m_pScene->CreateRTInstanceDesc(instanceDescs);

    auto instanceDescSize = UINT(instanceDescs.size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC));
    auto rtInstanceBuff = m_pRTInstanceBuffers[frameIdx];
    m_pDevice->WriteBuffer(rtInstanceBuff, instanceDescs.data(), instanceDescSize);

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC updateASDesc{};
    auto& inputs = updateASDesc.Inputs;
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.Flags =
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE |
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
    inputs.NumDescs = UINT(instanceDescs.size());
    inputs.InstanceDescs = rtInstanceBuff->GetGPUVirtualAddress();

    // TLAS�𒼐ڍX�V
    updateASDesc.SourceAccelerationStructureData = m_pTLAS->GetGPUVirtualAddress();
    updateASDesc.DestAccelerationStructureData = m_pTLAS->GetGPUVirtualAddress();
    updateASDesc.ScratchAccelerationStructureData = m_pTLASUpdate->GetGPUVirtualAddress();

    m_pCmdList->BuildRaytracingAccelerationStructure(&updateASDesc, 0, nullptr);
    // ���\�[�X�o���A�̐ݒ�
    auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(m_pTLAS.Get());
    m_pCmdList->ResourceBarrier(1, &barrier);
    // �R�}���h�̎��s��OnRender�֐����Ŏ��s
}

/// <summary>
/// �O���[�o�����[�g�V�O�l�`���̍쐬
/// </summary>
void Renderer::CreateGlobalRootSignature()
{
    std::vector<D3D12_ROOT_PARAMETER> rootParams{};
    D3D12_ROOT_PARAMETER rootParam{};
    std::vector<D3D12_STATIC_SAMPLER_DESC> samplerDescs{};
    D3D12_STATIC_SAMPLER_DESC samplerDesc{};

    // TLAS: t0
    rootParam = CreateRootParam(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0);
    rootParams.push_back(rootParam);
    // BgTex: t1
    rootParam = CreateRootParam(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1);
    rootParams.push_back(rootParam);
    // SceneCB: b0
    rootParam = CreateRootParam(D3D12_ROOT_PARAMETER_TYPE_CBV, 0);
    rootParams.push_back(rootParam);
    // Sampler: s0
    samplerDesc = CreateStaticSamplerDesc(D3D12_FILTER_MIN_MAG_MIP_LINEAR, 0);
    samplerDescs.push_back(samplerDesc);

    // �O���[�o�����[�g�V�O�l�`���̍쐬
    m_pGlobalRootSignature = m_pDevice->CreateRootSignature(rootParams, samplerDescs, L"GlobalRootSignature");
    Print(PrintInfoType::RTCAMP10, L"�O���[�o�����[�g�V�O�l�`���쐬 ����");
}

/// <summary>
/// ���[�J�����[�g�V�O�l�`���̍쐬
/// </summary>
void Renderer::CreateLocalRootSignature()
{
    std::vector<D3D12_ROOT_PARAMETER> rootParams{};
    D3D12_ROOT_PARAMETER rootParam{};
    std::vector<D3D12_STATIC_SAMPLER_DESC> samplerDescs{};

    // RayGen�V�F�[�_�[�p�̃��[�g�V�O�l�`��
    rootParams = {};
    rootParam = {};
    samplerDescs = {};
    // OutputBuffer : u0
    rootParam = CreateRootParam(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0);
    rootParams.push_back(rootParam);
    // ���[�J�����[�g�V�O�l�`���̍쐬
    m_pRayGenLocalRootSignature = m_pDevice->CreateRootSignature(rootParams, samplerDescs, L"LocalRootSignature:RayGen", /*isLocal*/ true);
    
    // ClosestHit�V�F�[�_�[�p�̃��[�g�V�O�l�`��
    rootParams = {};
    rootParam = {};
    samplerDescs = {};
    //// Register Space 1 ///
    // IB: t0
    rootParam = CreateRootParam(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
    rootParams.push_back(rootParam);
    // VB(POS): t1
    rootParam = CreateRootParam(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
    rootParams.push_back(rootParam);
    // VB(NORM): t2
    rootParam = CreateRootParam(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 1);
    rootParams.push_back(rootParam);
    // VB(TEXCOORD): t3
    rootParam = CreateRootParam(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 1);
    rootParams.push_back(rootParam);
    // BLASMatrixBuff: t4
    rootParam = CreateRootParam(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 1);
    rootParams.push_back(rootParam);

    /// Register Space 2 ///
    // DiffuseTex: t0
    rootParam = CreateRootParam(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 2);
    rootParams.push_back(rootParam);
    // MeshParam: b0
    rootParam = CreateRootParam(D3D12_ROOT_PARAMETER_TYPE_CBV, 0, 2);
    rootParams.push_back(rootParam);
    // ���[�J�����[�g�V�O�l�`���̍쐬
    m_pClosestHitLocalRootSignature = m_pDevice->CreateRootSignature(rootParams, samplerDescs, L"LocalRootSignature:ClosestHit", /*isLocal*/ true);

    Print(PrintInfoType::RTCAMP10, L"���[�J�����[�g�V�O�l�`���쐬 ����");
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

    auto closestHitBin = SetupShader(L"closesthit");
    D3D12_SHADER_BYTECODE closestHitShader{ closestHitBin.data(), closestHitBin.size() };
    auto closestHitDXIL = stateObjDesc.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
    closestHitDXIL->SetDXILLibrary(&closestHitShader);
    closestHitDXIL->DefineExport(L"ClosestHit");
    // MEMO: ����ClosestHit�V�F�[�_�[������ꍇ�͂�����Œǉ�

    // �q�b�g�O���[�v�ݒ� (Actor)
    auto hitGroupModel = stateObjDesc.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
    hitGroupModel->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);
    hitGroupModel->SetClosestHitShaderImport(L"ClosestHit");
    hitGroupModel->SetHitGroupExport(L"Actor");

    // �O���[�o�����[�g�V�O�l�`���ݒ�
    auto globalRootSig = stateObjDesc.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
    globalRootSig->SetRootSignature(m_pGlobalRootSignature.Get());

    // ���[�J�����[�g�V�O�l�`���ݒ�: RayGen
    auto rayGenLocalRootSig = stateObjDesc.CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
    rayGenLocalRootSig->SetRootSignature(m_pRayGenLocalRootSignature.Get());
    auto rgLocalRootSigExpAssoc = stateObjDesc.CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
    rgLocalRootSigExpAssoc->AddExport(L"RayGen");
    rgLocalRootSigExpAssoc->SetSubobjectToAssociate(*rayGenLocalRootSig);
    // ���[�J�����[�g�V�O�l�`���ݒ�: ClosestHit
    auto closesHitLocalRootSig = stateObjDesc.CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
    closesHitLocalRootSig->SetRootSignature(m_pClosestHitLocalRootSignature.Get());
    auto chLocalRootSigExpAssoc = stateObjDesc.CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
    chLocalRootSigExpAssoc->AddExport(L"Actor");
    chLocalRootSigExpAssoc->SetSubobjectToAssociate(*closesHitLocalRootSig);

    // ���C�g���[�V���O�p�C�v���C���p�ݒ�
    // common.hlsli�Ƒ�����
    const UINT MaxPayloadSize = sizeof(HitInfo);
    const UINT MaxAttributeSize = sizeof(XMFLOAT2);
    const UINT MaxRecursionDepth = 1; // �ő�ċA�i��

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
        std::wstring err = L"�X�e�[�g�I�u�W�F�N�g�̍\�z�Ɏ��s���܂���: " + (int)hr;
        Error(PrintInfoType::RTCAMP10, err);
    }
    Print(PrintInfoType::RTCAMP10, L"�X�e�[�g�I�u�W�F�N�g�̍\�z ����");
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
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    m_outputBufferDescHeap = m_pDevice->CreateUAV(m_pOutputBuffer.Get(), &uavDesc);
    Print(PrintInfoType::RTCAMP10, L"�o�͗p�o�b�t�@(UAV)�̍쐬 ����");
}

/// <summary>
/// �V�F�[�_�[�e�[�u���̍\�z
/// </summary>
void Renderer::CreateShaderTable()
{
    // RayGen: ShaderId
    UINT rayGenRecordSize = 0;
    rayGenRecordSize += D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    rayGenRecordSize += sizeof(D3D12_GPU_DESCRIPTOR_HANDLE); // OutputBuffer: u0
    rayGenRecordSize = ROUND_UP(rayGenRecordSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);


    // Miss: ShaderId
    UINT missRecordSize = 0;
    missRecordSize += D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    missRecordSize = ROUND_UP(missRecordSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);

    // HitGroup: ShaderId, IB, VB(POS/NORM/TEXCOORD), CB
    UINT hitGroupRecordSize = 0;
    hitGroupRecordSize += D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    hitGroupRecordSize += sizeof(D3D12_GPU_DESCRIPTOR_HANDLE); // IB: t0
    hitGroupRecordSize += sizeof(D3D12_GPU_DESCRIPTOR_HANDLE); // VB(POS): t1
    hitGroupRecordSize += sizeof(D3D12_GPU_DESCRIPTOR_HANDLE); // VB(NORM): t2
    hitGroupRecordSize += sizeof(D3D12_GPU_DESCRIPTOR_HANDLE); // VB(TEXCOORD): t3
    // hitGroupRecordSize += sizeof(D3D12_GPU_DESCRIPTOR_HANDLE); // BLAS UpdateBuffer: t4 ... ??
    // hitGroupRecordSize += sizeof(D3D12_GPU_DESCRIPTOR_HANDLE); // MaterialDiffuse: t0 ... ??
    hitGroupRecordSize += sizeof(D3D12_GPU_DESCRIPTOR_HANDLE); // CB: b0
    hitGroupRecordSize = ROUND_UP(hitGroupRecordSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);

    UINT hitGroupCount = m_pScene->GetTotalHitGroupCount();

    // �V�F�[�_�[�e�[�u���T�C�Y���v�Z
    UINT rayGenSize = 1 * rayGenRecordSize;
    UINT missSize = 1 * missRecordSize;
    UINT hitGroupSize = hitGroupCount * hitGroupRecordSize;

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
        Error(PrintInfoType::RTCAMP10, L"�X�e�[�g�I�u�W�F�N�g�����݂��܂���");
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
        p += WriteShaderId(p, id);
        // OutputBuffer: u0
        p += WriteGPUDescriptorHeap(p, m_outputBufferDescHeap);
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
        p += WriteShaderId(p, id);
    }

    // HitGroup
    auto hitGroupStart = pStart + rayGenEntrySize + missEntrySize;
    {
        auto recordStart = hitGroupStart;
        recordStart = m_pScene->WriteHitGroupShaderRecord(recordStart, hitGroupRecordSize, m_pRTStateObject);
    }
    m_pShaderTable->Unmap(0, nullptr);
    Print(PrintInfoType::RTCAMP10, L"�V�F�[�_�[�e�[�u���쐬 ����");

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
    missShaderTable.StrideInBytes = missRecordSize;
    startAddress += missEntrySize;

    auto& hitGroupTable = dispatchRayDesc.HitGroupTable;
    hitGroupTable.StartAddress = startAddress;
    hitGroupTable.SizeInBytes = hitGroupSize;
    hitGroupTable.StrideInBytes = hitGroupRecordSize;
    startAddress += hitGroupEntrySize;

    dispatchRayDesc.Width = GetWidth();
    dispatchRayDesc.Height = GetHeight();
    dispatchRayDesc.Depth = 1;
    Print(PrintInfoType::RTCAMP10, L"DispatchRayDesc�ݒ� ����");
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
    m_imguiDescHeap = m_pDevice->AllocateDescriptorHeap();
    auto d3d12Device = m_pDevice->GetDevice();
    ImGui_ImplDX12_Init(
        d3d12Device.Get(),
        Device::BackBufferCount,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        heap.Get(),
        m_imguiDescHeap.cpuHandle,
        m_imguiDescHeap.gpuHandle
    );

    m_imGuiParam.cameraPos = m_pScene->GetCamera()->GetPosition();
    m_imGuiParam.maxPathDepth = m_pScene->GetMaxPathDepth();
    m_imGuiParam.maxSPP = m_pScene->GetMaxSPP();
}
void Renderer::UpdateImGui()
{
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    auto frameRate = ImGui::GetIO().Framerate;
    ImGui::Begin("Info");
    ImGui::Text("Framerate %.3f ms", 1000.0f / frameRate);

    // �J�����p�����[�^
    std::shared_ptr<Camera> pCamera = m_pScene->GetCamera();
    m_imGuiParam.cameraPos = pCamera->GetPosition();
    ImGui::SliderFloat3("CameraPos Z", &m_imGuiParam.cameraPos.x, -10.0, 10.0);

    // ���ˉ�
    m_imGuiParam.maxPathDepth = m_pScene->GetMaxPathDepth();
    ImGui::SliderInt("Max Path Depth", &m_imGuiParam.maxPathDepth, 1, 32);

    // SPP
    m_imGuiParam.maxSPP = m_pScene->GetMaxSPP();
    ImGui::SliderInt("Max SPP", &m_imGuiParam.maxSPP, 1, 100);
    ImGui::End();

    // �X�V
    pCamera->SetPosition(m_imGuiParam.cameraPos);
    m_pScene->SetMaxPathDepth(m_imGuiParam.maxPathDepth);
    m_pScene->SetMaxSPP(m_imGuiParam.maxSPP);
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
