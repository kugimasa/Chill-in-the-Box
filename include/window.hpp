#pragma once

#include "device.hpp"

class Renderer;

class Window
{
public:
    static int Run(Renderer* renderer, HINSTANCE hInstance);

    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    static HWND GetHWND() { return m_hWnd; }

private:
    static HWND m_hWnd;
};
