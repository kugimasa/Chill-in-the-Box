#include "renderer.hpp"
#include "window.hpp"

int main()
{
    Renderer renderer(1280, 720, L"Sample");
    return Window::Run(&renderer, 0);
}
