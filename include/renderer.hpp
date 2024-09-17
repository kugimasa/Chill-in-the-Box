#pragma once

#include "device.hpp"
#include <DirectXMath.h>

using Vector3 = DirectX::XMFLOAT3;

class Renderer
{
public:
    // maxFrameを指定しない限りは描画し続ける
    Renderer(UINT width, UINT height, const std::wstring& title, int maxFrame = -1);

    void OnInit();
    void OnUpdate();
    void OnRender();
    void OnDestroy();

    UINT GetWidth() const { return m_width; }
    UINT GetHeight() const { return m_height; }
    float GetAspect() const { return float(m_width) / float(m_height); }
    const wchar_t* GetTitle() const { return m_title.c_str(); }

private:
    // デバイスの初期化関数
    bool InitGraphicDevice(HWND hwnd);

    // BLASの構築
    void BuildBLAS();

    // TLASの構築
    void BuildTLAS();

    // グローバルルートシグネチャ生成
    void CreateGlobalRootSignature();

    // レイトレーシング用のステートオブジェクトの構築
    void CreateStateObject();

    // レイトレーシング結果の書き込み用バッファの生成
    void CreateOutputBuffer();

    // シェーダーテーブルの構築
    void CreateShaderTable();

#ifdef _DEBUG
    void InitImGui();
    void UpdateImGui();
    void RenderImGui();
#endif // _DEBUG

private:

    UINT m_width;
    UINT m_height;
    UINT m_currentFrame;
    UINT m_maxFrame;
    std::wstring m_title;
    std::unique_ptr<Device> m_pDevice;

    struct Vertex {
        Vector3 Position;
    };

    ComPtr<ID3D12Resource> m_pVertexBuffer;
    ComPtr<ID3D12Resource> m_pRTInstanceBuffer;
    ComPtr<ID3D12Resource> m_pBLAS;
    ComPtr<ID3D12Resource> m_pTLAS;
    ComPtr<ID3D12Resource> m_pOutputBuffer;
    ComPtr<ID3D12Resource> m_pShaderTable;

    ComPtr<ID3D12DescriptorHeap> m_pTLASDescHeap;
    ComPtr<ID3D12DescriptorHeap> m_pOutputBufferDescHeap;

    ComPtr<ID3D12RootSignature> m_pGlobalRootSignature;
    ComPtr<ID3D12StateObject> m_pRTStateObject;
    ComPtr<ID3D12GraphicsCommandList4> m_pCmdList;

    D3D12_DISPATCH_RAYS_DESC m_dispatchRayDesc;
};
