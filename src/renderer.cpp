#include "renderer.hpp"
#include "window.hpp"

Renderer::Renderer(UINT width, UINT height, const std::wstring& title) :
    m_width(width),
    m_height(height),
    m_title(title)
{
}

void Renderer::OnInit()
{
    Print(PrintInfoType::RTCAMP10, "=======RTCAMP10=======");
    if (!InitGraphicDevice(Window::GetHWND()))
    {
    }
}

bool Renderer::InitGraphicDevice(HWND hwnd)
{
    m_device = std::make_unique<Device>();
    // �O���t�B�b�N�f�o�C�X�̏�����
    if (!m_device->OnInit())
    {
        Error(PrintInfoType::RTCAMP10, "�O���t�B�b�N�f�o�C�X�̏������Ɏ��s���܂���");
        return false;
    }
    // �X���b�v�`�F�C���̍쐬
    if (!m_device->CreateSwapChain(GetWidth(), GetHeight(), hwnd))
    {
        Error(PrintInfoType::RTCAMP10, "�X���b�v�`�F�C���̍쐬�Ɏ��s���܂���");
        return false;
    }
    return true;
}
