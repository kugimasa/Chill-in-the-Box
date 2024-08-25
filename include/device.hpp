#pragma once

#include <windows.h>

#include <d3d12.h>
#include <dxgi1_6.h>

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
    bool OnInit();

    bool CreateSwapChain(UINT width, UINT height, HWND hwnd);

    ComPtr<ID3D12CommandAllocator> GetCurrentCommandAllocator() {
        return m_pCmdAllocatorArr[m_frameIndex];
    }
    UINT GetCurrentFrameIndex() const { return m_frameIndex; }

    ComPtr<ID3D12GraphicsCommandList4> CreateCommandList();
    ComPtr<ID3D12Fence1> CreateFence();

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
