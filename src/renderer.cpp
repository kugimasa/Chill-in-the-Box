#include "renderer.hpp"
#include "window.hpp"

#include "utils/shader_compiler.h"
#include "utils/math_util.h"

#include <d3dx12.h>

using namespace DirectX;

Renderer::Renderer(UINT width, UINT height, const std::wstring& title) :
    m_width(width),
    m_height(height),
    m_title(title),
    m_dispatchRayDesc()
{
}

void Renderer::OnInit()
{
    Print(PrintInfoType::RTCAMP10, "=======RTCAMP10=======");
    // グラフィックデバイスの初期化
    if (!InitGraphicDevice(Window::GetHWND())) return;

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
}

void Renderer::OnUpdate()
{
}

void Renderer::OnRender()
{
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

    // Present可能なようにバリアをセット
    auto barrierToPresent = CD3DX12_RESOURCE_BARRIER::Transition(
        renderTarget.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PRESENT
    );
    m_pCmdList->ResourceBarrier(1, &barrierToPresent);

    m_pCmdList->Close();

    m_pDevice->ExecuteCommandList(m_pCmdList);
    m_pDevice->Present(1);
}

void Renderer::OnDestroy()
{
    if (m_pDevice)
    {
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

    // 必要なメモリを算出
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO blasPrebuild{};
    d3d12Device->GetRaytracingAccelerationStructurePrebuildInfo(
        &inputs, &blasPrebuild
    );
    // スクラッチバッファの確保
    ComPtr<ID3D12Resource> blasScratch;
    blasScratch = m_pDevice->CreateBuffer(
        blasPrebuild.ScratchDataSizeInBytes,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_HEAP_TYPE_DEFAULT
    );
    // BLAS用のバッファを確保
    m_pBLAS = m_pDevice->CreateBuffer(
        blasPrebuild.ResultDataMaxSizeInBytes,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
        D3D12_HEAP_TYPE_DEFAULT
    );
    if (m_pBLAS == nullptr || blasScratch == nullptr)
    {
        Error(PrintInfoType::RTCAMP10, "BLASの構築に失敗しました");
    }
    m_pBLAS->SetName(L"BLAS");

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

    // 必要なメモリを算出
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO tlasPrebuild{};
    d3d12Device->GetRaytracingAccelerationStructurePrebuildInfo(
        &inputs, &tlasPrebuild
    );
    // スクラッチバッファの確保
    ComPtr<ID3D12Resource> tlasScratch;
    tlasScratch = m_pDevice->CreateBuffer(
        tlasPrebuild.ScratchDataSizeInBytes,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_HEAP_TYPE_DEFAULT
    );
    // TLAS用のバッファを確保
    m_pTLAS = m_pDevice->CreateBuffer(
        tlasPrebuild.ResultDataMaxSizeInBytes,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
        D3D12_HEAP_TYPE_DEFAULT
    );
    if (m_pTLAS == nullptr || tlasScratch == nullptr)
    {
        Error(PrintInfoType::RTCAMP10, "TLASの構築に失敗しました");
    }
    m_pTLAS->SetName(L"TLAS");

    // Acceleration Structure 構築.
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
    m_pTLASDescHeap = m_pDevice->GetDescriptorHeap();
    auto srvHandle = m_pTLASDescHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.RaytracingAccelerationStructure.Location = m_pTLAS->GetGPUVirtualAddress();
    d3d12Device->CreateShaderResourceView(nullptr, &srvDesc, srvHandle);

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
    std::vector<char> shaderBin;
    // シェーダーロード
#if _DEBUG
    // シェーダーのランタイムコンパイル
    shaderBin = CompileShaderLibrary(RESOURCE_DIR L"/shader/path_tracer.hlsl");
#else
    shaderBin = LoadPreCompiledShaderLibrary(RESOURCE_DIR L"/shader/path_tracer.dxlib");
#endif

    // サブオブジェクト
    std::vector<D3D12_STATE_SUBOBJECT> subObjectVec;
    subObjectVec.reserve(32);

    // シェーダー関数設定
    std::vector<D3D12_EXPORT_DESC> exportVec { 
        { L"RayGen", nullptr, D3D12_EXPORT_FLAG_NONE },
        { L"Miss",     nullptr, D3D12_EXPORT_FLAG_NONE },
        { L"ClosestHit",    nullptr, D3D12_EXPORT_FLAG_NONE },
    };
    D3D12_DXIL_LIBRARY_DESC dxilLibDesc{};
    dxilLibDesc.DXILLibrary = D3D12_SHADER_BYTECODE{ shaderBin.data(), shaderBin.size() };
    dxilLibDesc.NumExports = UINT(exportVec.size());
    dxilLibDesc.pExports = exportVec.data();
    subObjectVec.emplace_back(
        D3D12_STATE_SUBOBJECT{
            D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY,&dxilLibDesc
        }
    );

    // ヒットグループ設定
    D3D12_HIT_GROUP_DESC hitGroupDesc{};
    hitGroupDesc.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
    hitGroupDesc.ClosestHitShaderImport = L"ClosestHit";
    hitGroupDesc.HitGroupExport = L"DefaultHitGroup";
    subObjectVec.emplace_back(
        D3D12_STATE_SUBOBJECT{
            D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP, &hitGroupDesc
        }
    );

    // グローバルルートシグネチャ設定
    D3D12_GLOBAL_ROOT_SIGNATURE globalRootSignature{};
    globalRootSignature.pGlobalRootSignature = m_pGlobalRootSignature.Get();
    subObjectVec.emplace_back(
        D3D12_STATE_SUBOBJECT{
            D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE, & globalRootSignature
        }
    );

    // シェーダー設定
    D3D12_RAYTRACING_SHADER_CONFIG rtShaderConfig{};
    rtShaderConfig.MaxPayloadSizeInBytes = sizeof(XMFLOAT4);
    rtShaderConfig.MaxAttributeSizeInBytes = sizeof(XMFLOAT2);
    subObjectVec.emplace_back(
        D3D12_STATE_SUBOBJECT{
            D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG, &rtShaderConfig
        }
    );

    // パイプライン設定
    D3D12_RAYTRACING_PIPELINE_CONFIG rtPipelineConfig{};
    // MEMO: トレースの深さはここで指定する
    rtPipelineConfig.MaxTraceRecursionDepth = 1;
    subObjectVec.emplace_back(
        D3D12_STATE_SUBOBJECT{
            D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG, &rtPipelineConfig
        }
    );

    // ステートオブジェクトの生成
    D3D12_STATE_OBJECT_DESC stateObjDesc{};
    stateObjDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
    stateObjDesc.NumSubobjects = UINT(subObjectVec.size());
    stateObjDesc.pSubobjects = subObjectVec.data();

    auto d3d12Device = m_pDevice->GetDevice();
    HRESULT hr = d3d12Device->CreateStateObject(
        &stateObjDesc, IID_PPV_ARGS(m_pRTStateObject.ReleaseAndGetAddressOf())
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
        DXGI_FORMAT_B8G8R8A8_UNORM,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_COPY_SOURCE,
        D3D12_HEAP_TYPE_DEFAULT
    );

    // UAVの作成(TLAS 特有)
    // MEMO: BuildTLAS内でSRVは作成済みなのでハンドル位置をずらす必要がある
    m_pOutputBufferDescHeap = m_pDevice->GetDescriptorHeap();
    auto uavHandle = m_pOutputBufferDescHeap->GetCPUDescriptorHandleForHeapStart();
    auto d3d12Device = m_pDevice->GetDevice();
    uavHandle.ptr += d3d12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    d3d12Device->CreateUnorderedAccessView(m_pOutputBuffer.Get(), nullptr, &uavDesc, uavHandle);
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
