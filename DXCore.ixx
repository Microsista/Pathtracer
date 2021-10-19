module;
#include <Windows.h>
#include <dxgi1_6.h>
#include <string>
#include "DeviceResources.h"
export module DXCore;

import Camera;
import DXSampleHelper;

export {
    class DXCore : public DX::IDeviceNotify
    {
    public:
        DXCore(UINT width, UINT height, std::wstring name) :
            m_width(width),
            m_height(height),
            m_windowBounds{ 0,0,0,0 },
            m_title(name),
            m_aspectRatio(0.0f),
            m_enableUI(true),
            m_adapterIDoverride(UINT_MAX)
        {
            WCHAR assetsPath[512];
            GetAssetsPath(assetsPath, _countof(assetsPath));
            m_assetsPath = assetsPath;

            UpdateForSizeChange(width, height);
        }
        virtual ~DXCore()
        {
        }

        virtual void OnInit() = 0;
        virtual void OnUpdate() = 0;
        virtual void OnRender() = 0;
        virtual void OnSizeChanged(UINT width, UINT height, bool minimized) = 0;
        virtual void OnDestroy() = 0;

        // Samples override the event handlers to handle specific messages.
        virtual void OnKeyDown(UINT8 /*key*/) {}
        virtual void OnKeyUp(UINT8 /*key*/) {}
        virtual void OnWindowMoved(int /*x*/, int /*y*/) {}
        virtual void OnMouseMove(int /*x*/, int /*y*/) {}
        virtual void OnLeftButtonDown(UINT /*x*/, UINT /*y*/) {}
        virtual void OnLeftButtonUp(UINT /*x*/, UINT /*y*/) {}
        virtual void OnDisplayChanged() {}

        // Overridable members.
        virtual void ParseCommandLineArgs(_In_reads_(argc) WCHAR* argv[], int argc)
        {
            for (int i = 1; i < argc; ++i)
            {
                // -disableUI
                if (_wcsnicmp(argv[i], L"-disableUI", wcslen(argv[i])) == 0 ||
                    _wcsnicmp(argv[i], L"/disableUI", wcslen(argv[i])) == 0)
                {
                    m_enableUI = false;
                }
                // -forceAdapter [id]
                else if (_wcsnicmp(argv[i], L"-forceAdapter", wcslen(argv[i])) == 0 ||
                    _wcsnicmp(argv[i], L"/forceAdapter", wcslen(argv[i])) == 0)
                {
                    ThrowIfFalse(i + 1 < argc, L"Incorrect argument format passed in.");

                    m_adapterIDoverride = _wtoi(argv[i + 1]);
                    i++;
                }
            }

        }

        // Accessors.
        UINT GetWidth() const { return m_width; }
        UINT GetHeight() const { return m_height; }
        const WCHAR* GetTitle() const { return m_title.c_str(); }
        RECT GetWindowsBounds() const { return m_windowBounds; }
        virtual IDXGISwapChain* GetSwapchain() { return nullptr; }
        DX::DeviceResources* GetDeviceResources() const { return m_deviceResources.get(); }

        void UpdateForSizeChange(UINT clientWidth, UINT clientHeight)
        {
            m_width = clientWidth;
            m_height = clientHeight;
            m_aspectRatio = static_cast<float>(clientWidth) / static_cast<float>(clientHeight);
            m_camera.SetLens(0.25f * DirectX::XM_PI, m_aspectRatio, 1.0f, 1000.0f);
        }
        void SetWindowBounds(int left, int top, int right, int bottom)
        {
            m_windowBounds.left = static_cast<LONG>(left);
            m_windowBounds.top = static_cast<LONG>(top);
            m_windowBounds.right = static_cast<LONG>(right);
            m_windowBounds.bottom = static_cast<LONG>(bottom);
        }
        // Helper function for resolving the full path of assets.
        std::wstring GetAssetFullPath(LPCWSTR assetName)
        {
            return m_assetsPath + assetName;
        }

    protected:
        // Helper function for setting the window's title text.
        void SetCustomWindowText(LPCWSTR text)
        {
            std::wstring windowText = m_title + L": " + text;
            SetWindowText(Application::GetHwnd(), windowText.c_str());
        }

        // Viewport dimensions.
        UINT m_width;
        UINT m_height;
        float m_aspectRatio;

        // Window bounds
        RECT m_windowBounds;

        // Override to be able to start without Dx11on12 UI for PIX. PIX doesn't support 11 on 12. 
        bool m_enableUI;

        // D3D device resources
        UINT m_adapterIDoverride;
        std::shared_ptr<DX::DeviceResources> m_deviceResources;

        // Camera
        Camera m_camera;

    private:
        // Root assets path.
        std::wstring m_assetsPath;

        // Window title.
        std::wstring m_title;
    };


    using namespace Microsoft::WRL;
    using namespace std;

    using Microsoft::WRL::ComPtr;

    int Application::Run(DXCore* pSample, HINSTANCE hInstance, int nCmdShow)
    {
        try
        {
            // Parse the command line parameters
            int argc;
            LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
            pSample->ParseCommandLineArgs(argv, argc);
            LocalFree(argv);

            // Initialize the window class.
            WNDCLASSEX windowClass = { 0 };
            windowClass.cbSize = sizeof(WNDCLASSEX);
            windowClass.style = CS_HREDRAW | CS_VREDRAW;
            windowClass.lpfnWndProc = WindowProc;
            windowClass.hInstance = hInstance;
            windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
            windowClass.lpszClassName = L"DXSampleClass";
            RegisterClassEx(&windowClass);

            RECT windowRect = { 0, 0, static_cast<LONG>(pSample->GetWidth()), static_cast<LONG>(pSample->GetHeight()) };
            AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

            // Create the window and store a handle to it.
            m_hwnd = CreateWindow(
                windowClass.lpszClassName,
                pSample->GetTitle(),
                m_windowStyle,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                windowRect.right - windowRect.left,
                windowRect.bottom - windowRect.top,
                nullptr,        // We have no parent window.
                nullptr,        // We aren't using menus.
                hInstance,
                pSample);

            // Initialize the sample. OnInit is defined in each child-implementation of DXSample.
            pSample->OnInit();

            ShowWindow(m_hwnd, nCmdShow);

            // Main sample loop.
            MSG msg = {};
            while (msg.message != WM_QUIT)
            {
                // Process any messages in the queue.
                if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
                {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
            }

            pSample->OnDestroy();

            // Return this part of the WM_QUIT message to Windows.
            return static_cast<char>(msg.wParam);
        }
        catch (std::exception& e)
        {
            OutputDebugString(L"Application hit a problem: ");
            OutputDebugStringA(e.what());
            OutputDebugString(L"\nTerminating.\n");

            pSample->OnDestroy();
            return EXIT_FAILURE;
        }
    }

    // Convert a styled window into a fullscreen borderless window and back again.
    void Application::ToggleFullscreenWindow(IDXGISwapChain* pSwapChain)
    {
        if (m_fullscreenMode)
        {
            // Restore the window's attributes and size.
            SetWindowLong(m_hwnd, GWL_STYLE, m_windowStyle);

            SetWindowPos(
                m_hwnd,
                HWND_NOTOPMOST,
                m_windowRect.left,
                m_windowRect.top,
                m_windowRect.right - m_windowRect.left,
                m_windowRect.bottom - m_windowRect.top,
                SWP_FRAMECHANGED | SWP_NOACTIVATE);

            ShowWindow(m_hwnd, SW_NORMAL);
        }
        else
        {
            // Save the old window rect so we can restore it when exiting fullscreen mode.
            GetWindowRect(m_hwnd, &m_windowRect);

            // Make the window borderless so that the client area can fill the screen.
            SetWindowLong(m_hwnd, GWL_STYLE, m_windowStyle & ~(WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU | WS_THICKFRAME));

            RECT fullscreenWindowRect;
            try
            {
                if (pSwapChain)
                {
                    // Get the settings of the display on which the app's window is currently displayed
                    ComPtr<IDXGIOutput> pOutput;
                    ThrowIfFailed(pSwapChain->GetContainingOutput(&pOutput));
                    DXGI_OUTPUT_DESC Desc;
                    ThrowIfFailed(pOutput->GetDesc(&Desc));
                    fullscreenWindowRect = Desc.DesktopCoordinates;
                }
                else
                {
                    // Fallback to EnumDisplaySettings implementation
                    throw HrException(S_FALSE);
                }
            }
            catch (HrException& e)
            {
                UNREFERENCED_PARAMETER(e);

                // Get the settings of the primary display
                DEVMODE devMode = {};
                devMode.dmSize = sizeof(DEVMODE);
                EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &devMode);

                fullscreenWindowRect = {
                    devMode.dmPosition.x,
                    devMode.dmPosition.y,
                    devMode.dmPosition.x + static_cast<LONG>(devMode.dmPelsWidth),
                    devMode.dmPosition.y + static_cast<LONG>(devMode.dmPelsHeight)
                };
            }

            SetWindowPos(
                m_hwnd,
                HWND_TOPMOST,
                fullscreenWindowRect.left,
                fullscreenWindowRect.top,
                fullscreenWindowRect.right,
                fullscreenWindowRect.bottom,
                SWP_FRAMECHANGED | SWP_NOACTIVATE);


            ShowWindow(m_hwnd, SW_MAXIMIZE);
        }

        m_fullscreenMode = !m_fullscreenMode;
    }

    // Main message handler for the sample.
    LRESULT CALLBACK Application::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        DXCore* pSample = reinterpret_cast<DXCore*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

        switch (message)
        {
        case WM_CREATE:
        {
            // Save the DXSample* passed in to CreateWindow.
            LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
            SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
        }
        return 0;

        case WM_KEYDOWN:
            if (pSample)
            {
                pSample->OnKeyDown(static_cast<UINT8>(wParam));
            }
            return 0;

        case WM_KEYUP:
            if (pSample)
            {
                pSample->OnKeyUp(static_cast<UINT8>(wParam));
            }
            return 0;

        case WM_SYSKEYDOWN:
            // Handle ALT+ENTER:
            if ((wParam == VK_RETURN) && (lParam & (1 << 29)))
            {
                if (pSample && pSample->GetDeviceResources()->IsTearingSupported())
                {
                    ToggleFullscreenWindow(pSample->GetSwapchain());
                    return 0;
                }
            }
            // Send all other WM_SYSKEYDOWN messages to the default WndProc.
            break;

        case WM_PAINT:
            if (pSample)
            {
                pSample->OnUpdate();
                pSample->OnRender();
            }
            return 0;

        case WM_SIZE:
            if (pSample)
            {
                RECT windowRect = {};
                GetWindowRect(hWnd, &windowRect);
                pSample->SetWindowBounds(windowRect.left, windowRect.top, windowRect.right, windowRect.bottom);

                RECT clientRect = {};
                GetClientRect(hWnd, &clientRect);
                pSample->OnSizeChanged(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top, wParam == SIZE_MINIMIZED);
            }
            return 0;

        case WM_MOVE:
            if (pSample)
            {
                RECT windowRect = {};
                GetWindowRect(hWnd, &windowRect);
                pSample->SetWindowBounds(windowRect.left, windowRect.top, windowRect.right, windowRect.bottom);

                int xPos = (int)(short)LOWORD(lParam);
                int yPos = (int)(short)HIWORD(lParam);
                pSample->OnWindowMoved(xPos, yPos);
            }
            return 0;

        case WM_DISPLAYCHANGE:
            if (pSample)
            {
                pSample->OnDisplayChanged();
            }
            return 0;

        case WM_MOUSEMOVE:
            if (pSample && static_cast<UINT8>(wParam) == MK_LBUTTON)
            {
                UINT x = LOWORD(lParam);
                UINT y = HIWORD(lParam);
                pSample->OnMouseMove(x, y);
            }
            return 0;

        case WM_LBUTTONDOWN:
        {
            UINT x = LOWORD(lParam);
            UINT y = HIWORD(lParam);
            pSample->OnLeftButtonDown(x, y);
        }
        return 0;

        case WM_LBUTTONUP:
        {
            UINT x = LOWORD(lParam);
            UINT y = HIWORD(lParam);
            pSample->OnLeftButtonUp(x, y);
        }
        return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }

        // Handle any messages the switch statement didn't.
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
}