#include "Core.h"

int WinMain(HINSTANCE, HINSTANCE, LPSTR, int viewParams) {
    Core core(1920, 1080, L"Real-time pathtracer by Karol Jampolski");
    return Application::Run(&core, nullptr, viewParams);
}