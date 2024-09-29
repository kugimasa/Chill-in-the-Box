#include "renderer.hpp"
#include "window.hpp"

int main(int argc, char *argv[])
{
    int maxFrame = -1;
    // コマンドライン入力形式
    // ./[renderer].exe --frame {max_frame}
    if (argc == 3)
    {
        if (strcmp(argv[1], "--frame") == 0) {
            maxFrame = atoi(argv[2]);
        }
    }
    Renderer renderer(1024, 1024, L"rtcamp10", maxFrame);
    return Window::Run(&renderer, 0);
}
