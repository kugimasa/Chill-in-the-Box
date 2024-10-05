#pragma once

#include "device.hpp"
#include "scene/scene.hpp"

class Renderer
{
public:
    // maxFrame���w�肵�Ȃ�����͕`�悵������
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
    // �f�o�C�X�̏������֐�
    bool InitGraphicDevice(HWND hwnd);

    // BLAS�̍\�z
    void BuildBLAS();

    // TLAS�̍\�z
    void BuildTLAS();

    // BLAS�̍X�V
    void UpdateTLAS();

    // �O���[�o�����[�g�V�O�l�`������
    void CreateGlobalRootSignature();

    // ���[�J�����[�g�V�O�l�`������
    void CreateLocalRootSignature();

    // ���C�g���[�V���O�p�̃X�e�[�g�I�u�W�F�N�g�̍\�z
    void CreateStateObject();

    // ���C�g���[�V���O���ʂ̏������ݗp�o�b�t�@�̐���
    void CreateOutputBuffer();

    // �V�F�[�_�[�e�[�u���̍\�z
    void CreateShaderTable();

    // �摜�̏o��
    void OutputImage(ComPtr<ID3D12Resource> imageBuffer);

#ifdef _DEBUG
    void InitImGui();
    void UpdateImGui();
    void RenderImGui();
#endif // _DEBUG

private:

    UINT m_width;
    UINT m_height;
    int m_currentFrame;
    int m_maxFrame;
    std::wstring m_title;
    std::unique_ptr<Device> m_pDevice;

    struct Vertex {
        Float3 Position;
    };

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
        UINT rayDepth;
    };

#ifdef _DEBUG
    struct ImGuiParam {
        Float3 cameraPos;
        int maxPathDepth;
    };
    ImGuiParam m_imGuiParam;
#endif // _DEBUG
};
