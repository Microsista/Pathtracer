#include "../Core/stdafx.h"
#include "AdvancedRoom.h"
#pragma comment(lib, "DirectXTK12.lib")

_Use_decl_annotations_
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    Room sample(1280, 720, L"AdvancedRoom");
    return Win32Application::Run(&sample, hInstance, nCmdShow);
}
