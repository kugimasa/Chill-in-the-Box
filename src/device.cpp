#include "device.hpp"

Device::Device() :
    m_fenceValueArr(),
    m_rtvDescSize(0),
    m_dsvDescSize(0),
    m_heapDescSize(0),
    m_heapAllocateIndex(0),
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

    // COM�̏�����
    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr))
    {
        Error(PrintInfoType::D3D12, "COM�̏������Ɏ��s���܂���");
        return false;
    }

    UINT dxgiFlags = 0;

    // �f�o�b�O���C���[�̏�����
#ifdef _DEBUG
    ComPtr<ID3D12Debug> debug;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
    {
        debug->EnableDebugLayer();
        dxgiFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }
#endif

    // DXGIFactory�̐���
    ComPtr<IDXGIFactory3> factory3;
    hr = CreateDXGIFactory2(dxgiFlags, IID_PPV_ARGS(&factory3));
    if (FAILED(hr))
    {
        return false;
    }

    // �n�[�h�E�F�A�A�_�v�^�̌���
    ComPtr<IDXGIAdapter1> hardwareAdapter1;
    {
        UINT adapterIndex = 0;
        ComPtr<IDXGIAdapter1> adapter1;
        while (DXGI_ERROR_NOT_FOUND != factory3->EnumAdapters1(adapterIndex, &adapter1))
        {
            DXGI_ADAPTER_DESC1 desc1{};
            adapter1->GetDesc1(&desc1);
            ++adapterIndex;
            // �\�t�g�E�F�A�A�_�v�^�͎g�p���Ȃ�
            if (desc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                continue;
            }
            // D3D12���T�|�[�g���Ă��邩�̃`�F�b�N����
            // ���ۂɂ̓f�o�C�X���쐬���Ȃ�
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

    // D3D12�f�o�C�X�̍쐬
    hr = D3D12CreateDevice(hardwareAdapter1.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_pD3D12Device5));
    if (FAILED(hr))
    {
        Error(PrintInfoType::D3D12, "D3D12�f�o�C�X�̍쐬�Ɏ��s���܂���");
        return false;
    }
    
    // DXR�ɑΉ����Ă��邩�̊m�F
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5{};
    hr = m_pD3D12Device5->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, UINT(sizeof(options5)));
    if (FAILED(hr) || options5.RaytracingTier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
    {
        Error(PrintInfoType::D3D12, "DirectX Raytracing ���T�|�[�g����Ă��܂���");
        return false;
    }

    // �R�}���h�L���[�̍쐬
    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    hr = m_pD3D12Device5->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_pCmdQueue));
    if (FAILED(hr))
    {
        Error(PrintInfoType::D3D12, "�R�}���h�L���[�̍쐬�Ɏ��s���܂���");
        return false;
    }

    // �f�B�X�N���v�^�q�[�v�̍쐬(RTV)
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heapDesc.NumDescriptors = BackBufferCount;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    hr = m_pD3D12Device5->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(m_pRtvHeap.ReleaseAndGetAddressOf()));
    if (FAILED(hr))
    {
        Error(PrintInfoType::D3D12, "�f�B�X�N���v�^�q�[�v(RTV)�̍쐬�Ɏ��s���܂���");
        return false;
    }
    m_rtvDescSize = m_pD3D12Device5->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    // �f�B�X�N���v�^�q�[�v�̍쐬(DSV);
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    hr = m_pD3D12Device5->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(m_pDsvHeap.ReleaseAndGetAddressOf()));
    if (FAILED(hr))
    {
        Error(PrintInfoType::D3D12, "�f�B�X�N���v�^�q�[�v(DSV)�̍쐬�Ɏ��s���܂���");
        return false;
    }
    m_dsvDescSize = m_pD3D12Device5->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    // �f�B�X�N���v�^�q�[�v�̍쐬(SRV/CBV/UAV�Ȃ�)
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors = ShaderResourceViewMax;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    hr = m_pD3D12Device5->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(m_pHeap.ReleaseAndGetAddressOf()));
    if (FAILED(hr))
    {
        Error(PrintInfoType::D3D12, "�f�B�X�N���v�^�q�[�v(SRV/CBV/UAV�Ȃ�)�̍쐬�Ɏ��s���܂���");
        return false;
    }
    m_heapDescSize = m_pD3D12Device5->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    // �R�}���h�A���P�[�^�[�̏���
    for (UINT i = 0; i < BackBufferCount; ++i)
    {
        hr = m_pD3D12Device5->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(m_pCmdAllocatorArr[i].ReleaseAndGetAddressOf()));
        if (FAILED(hr))
        {
            Error(PrintInfoType::D3D12, "�R�}���h�A���P�[�^�̍쐬�Ɏ��s���܂���");
            return false;
        }
    }

    // �R�}���h���X�g�̍쐬
    auto cmd = CreateCommandList();
    cmd->Close();

    // �t�F���X�̍쐬
    CreateFence();

    m_fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    m_waitEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    return true;
}

void Device::OnDestroy()
{
    WaitForGpu();
    CloseHandle(m_fenceEvent);
    m_fenceEvent = 0;
    CloseHandle(m_waitEvent);
    m_waitEvent = 0;
    for (UINT i = 0; i < BackBufferCount; ++i) {
        m_pRenderTargets[i].Reset();
    }
    m_pDepthStencil.Reset();
    m_pSwapChain3.Reset();
    m_pCmdQueue.Reset();
    m_pD3D12Device5.Reset();
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

        // �X���b�v�`�F�C���̐ݒ�
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc1{};
        swapChainDesc1.BufferCount = BackBufferCount;
        swapChainDesc1.Width = width;
        swapChainDesc1.Height = height;
        swapChainDesc1.Format = m_backBufferFormat;
        swapChainDesc1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc1.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc1.SampleDesc.Count = 1;

        // �X���b�v�`�F�C���̍쐬
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
            Error(PrintInfoType::D3D12, "�X���b�v�`�F�C���̍쐬�Ɏ��s���܂���");
            return false;
        }
        // �X���b�v�`�F�C���̕ϊ�
        swapChain.As(&m_pSwapChain3);

        // �t���[���o�b�t�@�̐������t�F���X���쐬
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
                Error(PrintInfoType::D3D12, "�t���[���o�b�t�@�̃t�F���X�쐬�Ɏ��s���܂���");
                return false;
            }
        }
    }
    
    // RTV�̍쐬
    {
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
        rtvDesc.Format = m_backBufferFormat;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        for (UINT i = 0; i < BackBufferCount; ++i)
        {
            hr = m_pSwapChain3->GetBuffer(i, IID_PPV_ARGS(m_pRenderTargets[i].ReleaseAndGetAddressOf()));
            if (FAILED(hr))
            {
                Error(PrintInfoType::D3D12, "�o�b�N�o�b�t�@�̎擾�Ɏ��s���܂���");
                return false;
            }
            wchar_t name[25] = {};
            swprintf_s(name, L"Render target %u", i);
            m_pRenderTargets[i]->SetName(name);
            auto rtvHandle = m_pRtvHeap->GetCPUDescriptorHandleForHeapStart();
            rtvHandle.ptr += m_rtvDescSize * i;
            m_pD3D12Device5->CreateRenderTargetView(m_pRenderTargets[i].Get(), &rtvDesc, rtvHandle);
        }
    }

    // DSV�̍쐬
    {
        // �q�[�v�v���p�e�B�̐ݒ�
        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
        heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProps.CreationNodeMask = 1;
        heapProps.VisibleNodeMask = 1;

        // ���\�[�X�̐ݒ�
        D3D12_RESOURCE_DESC depthDesc{};
        depthDesc.Format = m_depthFormat;
        depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        depthDesc.Width = width;
        depthDesc.Height = height;
        depthDesc.DepthOrArraySize = 1;
        depthDesc.MipLevels = 1;
        depthDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        depthDesc.SampleDesc.Count = 1;
        // �N���A�l�̐ݒ�
        D3D12_CLEAR_VALUE clearValue{};
        clearValue.Format = m_depthFormat;
        clearValue.DepthStencil.Depth = 1.0f;

        // ���\�[�X�̍쐬
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
            Error(PrintInfoType::D3D12, "DSV�p�̃��\�[�X�쐬�Ɏ��s���܂���");
            return false;
        }
        m_pDepthStencil->SetName(L"Depth Stencil");
    }
    // �t���[���o�b�t�@�̃C���f�b�N�X���X�V
    m_frameIndex = m_pSwapChain3->GetCurrentBackBufferIndex();

    // �r���[�|�[�g�̏�����
    m_viewport.TopLeftX = 0.0f;
    m_viewport.TopLeftY = 0.0f;
    m_viewport.Width = float(width);
    m_viewport.Height = float(height);
    m_viewport.MinDepth = 0.0f;
    m_viewport.MaxDepth = 1.0f;

    // �V�U�[��`�̏�����
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
    // �q�[�v�v���p�e�B�̐ݒ�
    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = heapType;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    // ���\�[�X�̐ݒ�
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

    // ���\�[�X�̐���
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
        std::wstring errWstr = L"�o�b�t�@�̍쐬�Ɏ��s���܂���: " + std::wstring(name);
        Error(PrintInfoType::D3D12, errWstr);
    }
    if (resource != nullptr && name != nullptr)
    {
        resource->SetName(name);
    }
    return resource;
}

// FIXME: CreateBuffer�Ə������܂Ƃ߂ꂻ��
ComPtr<ID3D12Resource> Device::CreateTexture2D(UINT width, UINT height, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES initialState, D3D12_HEAP_TYPE heapType)
{
    // �q�[�v�v���p�e�B�̐ݒ�
    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = heapType;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    // ���\�[�X�̐ݒ�
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

    // ���\�[�X�̐���
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
        Error(PrintInfoType::D3D12, "Texture2D�̍쐬�Ɏ��s���܂���");
    }
    return resource;
}

ComPtr<ID3D12Resource> Device::CreateImageBuffer(ComPtr<ID3D12Resource> pSource, D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState)
{
    auto srcDesc = pSource->GetDesc();
    UINT numRows = 0;
    UINT64 rowSizeInBytes = 0;
    UINT64 totalBytes = 0;
    m_pD3D12Device5->GetCopyableFootprints(
        &srcDesc,
        0,
        1,
        0,
        nullptr,
        &numRows,
        &rowSizeInBytes,
        &totalBytes
    );
    UINT64 srcPitch = (rowSizeInBytes + 255) & ~0xFFu;

    // Readback�p�o�b�t�@�̐ݒ�
    const CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_READBACK);
    D3D12_RESOURCE_DESC bufferDesc = {};
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufferDesc.Height = 1;
    bufferDesc.Width = srcPitch * srcDesc.Height;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufferDesc.MipLevels = 1;
    bufferDesc.SampleDesc.Count = 1;


    // Readback�p�o�b�t�@�̍쐬
    ComPtr<ID3D12Resource> imageBuffer;
    HRESULT hr;
    hr = m_pD3D12Device5->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(imageBuffer.ReleaseAndGetAddressOf())
    );
    if (FAILED(hr))
    {
        Error(PrintInfoType::D3D12, "�摜�o�b�t�@�̍쐬�Ɏ��s���܂���");
    }
    if (imageBuffer != nullptr)
    {
        imageBuffer->SetName(L"Image Buffer");
    }

    // �R�}���h���X�g�̍쐬
    ComPtr<ID3D12CommandAllocator> pCmdAllocator;
    hr = m_pD3D12Device5->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT, 
        IID_PPV_ARGS(pCmdAllocator.GetAddressOf())
    );
    if (FAILED(hr))
    {
        Error(PrintInfoType::D3D12, "�R�}���h�A���P�[�^�̍쐬�Ɏ��s���܂���");
    }
    if (pCmdAllocator != nullptr)
    {
        pCmdAllocator->SetName(L"Image Buffer - CommandAllocator");
    }
    ComPtr<ID3D12GraphicsCommandList4> pCmdList4;
    m_pD3D12Device5->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        pCmdAllocator.Get(),
        nullptr,
        IID_PPV_ARGS(pCmdList4.ReleaseAndGetAddressOf())
    );
    if (FAILED(hr))
    {
        Error(PrintInfoType::D3D12, "�R�}���h���X�g�̍쐬�Ɏ��s���܂���");
    }
    if (pCmdList4 != nullptr)
    {
        pCmdList4->SetName(L"Image Buffer - CommandList");
    }
    auto barrierToCopySrc = CD3DX12_RESOURCE_BARRIER::Transition(
        pSource.Get(),
        beforeState,
        D3D12_RESOURCE_STATE_COPY_SOURCE
    );
    pCmdList4->ResourceBarrier(1, &barrierToCopySrc);

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT bufferFootprint = {};
    bufferFootprint.Footprint.Width = static_cast<UINT>(srcDesc.Width);
    bufferFootprint.Footprint.Height = srcDesc.Height;
    bufferFootprint.Footprint.Depth = 1;
    bufferFootprint.Footprint.RowPitch = static_cast<UINT>(srcPitch);
    bufferFootprint.Footprint.Format = srcDesc.Format;

    // �摜�o�b�t�@�Ƀe�N�X�`���R�s�[
    const CD3DX12_TEXTURE_COPY_LOCATION copyDest(imageBuffer.Get(), bufferFootprint);
    const CD3DX12_TEXTURE_COPY_LOCATION copySrc(pSource.Get(), 0);
    pCmdList4->CopyTextureRegion(&copyDest, 0, 0, 0, &copySrc, nullptr);

    auto barrierToAfterState = CD3DX12_RESOURCE_BARRIER::Transition(
        pSource.Get(),
        D3D12_RESOURCE_STATE_COPY_SOURCE,
        afterState
    );
    pCmdList4->ResourceBarrier(1, &barrierToAfterState);
    // �R�}���h�I��
    pCmdList4->Close();
    // �R�}���h���s
    ExecuteCommandList(pCmdList4);

    // GPU�ҋ@
    auto waitFence = CreateFence();
    UINT64 fenceValue = 1;
    waitFence->SetEventOnCompletion(fenceValue, m_waitEvent);
    m_pCmdQueue->Signal(waitFence.Get(), fenceValue);
    WaitForSingleObject(m_waitEvent, INFINITE);

    return imageBuffer;
}

ComPtr<ID3D12Resource> Device::InitializeBuffer(size_t size, const void* initData, D3D12_RESOURCE_FLAGS flags, D3D12_HEAP_TYPE heapType, const wchar_t* name)
{
    if (size <= 0)
    {
        Error(PrintInfoType::D3D12, "�o�b�t�@�T�C�Y�������ł�: ", size);
    }
    auto initialState = D3D12_RESOURCE_STATE_COMMON;
    if (heapType == D3D12_HEAP_TYPE_UPLOAD)
    {
        initialState = D3D12_RESOURCE_STATE_GENERIC_READ;
    }

    auto resource = CreateBuffer(
        size,
        flags,
        initialState,
        heapType,
        name
    );
    if (resource && name != nullptr) {
        resource->SetName(name);
    }
    if (initData != nullptr) {
        if (heapType == D3D12_HEAP_TYPE_DEFAULT)
        {
            WriteResource(resource, initData, size);
        }
        if (heapType == D3D12_HEAP_TYPE_UPLOAD)
        {
            WriteBuffer(resource, initData, size);
        }
    }
    return resource;
}

ComPtr<ID3D12RootSignature> Device::CreateRootSignature(const std::vector<D3D12_ROOT_PARAMETER>& rootParams, const std::vector<D3D12_STATIC_SAMPLER_DESC>& samplerDesc, const wchar_t* name, const bool isLocal)
{
    D3D12_ROOT_SIGNATURE_DESC rootSigDesc{};
    rootSigDesc.NumParameters = UINT(rootParams.size());
    rootSigDesc.pParameters = rootParams.data();
    if (!samplerDesc.empty())
    {
        rootSigDesc.NumStaticSamplers = UINT(samplerDesc.size());
        rootSigDesc.pStaticSamplers = samplerDesc.data();
    }
    if (isLocal)
    {
        rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
    }
    ComPtr<ID3D12RootSignature> pRootSig;
    HRESULT hr;
    ComPtr<ID3DBlob> pSigBlob;
    ComPtr<ID3DBlob> pErrBlob;
    hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &pSigBlob, &pErrBlob);
    if (FAILED(hr))
    {
        std::wstring errStr = L"���[�g�V�O�l�`���̃V���A���C�Y�Ɏ��s���܂���: " + std::wstring(name);
        Error(PrintInfoType::RTCAMP10, errStr);
    }
    m_pD3D12Device5->CreateRootSignature(
        0,
        pSigBlob->GetBufferPointer(),
        pSigBlob->GetBufferSize(),
        IID_PPV_ARGS(&pRootSig)
    );
    if (FAILED(hr))
    {
        std::wstring errStr = L"���[�g�V�O�l�`���̍쐬�Ɏ��s���܂���: " + std::wstring(name);
        Error(PrintInfoType::RTCAMP10, errStr);
    }
    pRootSig->SetName(name);
    return  pRootSig;
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

void Device::WriteResource(ComPtr<ID3D12Resource> resource, const void* pData, size_t dataSize)
{
    if (resource == nullptr)
    {
        return;
    }
    ComPtr<ID3D12Resource> stagingBuffer;
    HRESULT hr;
    const CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    // ���\�[�X�̐ݒ�
    D3D12_RESOURCE_DESC resDesc{};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Width = dataSize;
    resDesc.Height = 1;
    resDesc.DepthOrArraySize = 1;
    resDesc.MipLevels = 1;
    resDesc.SampleDesc = { 1, 0 };
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    hr = m_pD3D12Device5->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(stagingBuffer.ReleaseAndGetAddressOf())
    );
    if (FAILED(hr))
    {
        Error(PrintInfoType::D3D12, "�X�e�[�W���O�o�b�t�@�̍쐬�Ɏ��s���܂���");
    }
    if (stagingBuffer != nullptr)
    {
        stagingBuffer->SetName(L"Staging Buffer");
    }
    // �f�[�^���X�e�[�W���O�o�b�t�@�փR�s�[
    WriteBuffer(stagingBuffer, pData, dataSize);

    // �X�e�[�W���O�o�b�t�@�̓��e�����\�[�X�ɃR�s�[
    auto cmd = CreateCommandList();
    ComPtr<ID3D12Fence> fence = CreateFence();
    cmd->CopyResource(resource.Get(), stagingBuffer.Get());
    cmd->Close();

    ExecuteCommandList(cmd);
    WaitForGpu();
}

bool Device::CreateConstantBuffer(std::vector<ComPtr<ID3D12Resource>>& resources, UINT size, const wchar_t* name)
{
    size = ROUND_UP(size, 256);
    resources.resize(BackBufferCount);
    for (auto& resource : resources)
    {
        resource = CreateBuffer(
            size,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            D3D12_HEAP_TYPE_UPLOAD
        );
        if (resource)
        {
            resource->SetName(name);
        }
        else
        {
            Error(PrintInfoType::D3D12, "�萔�o�b�t�@�̍쐬�Ɏ��s���܂���");
            return false;
        }
    }
    return true;
}

DescriptorHeap Device::AllocateDescriptorHeap()
{
    DescriptorHeap descriptorHeap{};
    auto it = m_heapDescMap.find(1);
    if (it != m_heapDescMap.end())
    {
        if (!it->second.empty())
        {
            descriptorHeap = it->second.front();
            it->second.pop_front();
            return descriptorHeap;
        }
    }
    auto cpuHandle = m_pHeap->GetCPUDescriptorHandleForHeapStart();
    auto gpuHandle = m_pHeap->GetGPUDescriptorHandleForHeapStart();

    descriptorHeap = {};
    auto heapDesc = m_pHeap->GetDesc();
    if (m_heapAllocateIndex < heapDesc.NumDescriptors)
    {
        auto offset = m_heapDescSize * m_heapAllocateIndex;
        cpuHandle.ptr += offset;
        gpuHandle.ptr += offset;
        descriptorHeap.heapBaseOffset = offset;
        descriptorHeap.cpuHandle = cpuHandle;
        descriptorHeap.gpuHandle = gpuHandle;
        descriptorHeap.heapType = heapDesc.Type;
        m_heapAllocateIndex++;
    }
    return descriptorHeap;
}

void Device::DeallocateDescriptorHeap(DescriptorHeap& hescHeap)
{
    auto it = m_heapDescMap.find(1);
    if (it == m_heapDescMap.end())
    {
        m_heapDescMap.insert(std::make_pair(1, std::list<DescriptorHeap>()));
        it = m_heapDescMap.find(1);
    }
    it->second.push_front(hescHeap);
}

DescriptorHeap Device::CreateSRV(ComPtr<ID3D12Resource> resource, UINT numElements, UINT firstElement, DXGI_FORMAT format)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Format = format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.NumElements = numElements;
    srvDesc.Buffer.FirstElement = firstElement;
    return CreateSRV(resource, &srvDesc);
}

DescriptorHeap Device::CreateSRV(ComPtr<ID3D12Resource> resource, UINT numElements, UINT firstElement, UINT stride)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.NumElements = numElements;
    srvDesc.Buffer.FirstElement = firstElement;
    srvDesc.Buffer.StructureByteStride = stride;
    return CreateSRV(resource, &srvDesc);
}

DescriptorHeap Device::CreateSRV(ComPtr<ID3D12Resource> resource, const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc)
{
    auto descriptor = AllocateDescriptorHeap();
    m_pD3D12Device5->CreateShaderResourceView(
        resource.Get(),
        srvDesc,
        descriptor.cpuHandle);
    return descriptor;
}

DescriptorHeap Device::CreateUAV(ComPtr<ID3D12Resource> resource, const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc)
{
    auto descriptor = AllocateDescriptorHeap();
    m_pD3D12Device5->CreateUnorderedAccessView(
        resource.Get(),
        nullptr,
        uavDesc,
        descriptor.cpuHandle);
    return descriptor;
}

D3D12_CPU_DESCRIPTOR_HANDLE  Device::GetCurrentRTVDesc()
{
    auto rtvCPUHandle = m_pRtvHeap->GetCPUDescriptorHandleForHeapStart();
    auto offset = m_rtvDescSize * m_frameIndex;
    rtvCPUHandle.ptr += offset;
    return rtvCPUHandle;
}

void Device::ExecuteCommandList(ComPtr<ID3D12GraphicsCommandList4> command)
{
    ID3D12CommandList* cmdLists[] = {
        command.Get(),
    };
    m_pCmdQueue->ExecuteCommandLists(1, cmdLists);
}

void Device::Present(UINT syncInterval)
{
    if (m_pSwapChain3)
    {
        m_pSwapChain3->Present(syncInterval, 0);
        auto fence1 = m_pFrameFence1Arr[m_frameIndex];
        auto value = ++m_fenceValueArr[m_frameIndex];
        m_pCmdQueue->Signal(fence1.Get(), value);

        m_frameIndex = m_pSwapChain3->GetCurrentBackBufferIndex();
        fence1 = m_pFrameFence1Arr[m_frameIndex];
        auto endValue = m_fenceValueArr[m_frameIndex];
        if (fence1->GetCompletedValue() < endValue)
        {
            fence1->SetEventOnCompletion(endValue, m_fenceEvent);
            WaitForSingleObject(m_fenceEvent, INFINITE);
        }
    }
}

/// <summary>
/// �R�}���h�̏I����ҋ@
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
