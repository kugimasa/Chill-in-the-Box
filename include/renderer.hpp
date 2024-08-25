#pragma once

#include "device.hpp"

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
    bool InitGraphicDevice(HWND hwnd);

private:
    UINT m_width;
    UINT m_height;
    std::wstring m_title;
    std::unique_ptr<Device> m_device;
};
