#include "stdafx.h"
#include "Core.h"

#ifdef _WIN32
#pragma comment(lib, "lua542/liblua54.a")
#endif

_Use_decl_annotations_
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {       
    Core app(1920, 1080, L"Real-time pathtracer by Karol Jampolski");
    int result = Win32Core::Run(&app, hInstance, nCmdShow);
    
    return result;
}
