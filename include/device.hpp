#pragma once

#include <windows.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dx12.h>

#include <string>
#include <wrl.h>
#include <memory>
#include <vector>
#include <list>
#include <array>
#include <unordered_map>
#include <stdexcept>

#include "utils/print_util.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;

class Device
{
public:
    Device();
    Device(const Device&) = delete;
    
    ~Device();

    bool OnInit();
    void OnDestroy();

    bool CreateSwapChain(UINT width, UINT height, HWND hwnd);
    ComPtr<ID3D12GraphicsCommandList4> CreateCommandList();
    ComPtr<ID3D12Fence1> CreateFence();
    ComPtr<ID3D12Resource> CreateBuffer(size_t size, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES initialState, D3D12_HEAP_TYPE heapType, const wchar_t* name = nullptr);
    ComPtr<ID3D12Resource> CreateTexture2D(UINT width, UINT height, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES initialState, D3D12_HEAP_TYPE heapType);
    ComPtr<ID3D12Resource> CreateImageBuffer(ComPtr<ID3D12Resource> pSource, D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState);
    void WriteBuffer(ComPtr<ID3D12Resource> resource, const void* pData, size_t dataSize);

    ComPtr<ID3D12Device5> GetDevice() { return m_pD3D12Device5; }
    ComPtr<ID3D12CommandAllocator> GetCurrentCommandAllocator() {
        return m_pCmdAllocatorArr[m_frameIndex];
    }
    ComPtr<ID3D12DescriptorHeap> GetDescriptorHeap() { return m_pHeap; }
    ComPtr<ID3D12DescriptorHeap> GetRTVHeap() { return m_pRtvHeap; }
    ComPtr<ID3D12Resource> GetRenderTarget() { return m_pRenderTargets[m_frameIndex]; }
    ComPtr<ID3D12Resource> GetDepthStencil() { return m_pDepthStencil; }
    UINT GetCurrentFrameIndex() const { return m_frameIndex; }
    const D3D12_VIEWPORT& GetViewport() const { return m_viewport;  }

    D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRTVDesc();

    void ExecuteCommandList(ComPtr<ID3D12GraphicsCommandList4> command);
    void Present(UINT syncInterval);
    void WaitForGpu() noexcept;

public:
    static const UINT BackBufferCount = 3;
    static const UINT FrameBufferCount = 64;
    static const UINT ShaderResourceViewMax = 1024;

private:
    ComPtr<ID3D12Device5> m_pD3D12Device5;
    ComPtr<ID3D12CommandQueue> m_pCmdQueue;
    ComPtr<IDXGISwapChain3> m_pSwapChain3;

    std::array<ComPtr<ID3D12CommandAllocator>, BackBufferCount> m_pCmdAllocatorArr;
    std::array<ComPtr<ID3D12Fence1>, BackBufferCount> m_pFrameFence1Arr;
    std::array<UINT64, BackBufferCount> m_fenceValueArr;

    ComPtr<ID3D12DescriptorHeap> m_pRtvHeap;
    UINT m_rtvDescSize;
    ComPtr<ID3D12DescriptorHeap> m_pDsvHeap;
    UINT m_dsvDescSize;
    ComPtr<ID3D12DescriptorHeap> m_pHeap;

    HANDLE m_fenceEvent = 0;
    HANDLE m_waitEvent = 0;

    UINT m_frameIndex = 0;

    DXGI_FORMAT m_backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT m_depthFormat = DXGI_FORMAT_D32_FLOAT;

    std::array<ComPtr<ID3D12Resource>, BackBufferCount> m_pRenderTargets;
    ComPtr<ID3D12Resource> m_pDepthStencil;

    D3D12_VIEWPORT  m_viewport;
    D3D12_RECT m_scissorRect;
};
