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
    // グラフィックデバイスの初期化
    if (!m_device->OnInit())
    {
        Error(PrintInfoType::RTCAMP10, "グラフィックデバイスの初期化に失敗しました");
        return false;
    }
    // スワップチェインの作成
    if (!m_device->CreateSwapChain(GetWidth(), GetHeight(), hwnd))
    {
        Error(PrintInfoType::RTCAMP10, "スワップチェインの作成に失敗しました");
        return false;
    }
    return true;
}
