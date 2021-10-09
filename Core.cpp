#include "Core.h"


_Use_decl_annotations_
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    Core* app = new Core(1920, 1080, L"Real-time pathtracer by Karol Jampolski");
    return Win32Core::Run(app, hInstance, nCmdShow);
}
