#include "window.hpp"
#include "renderer.hpp"

#ifdef _DEBUG
#include <imgui.h>
#include <imgui_impl_win32.h>
#endif

HWND Window::m_hWnd = nullptr;

int Window::Run(Renderer* renderer, HINSTANCE hInstance)
{
    if (!renderer) return EXIT_FAILURE;

#ifdef _DEBUG
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
#endif // _DEBUG

    try
    {
        // ウィンドウ情報のセット
        WNDCLASSEXW windowClass{};
        windowClass.cbSize = sizeof(WNDCLASSEXW);
        windowClass.style = CS_HREDRAW | CS_VREDRAW;
        windowClass.hInstance = hInstance;
        windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
        windowClass.lpfnWndProc = WindowProc;
        windowClass.lpszClassName = L"Renderer";
        RegisterClassExW(&windowClass);

        // ウィンドウサイズの設定
        RECT lpRect = {0, 0, LONG(renderer->GetWidth()), LONG(renderer->GetHeight())};
        DWORD dwStyle = WS_OVERLAPPEDWINDOW;
        dwStyle &= ~(WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SIZEBOX);
        AdjustWindowRect(&lpRect, dwStyle, FALSE);

        // ウィンドウ生成
        m_hWnd = CreateWindowW(
            windowClass.lpszClassName,
            renderer->GetTitle(),
            dwStyle,
            CW_USEDEFAULT, CW_USEDEFAULT,
            lpRect.right - lpRect.left, lpRect.bottom - lpRect.top,
            nullptr,
            nullptr,
            hInstance,
            renderer
        );

#ifdef _DEBUG
        ImGui_ImplWin32_Init(m_hWnd);
#endif // _DEBUG

        // レンダラーの初期化
        renderer->OnInit();

#ifdef _DEBUG
        int nCmdShow = SW_SHOWNORMAL;
#else // _DEBUG
        // Release版ではウィンドウを出さない
        int nCmdShow = SW_HIDE;
#endif
        
        ShowWindow(m_hWnd, nCmdShow);

        // メインループ
        MSG msg{};
        while (msg.message != WM_QUIT) {
            if (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            else
            {
                // Release版ではここで描画実行
                renderer->OnRender();
            }
        }

        renderer->OnDestroy();

#ifdef _DEBUG
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
#endif // _DEBUG

        return EXIT_SUCCESS;
    }
    catch (std::exception& e)
    {
        renderer->OnDestroy();
        Error(PrintInfoType::RTCAMP10, "ウィンドウの作成に失敗しました");
        return EXIT_FAILURE;
    }
}

LRESULT CALLBACK Window::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* renderer = (Renderer*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

#ifdef _DEBUG
    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
    {
        return TRUE;
    }
#endif // _DEBUG

    switch (message)
    {

    case WM_CREATE:
        {
            auto pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
            SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
        }
        return 0;

    case WM_PAINT:
        if (renderer)
        {
            renderer->OnUpdate();
            renderer->OnRender();
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }

    return DefWindowProcW(hWnd, message, wParam, lParam);
}
