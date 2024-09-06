#pragma once

#include "device.hpp"
#include <DirectXMath.h>

using Vector3 = DirectX::XMFLOAT3;

class Renderer
{
public:

    Renderer(UINT width, UINT height, const std::wstring& title);

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

    // レイトレーシング用のStateObjectの構築

    // レイトレーシング結果の書き込み用バッファの生成

    // シェーダーテーブルの構築

    // グローバルルートシグネチャ生成

private:

    UINT m_width;
    UINT m_height;
    std::wstring m_title;
    std::unique_ptr<Device> m_pDevice;

    struct Vertex {
        Vector3 Position;
    };

    ComPtr<ID3D12Resource> m_pVertexBuffer;
    ComPtr<ID3D12Resource> m_pRTInstanceBuffer;

    ComPtr<ID3D12Resource> m_pBLAS;
    ComPtr<ID3D12Resource> m_pTLAS;


};
