#pragma once

#include "device.hpp"
#include "scene/scene.hpp"

class Renderer
{
public:
    // maxFrameを指定しない限りは描画し続ける
    Renderer(UINT width, UINT height, const std::wstring& title, int maxFrame = -1);

    void OnInit();
    void OnUpdate();
    void OnRender();
    void OnDestroy();

    bool GetIsRunning() const{ return m_isRunning; }
    UINT GetWidth() const { return m_width; }
    UINT GetHeight() const { return m_height; }
    float GetAspect() const { return float(m_width) / float(m_height); }
    const wchar_t* GetTitle() const { return m_title.c_str(); }

private:
    // デバイスの初期化関数
    bool InitGraphicDevice(HWND hwnd);

    // TLASの構築
    void BuildTLAS();

    // BLASの更新
    void UpdateTLAS();

    // グローバルルートシグネチャ生成
    void CreateGlobalRootSignature();

    // ローカルルートシグネチャ生成
    void CreateLocalRootSignature();

    // レイトレーシング用のステートオブジェクトの構築
    void CreateStateObject();

    // レイトレーシング結果の書き込み用バッファの生成
    void CreateOutputBuffer();

    // シェーダーテーブルの構築
    void CreateShaderTable();

    // 画像の出力
    void OutputImage(ComPtr<ID3D12Resource> imageBuffer);

#ifdef _DEBUG
    void InitImGui();
    void UpdateImGui();
    void RenderImGui();
#endif // _DEBUG

private:

    bool m_isRunning;
    UINT m_width;
    UINT m_height;
    int m_currentFrame;
    int m_maxFrame;
    std::wstring m_title;
    std::unique_ptr<Device> m_pDevice;

    std::shared_ptr<Scene> m_pScene;

    ComPtr<ID3D12Resource> m_pVertexBuffer;
    std::vector<ComPtr<ID3D12Resource>> m_pRTInstanceBuffers;
    ComPtr<ID3D12Resource> m_pBLAS;
    ComPtr<ID3D12Resource> m_pTLAS;
    ComPtr<ID3D12Resource> m_pTLASUpdate;
    ComPtr<ID3D12Resource> m_pOutputBuffer;
    ComPtr<ID3D12Resource> m_pShaderTable;

    ComPtr<ID3D12RootSignature> m_pGlobalRootSignature;
    ComPtr<ID3D12RootSignature> m_pRayGenLocalRootSignature;
    ComPtr<ID3D12RootSignature> m_pClosestHitLocalRootSignature;
    ComPtr<ID3D12StateObject> m_pRTStateObject;
    ComPtr<ID3D12GraphicsCommandList4> m_pCmdList;

    DescriptorHeap m_imguiDescHeap;
    DescriptorHeap m_tlasDescHeap;
    DescriptorHeap m_outputBufferDescHeap;

    D3D12_DISPATCH_RAYS_DESC m_dispatchRayDesc;

    struct HitInfo {
        Float3 hitPos;
        Float3 reflectDir;
        Float3 color;
        Float3 attenuation;
        UINT rayDepth;
        UINT seed;
    };

    std::chrono::system_clock::time_point m_startTime;
    std::chrono::system_clock::time_point m_endTime;

#ifdef _DEBUG
    struct ImGuiParam {
        Float3 cameraPos;
        int maxPathDepth;
        int maxSPP;
    };
    ImGuiParam m_imGuiParam;
#endif // _DEBUG
};
