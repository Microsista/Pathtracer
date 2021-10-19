#include <Windows.h>
#include "DeviceResources.h"
import Core;

_Use_decl_annotations_
int WinMain(HINSTANCE, HINSTANCE, LPSTR, int viewParams) {
    Core* core = new Core(1920, 1080, L"Real-time pathtracer by Karol Jampolski");
    return Application::Run(core, nullptr, viewParams);
}