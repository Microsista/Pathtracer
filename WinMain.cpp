#include <Windows.h>

import Core;
import Application;

_Use_decl_annotations_
int WinMain(HINSTANCE, HINSTANCE, LPSTR, int viewParams) {
    Core core(1920, 1080);
    return Application::Run(&core, nullptr, viewParams);
}