#include "device.hpp"

Device::Device() :
    m_fenceValueArr(),
    m_rtvDescSize(0),
    m_dsvDescSize(0),
    m_viewport(), 
    m_scissorRect()
{
}

Device::~Device()
{
}

bool Device::OnInit()
{
    HRESULT hr;

    UINT dxgiFlags = 0;

    // デバッグレイヤーの初期化
#ifdef _DEBUG
    ComPtr<ID3D12Debug> debug;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
    {
        debug->EnableDebugLayer();
        dxgiFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }
#endif

    // DXGIFactoryの生成
    ComPtr<IDXGIFactory3> factory3;
    hr = CreateDXGIFactory2(dxgiFlags, IID_PPV_ARGS(&factory3));
    if (FAILED(hr))
    {
        return false;
    }

    // ハードウェアアダプタの検索
    ComPtr<IDXGIAdapter1> hardwareAdapter1;
    {
        UINT adapterIndex = 0;
        ComPtr<IDXGIAdapter1> adapter1;
        while (DXGI_ERROR_NOT_FOUND != factory3->EnumAdapters1(adapterIndex, &adapter1))
        {
            DXGI_ADAPTER_DESC1 desc1{};
            adapter1->GetDesc1(&desc1);
            ++adapterIndex;
            // ソフトウェアアダプタは使用しない
            if (desc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                continue;
            }
            // D3D12をサポートしているかのチェックする
            // 実際にはデバイスを作成しない
            hr = D3D12CreateDevice(
                adapter1.Get(),
                D3D_FEATURE_LEVEL_12_0,
                __uuidof(ID3D12Device),
                nullptr
            );
            if (SUCCEEDED(hr))
            {
                break;
            }
        }
        adapter1.As(&hardwareAdapter1);
    }

    // D3D12デバイスの作成
    hr = D3D12CreateDevice(hardwareAdapter1.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_pD3D12Device5));
    if (FAILED(hr))
    {
        Error(PrintInfoType::D3D12, "D3D12デバイスの作成に失敗しました");
        return false;
    }
    
    // DXRに対応しているかの確認
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5{};
    hr = m_pD3D12Device5->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, UINT(sizeof(options5)));
    if (FAILED(hr) || options5.RaytracingTier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
    {
        Error(PrintInfoType::D3D12, "DirectX Raytracing がサポートされていません");
        return false;
    }

    // コマンドキューの作成
    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    hr = m_pD3D12Device5->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_pCmdQueue));
    if (FAILED(hr))
    {
        Error(PrintInfoType::D3D12, "コマンドキューの作成に失敗しました");
        return false;
    }

    // ディスクリプタヒープの作成(RTV)
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heapDesc.NumDescriptors = BackBufferCount;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    hr = m_pD3D12Device5->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(m_pRtvHeap.ReleaseAndGetAddressOf()));
    if (FAILED(hr))
    {
        Error(PrintInfoType::D3D12, "ディスクリプタヒープ(RTV)の作成に失敗しました");
        return false;
    }
    m_rtvDescSize = m_pD3D12Device5->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    // ディスクリプタヒープの作成(DSV);
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    hr = m_pD3D12Device5->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(m_pDsvHeap.ReleaseAndGetAddressOf()));
    if (FAILED(hr))
    {
        Error(PrintInfoType::D3D12, "ディスクリプタヒープ(DSV)の作成に失敗しました");
        return false;
    }
    m_dsvDescSize = m_pD3D12Device5->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    // ディスクリプタヒープの作成(SRV/CBV/UAVなど)
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors = ShaderResourceViewMax;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    hr = m_pD3D12Device5->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(m_pHeap.ReleaseAndGetAddressOf()));
    if (FAILED(hr))
    {
        Error(PrintInfoType::D3D12, "ディスクリプタヒープ(SRV/CBV/UAVなど)の作成に失敗しました");
        return false;
    }

    // コマンドアロケーターの準備
    for (UINT i = 0; i < BackBufferCount; ++i)
    {
        hr = m_pD3D12Device5->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(m_pCmdAllocatorArr[i].ReleaseAndGetAddressOf()));
        if (FAILED(hr))
        {
            Error(PrintInfoType::D3D12, "コマンドアロケータの作成に失敗しました");
            return false;
        }
    }

    // コマンドリストの作成
    auto cmd = CreateCommandList();
    cmd->Close();

    // フェンスの作成
    CreateFence();

    m_fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    m_waitEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    return true;
}

bool Device::CreateSwapChain(UINT width, UINT height, HWND hwnd)
{
    HRESULT hr;
    if (!m_pSwapChain3)
    {
        ComPtr<IDXGIFactory3> factory;
        hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));
        if (FAILED(hr)) {
            return false;
        }

        // スワップチェインの設定
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc1{};
        swapChainDesc1.BufferCount = BackBufferCount;
        swapChainDesc1.Width = width;
        swapChainDesc1.Height = height;
        swapChainDesc1.Format = m_backBufferFormat;
        swapChainDesc1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc1.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc1.SampleDesc.Count = 1;

        // スワップチェインの作成
        ComPtr<IDXGISwapChain1> swapChain;
        hr = factory->CreateSwapChainForHwnd(
            m_pCmdQueue.Get(),
            hwnd,
            &swapChainDesc1,
            nullptr,
            nullptr,
            &swapChain
        );
        if (FAILED(hr))
        {
            Error(PrintInfoType::D3D12, "スワップチェインの作成に失敗しました");
            return false;
        }
        // スワップチェインの変換
        swapChain.As(&m_pSwapChain3);

        // フレームバッファの数だけフェンスを作成
        for (UINT i = 0; i < BackBufferCount; ++i)
        {
            m_fenceValueArr[i] = 0;
            hr = m_pD3D12Device5->CreateFence(
                m_fenceValueArr[i], 
                D3D12_FENCE_FLAG_NONE,
                IID_PPV_ARGS(m_pFrameFence1Arr[i].ReleaseAndGetAddressOf())
            );
            if (FAILED(hr))
            {
                Error(PrintInfoType::D3D12, "フレームバッファのフェンス作成に失敗しました");
                return false;
            }
        }
    }
    
    // RTVの作成
    {
        auto rtvHandle = m_pRtvHeap->GetCPUDescriptorHandleForHeapStart();
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
        rtvDesc.Format = m_backBufferFormat;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        for (UINT i = 0; i < BackBufferCount; ++i)
        {
            hr = m_pSwapChain3->GetBuffer(i, IID_PPV_ARGS(m_pRenderTargets[i].ReleaseAndGetAddressOf()));
            if (FAILED(hr))
            {
                Error(PrintInfoType::D3D12, "バックバッファの取得に失敗しました");
                return false;
            }
            wchar_t name[25] = {};
            swprintf_s(name, L"Render target %u", i);
            m_pRenderTargets[i]->SetName(name);
            m_pD3D12Device5->CreateRenderTargetView(m_pRenderTargets[i].Get(), &rtvDesc, rtvHandle);
        }
    }

    // DSVの作成
    {
        // ヒーププロパティの設定
        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
        heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProps.CreationNodeMask = 1;
        heapProps.VisibleNodeMask = 1;

        // リソースの設定
        D3D12_RESOURCE_DESC depthDesc{};
        depthDesc.Format = m_depthFormat;
        depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        depthDesc.Width = width;
        depthDesc.Height = height;
        depthDesc.DepthOrArraySize = 1;
        depthDesc.MipLevels = 1;
        depthDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        depthDesc.SampleDesc.Count = 1;
        // クリア値の設定
        D3D12_CLEAR_VALUE clearValue{};
        clearValue.Format = m_depthFormat;
        clearValue.DepthStencil.Depth = 1.0f;

        // リソースの作成
        hr = m_pD3D12Device5->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &depthDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &clearValue,
            IID_PPV_ARGS(m_pDepthStencil.ReleaseAndGetAddressOf())
        );
        if (FAILED(hr))
        {
            Error(PrintInfoType::D3D12, "DSV用のリソース作成に失敗しました");
            return false;
        }
        m_pDepthStencil->SetName(L"Depth Stencil");
    }
    // フレームバッファのインデックスを更新
    m_frameIndex = m_pSwapChain3->GetCurrentBackBufferIndex();

    // ビューポートの初期化
    m_viewport.TopLeftX = 0.0f;
    m_viewport.TopLeftY = 0.0f;
    m_viewport.Width = float(width);
    m_viewport.Height = float(height);
    m_viewport.MinDepth = 0.0f;
    m_viewport.MaxDepth = 1.0f;

    // シザー矩形の初期化
    m_scissorRect.top = 0;
    m_scissorRect.bottom = height;
    m_scissorRect.left = 0;
    m_scissorRect.right = width;

    return true;
}

ComPtr<ID3D12GraphicsCommandList4> Device::CreateCommandList()
{
    ComPtr<ID3D12GraphicsCommandList4> cmdList4;
    auto allocator = GetCurrentCommandAllocator();
    m_pD3D12Device5->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        allocator.Get(),
        nullptr,
        IID_PPV_ARGS(cmdList4.ReleaseAndGetAddressOf())
    );
    return cmdList4;
}

ComPtr<ID3D12Fence1> Device::CreateFence()
{
    ComPtr<ID3D12Fence1> fence1;
    m_pD3D12Device5->CreateFence(
        0,
        D3D12_FENCE_FLAG_NONE,
        IID_PPV_ARGS(fence1.ReleaseAndGetAddressOf())
    );
    return fence1;
}

ComPtr<ID3D12Resource> Device::CreateBuffer(size_t size, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES initialState, D3D12_HEAP_TYPE heapType, const wchar_t* name)
{
    // ヒーププロパティの設定
    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = heapType;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    // リソースの設定
    D3D12_RESOURCE_DESC resDesc{};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Alignment = 0;
    resDesc.Width = size;
    resDesc.Height = 1;
    resDesc.DepthOrArraySize = 1;
    resDesc.MipLevels = 1;
    resDesc.Format = DXGI_FORMAT_UNKNOWN;
    resDesc.SampleDesc = { 1, 0 };
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resDesc.Flags = flags;

    // リソースの生成
    HRESULT hr;
    ComPtr<ID3D12Resource> resource;
    hr = m_pD3D12Device5->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        initialState,
        nullptr,
        IID_PPV_ARGS(resource.ReleaseAndGetAddressOf())
    );

    if (FAILED(hr))
    {
        Error(PrintInfoType::D3D12, "バッファの作成に失敗しました");
    }
    if (resource != nullptr && name != nullptr)
    {
        resource->SetName(name);
    }
    return resource;
}

// FIXME: CreateBufferと処理をまとめれそう
ComPtr<ID3D12Resource> Device::CreateTexture2D(UINT width, UINT height, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES initialState, D3D12_HEAP_TYPE heapType)
{
    // ヒーププロパティの設定
    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = heapType;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    // リソースの設定
    D3D12_RESOURCE_DESC resDesc{};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resDesc.Alignment = 0;
    resDesc.Width = width;
    resDesc.Height = height;
    resDesc.DepthOrArraySize = 1;
    resDesc.MipLevels = 1;
    resDesc.Format = format;
    resDesc.SampleDesc = { 1, 0 };
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resDesc.Flags = flags;

    // リソースの生成
    HRESULT hr;
    ComPtr<ID3D12Resource> resource;
    hr = m_pD3D12Device5->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        initialState,
        nullptr,
        IID_PPV_ARGS(resource.ReleaseAndGetAddressOf())
    );

    if (FAILED(hr))
    {
        Error(PrintInfoType::D3D12, "Texture2Dの作成に失敗しました");
    }
    return resource;
}

void Device::WriteBuffer(ComPtr<ID3D12Resource> resource, const void* pData, size_t dataSize)
{
    if (resource == nullptr)
    {
        return;
    }
    void* mapped = nullptr;
    D3D12_RANGE range{ 0, dataSize };
    HRESULT hr = resource->Map(0, &range, &mapped);
    if (SUCCEEDED(hr))
    {
        memcpy(mapped, pData, dataSize);
        resource->Unmap(0, &range);
    }
}

void Device::ExecuteCommandList(ComPtr<ID3D12GraphicsCommandList4> command)
{
    ID3D12CommandList* cmdLists[] = {
        command.Get(),
    };
    m_pCmdQueue->ExecuteCommandLists(1, cmdLists);
}

/// <summary>
/// コマンドの終了を待機
/// </summary>
void Device::WaitForGpu() noexcept
{
    if (m_pCmdQueue)
    {
        auto cmdList = CreateCommandList();
        auto waitFence = CreateFence();
        UINT64 fenceValue = 1;
        waitFence->SetEventOnCompletion(fenceValue, m_waitEvent);
        m_pCmdQueue->Signal(waitFence.Get(), fenceValue);
        WaitForSingleObject(m_waitEvent, INFINITE);
    }
}
