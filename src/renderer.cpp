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
    // グラフィックデバイスの初期化
    if (!InitGraphicDevice(Window::GetHWND())) return;

    // シーンの構築
    m_scene.OnInit(m_pDevice, GetAspect());

    // BLASの構築
    BuildBLAS();
    // FIXME: BLAS、TLASの構築処理を可能な限りまとめたい
    BuildTLAS();

    // グローバルルートシグネチャの用意
    CreateGlobalRootSignature();

    // ステートオブジェクトの構築
    CreateStateObject();

    // 出力バッファの作成
    CreateOutputBuffer();

    // シェーダーテーブルの作成
    CreateShaderTable();

    // コマンドリストの用意
    m_pCmdList = m_pDevice->CreateCommandList();
    m_pCmdList->Close();

#ifdef _DEBUG
    // ImGuiの初期化
    InitImGui();
#endif // _DEBUG

    // fpngの初期化
    fpng_init();

}

void Renderer::OnUpdate()
{
    // シーンの更新処理
    m_scene.OnUpdate(m_pDevice, m_currentFrame, m_maxFrame);

#ifdef _DEBUG
    UpdateImGui();
#endif // _DEBUG
}

void Renderer::OnRender()
{
    // 最後のフレームが描画されたら終了
    if (m_maxFrame > 0 && m_currentFrame >= m_maxFrame)
    {
        // 終了処理
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

    m_pCmdList->SetComputeRootDescriptorTable(0, m_tlasDescHeap.gpuHandle);
    m_pCmdList->SetComputeRootDescriptorTable(1, m_outputBufferDescHeap.gpuHandle);

    // 定数バッファの設定
    auto sceneCB = m_scene.GetConstantBuffer(m_pDevice);
    m_pCmdList->SetComputeRootConstantBufferView(2, sceneCB->GetGPUVirtualAddress());

    // レイトレース結果をUAVへ
    auto barrierToUAV = CD3DX12_RESOURCE_BARRIER::Transition(
        m_pOutputBuffer.Get(),
        D3D12_RESOURCE_STATE_COPY_SOURCE,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS
    );
    m_pCmdList->ResourceBarrier(1, &barrierToUAV);

    // レイトレース
    m_pCmdList->SetPipelineState1(m_pRTStateObject.Get());
    m_pCmdList->DispatchRays(&m_dispatchRayDesc);

    // レイトレース結果をバックバッファへコピー
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
    // ImGui描画用の設定
    auto barrierToRT = CD3DX12_RESOURCE_BARRIER::Transition(
        renderTarget.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_RENDER_TARGET
    );
    m_pCmdList->ResourceBarrier(1, &barrierToRT);

    // ImGuiの描画
    RenderImGui();

    // レンダーターゲットからPresentする
    auto barrierToPresent = CD3DX12_RESOURCE_BARRIER::Transition(
        renderTarget.Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT
    );
#else // _DEBUG
    // Present可能なようにバリアをセット
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

    // 最大フレーム指定がある場合にのみ画像出力
    if (m_maxFrame > 0)
    {
        // 画像用のバッファを作成
        auto imageBuffer = m_pDevice->CreateImageBuffer(
            renderTarget,
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_PRESENT
        );
        // 画像の出力
        OutputImage(imageBuffer);
    }
    // フレームの更新
    m_currentFrame++;
}

void Renderer::OnDestroy()
{
#ifdef _DEBUG
    ImGui_ImplDX12_Shutdown();
    m_pDevice->DeallocateDescriptorHeap(m_imguiDescHeap);
#endif // _DEBUG

    if (m_pDevice)
    {
        m_pDevice->DeallocateDescriptorHeap(m_tlasDescHeap);
        m_pDevice->DeallocateDescriptorHeap(m_outputBufferDescHeap);
        m_pDevice->OnDestroy();
    }
    m_pDevice.reset();
}

bool Renderer::InitGraphicDevice(HWND hwnd)
{
    m_pDevice = std::make_unique<Device>();
    // グラフィックデバイスの初期化
    if (!m_pDevice->OnInit())
    {
        Error(PrintInfoType::RTCAMP10, "グラフィックデバイスの初期化に失敗しました");
        return false;
    }
    // スワップチェインの作成
    if (!m_pDevice->CreateSwapChain(GetWidth(), GetHeight(), hwnd))
    {
        Error(PrintInfoType::RTCAMP10, "スワップチェインの作成に失敗しました");
        return false;
    }
    return true;
}

/// <summary>
/// BLASの構築
/// </summary>
void Renderer::BuildBLAS()
{
    auto d3d12Device = m_pDevice->GetDevice();

    // 頂点バッファの用意
    Vertex vertices[] = {
        XMFLOAT3(-0.5f, -0.5f, 0.0f),
        XMFLOAT3(0.5f, -0.5f, 0.0f),
        XMFLOAT3(0.0f, 0.75f, 0.0f),
    };

    // 頂点バッファの生成
    auto vbSize = sizeof(vertices);
    m_pVertexBuffer = m_pDevice->CreateBuffer(
        vbSize,
        D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        D3D12_HEAP_TYPE_UPLOAD
    );
    if (m_pVertexBuffer == nullptr)
    {
        Error(PrintInfoType::RTCAMP10, "頂点バッファの作成に失敗しました");
    }
    m_pDevice->WriteBuffer(m_pVertexBuffer, vertices, vbSize);

    // BLASを作成
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

    // BLAS関連のバッファを確保
    auto blas = CreateASBuffers(m_pDevice, buildASDesc, L"BLAS");
    auto blasScratch = blas.scratchBuffer;
    m_pBLAS = blas.asBuffer;

    // Acceleration Structure を構築
    buildASDesc.ScratchAccelerationStructureData = blasScratch->GetGPUVirtualAddress();
    buildASDesc.DestAccelerationStructureData = m_pBLAS->GetGPUVirtualAddress();
    // コマンドを準備
    auto commad = m_pDevice->CreateCommandList();
    commad->BuildRaytracingAccelerationStructure(
        &buildASDesc, 0, nullptr
    );
    // リソースバリアの設定
    D3D12_RESOURCE_BARRIER uavBarrier{};
    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.UAV.pResource = m_pBLAS.Get();
    commad->ResourceBarrier(1, &uavBarrier);
    commad->Close();
    // コマンドを実行 - BLAS構築
    m_pDevice->ExecuteCommandList(commad);
    // コマンドの完了を待機
    m_pDevice->WaitForGpu();
    Print(PrintInfoType::RTCAMP10, "BLAS構築 完了");
}


/// <summary>
/// TLASの構築
/// </summary>
void Renderer::BuildTLAS()
{
    auto d3d12Device = m_pDevice->GetDevice();

    // FIXME: ここで配置で問題ない...?
    // MEMO: 複数のインスタンスがある場合は別途拡張が必要になりそう
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

    // インスタンス情報用のバッファを確保
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

    // TLAS関連のバッファを確保
    auto tlas = CreateASBuffers(m_pDevice, buildASDesc, L"TLAS");
    auto tlasScratch = tlas.scratchBuffer;
    m_pTLAS = tlas.asBuffer;

    // Acceleration Structure 構築
    buildASDesc.Inputs.InstanceDescs = m_pRTInstanceBuffer->GetGPUVirtualAddress(); // TLAS の場合のみ
    buildASDesc.ScratchAccelerationStructureData = tlasScratch->GetGPUVirtualAddress();
    buildASDesc.DestAccelerationStructureData = m_pTLAS->GetGPUVirtualAddress();
    // コマンドを準備
    auto commad = m_pDevice->CreateCommandList();
    commad->BuildRaytracingAccelerationStructure(
        &buildASDesc, 0, nullptr
    );
    // リソースバリアの設定
    D3D12_RESOURCE_BARRIER uavBarrier{};
    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.UAV.pResource = m_pTLAS.Get();
    commad->ResourceBarrier(1, &uavBarrier);
    commad->Close();

    // コマンドを実行 - TLAS構築
    m_pDevice->ExecuteCommandList(commad);

    // SRVの作成(TLAS 特有)
    m_tlasDescHeap = m_pDevice->AllocateDescriptorHeap();
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.RaytracingAccelerationStructure.Location = m_pTLAS->GetGPUVirtualAddress();
    d3d12Device->CreateShaderResourceView(nullptr, &srvDesc, m_tlasDescHeap.cpuHandle);

    // コマンドの完了を待機
    m_pDevice->WaitForGpu();

    Print(PrintInfoType::RTCAMP10, "TLAS構築 完了");
}

/// <summary>
/// グローバルルートシグネチャの作成
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
    // 出力用バッファ(UAV): u0
    D3D12_DESCRIPTOR_RANGE descRangeUAV{};
    descRangeUAV.BaseShaderRegister = 0;
    descRangeUAV.NumDescriptors = 1;
    descRangeUAV.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    rootParam.DescriptorTable.pDescriptorRanges = &descRangeUAV;
    rootParamVec.push_back(rootParam);
    // SceneCB: b0
    D3D12_ROOT_PARAMETER sceneCBParam{};
    sceneCBParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    sceneCBParam.Descriptor.ShaderRegister = 0;
    sceneCBParam.Descriptor.RegisterSpace = 0;
    rootParamVec.push_back(sceneCBParam);

    D3D12_ROOT_SIGNATURE_DESC rootSigDesc{};
    rootSigDesc.NumParameters = UINT(rootParamVec.size());
    rootSigDesc.pParameters = rootParamVec.data();

    // グローバルルートシグネチャの作成
    HRESULT hr;
    ComPtr<ID3DBlob> pSigBlob;
    ComPtr<ID3DBlob> pErrBlob;
    hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &pSigBlob, &pErrBlob);
    if (FAILED(hr))
    {
        Error(PrintInfoType::RTCAMP10, "グローバルルートシグネチャのシリアライズに失敗");
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
        Error(PrintInfoType::RTCAMP10, "グローバルルートシグネチャの作成に失敗");
    }
    m_pGlobalRootSignature->SetName(L"GlobalRootSignature");
    Print(PrintInfoType::RTCAMP10, "グローバルルートシグネチャ作成 完了");
}

/// <summary>
/// ステートオブジェクトの構築
/// </summary>
void Renderer::CreateStateObject()
{

    // ステートオブジェクト設定
    CD3DX12_STATE_OBJECT_DESC stateObjDesc;
    stateObjDesc.SetStateObjectType(D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE);

    // シェーダ登録
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

    // ヒットグループ設定
    auto hitGroup = stateObjDesc.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
    hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);
    hitGroup->SetClosestHitShaderImport(L"ClosestHit");
    hitGroup->SetHitGroupExport(L"DefaultHitGroup");

    // グローバルルートシグネチャ設定
    auto globalRootSig = stateObjDesc.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
    globalRootSig->SetRootSignature(m_pGlobalRootSignature.Get());

    // TODO: ローカルルートシグネチャ設定
    //

    // レイトレーシングパイプライン用設定
    const UINT MaxPayloadSize = sizeof(XMFLOAT4);
    const UINT MaxAttributeSize = sizeof(XMFLOAT2);
    const UINT MaxRecursionDepth = 1;

    // シェーダー設定
    auto rtShaderConfig = stateObjDesc.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
    rtShaderConfig->Config(MaxPayloadSize, MaxAttributeSize);

    // パイプライン設定
    auto rtPipelineConfig = stateObjDesc.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
    rtPipelineConfig->Config(MaxRecursionDepth);

    auto d3d12Device = m_pDevice->GetDevice();
    HRESULT hr = d3d12Device->CreateStateObject(
        stateObjDesc,
        IID_PPV_ARGS(m_pRTStateObject.ReleaseAndGetAddressOf())
    ); 
    if (FAILED(hr))
    {
        Error(PrintInfoType::RTCAMP10, "ステートオブジェクトの構築に失敗しました: ", hr);
    }
    Print(PrintInfoType::RTCAMP10, "ステートオブジェクトの構築 完了");
}

/// <summary>
/// レイトレーシング結果の書き込み用バッファの作成
/// </summary>
void Renderer::CreateOutputBuffer()
{
    auto width = GetWidth();
    auto height = GetHeight();

    // 書き込み用バッファの作成
    m_pOutputBuffer = m_pDevice->CreateTexture2D(
        width, height,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_COPY_SOURCE,
        D3D12_HEAP_TYPE_DEFAULT
    );

    // UAVの作成(TLAS 特有)
    m_outputBufferDescHeap = m_pDevice->AllocateDescriptorHeap();
    auto d3d12Device = m_pDevice->GetDevice();
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    d3d12Device->CreateUnorderedAccessView(m_pOutputBuffer.Get(), nullptr, &uavDesc, m_outputBufferDescHeap.cpuHandle);
    Print(PrintInfoType::RTCAMP10, "出力用バッファ(UAV)の作成 完了");
}

/// <summary>
/// シェーダーテーブルの構築
/// </summary>
void Renderer::CreateShaderTable()
{
    // アライメント制約を保ちつつ、レコードサイズを設定
    UINT recordSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    recordSize = ROUND_UP(recordSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);

    // シェーダーテーブルサイズを計算
    // FIXME: それぞれ配列で管理すれば良さそう
    UINT rayGenSize = 1 * recordSize;
    UINT missSize = 1 * recordSize;
    UINT hitGroupSize = 1 * recordSize;

    // 各テーブルでの開始位置のアライメント制約
    auto tableAlign = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;
    UINT rayGenEntrySize = ROUND_UP(rayGenSize, tableAlign);
    UINT missEntrySize = ROUND_UP(missSize, tableAlign);
    UINT hitGroupEntrySize = ROUND_UP(hitGroupSize, tableAlign);
    
    // シェーダーテーブルの確保
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
        Error(PrintInfoType::RTCAMP10, "ステートオブジェクトが存在しません");
    }
    m_pRTStateObject.As(&pRTStateObjectProps);

    // 各シェーダーレコードの書き込み
    void* mapped = nullptr;
    m_pShaderTable->Map(0, nullptr, &mapped);
    uint8_t* pStart = static_cast<uint8_t*>(mapped);

    // RayGenシェーダー
    auto rayGenShaderStart = pStart;
    {
        uint8_t* p = rayGenShaderStart;
        std::wstring exportName = L"RayGen";
        auto id = pRTStateObjectProps->GetShaderIdentifier(exportName.c_str());
        if (id == nullptr)
        {
            auto message = L"シェーダーIDが見つかりません: " + exportName;
            Error(PrintInfoType::RTCAMP10, message);
        }
        memcpy(p, id, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
        p += D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    }

    // Missシェーダー
    auto missShaderStart = pStart + rayGenEntrySize;
    {
        uint8_t* p = missShaderStart;
        std::wstring exportName = L"Miss";
        auto id = pRTStateObjectProps->GetShaderIdentifier(exportName.c_str());
        if (id == nullptr)
        {
            auto message = L"シェーダーIDが見つかりません: " + exportName;
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
            auto message = L"シェーダーIDが見つかりません: " + exportName;
            Error(PrintInfoType::RTCAMP10, message);
        }
        memcpy(p, id, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
        p += D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    }

    m_pShaderTable->Unmap(0, nullptr);
    Print(PrintInfoType::RTCAMP10, "シェーダーテーブル作成 完了");

    // DispatchRays用の情報をセット
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
    Print(PrintInfoType::RTCAMP10, "DispatchRayDesc設定 完了");
}

void Renderer::OutputImage(ComPtr<ID3D12Resource> imageBuffer)
{
    // CPU側で画像の出力
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
}
void Renderer::UpdateImGui()
{
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    auto frameRate = ImGui::GetIO().Framerate;
    ImGui::Begin("Info");
    ImGui::Text("Framerate %.3f ms", 1000.0f / frameRate);
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
