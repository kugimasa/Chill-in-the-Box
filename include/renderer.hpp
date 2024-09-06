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
    // �f�o�C�X�̏������֐�
    bool InitGraphicDevice(HWND hwnd);

    // BLAS�̍\�z
    void BuildBLAS();

    // TLAS�̍\�z
    void BuildTLAS();

    // ���C�g���[�V���O�p��StateObject�̍\�z

    // ���C�g���[�V���O���ʂ̏������ݗp�o�b�t�@�̐���

    // �V�F�[�_�[�e�[�u���̍\�z

    // �O���[�o�����[�g�V�O�l�`������

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
