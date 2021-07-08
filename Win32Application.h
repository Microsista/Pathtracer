#pragma once

#include "DXCore.h"

class DXCore;

class Win32Core
{
public:
    static int Run(DXCore* pSample, HINSTANCE hInstance, int nCmdShow);
    static void ToggleFullscreenWindow(IDXGISwapChain* pOutput = nullptr);
    static void SetWindowZorderToTopMost(bool setToTopMost);
    static HWND GetHwnd() { return m_hwnd; }
    static bool IsFullscreen() { return m_fullscreenMode; }

protected:
    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
    static HWND m_hwnd;
    static bool m_fullscreenMode;
    static const UINT m_windowStyle = WS_OVERLAPPEDWINDOW;
    static RECT m_windowRect;
};