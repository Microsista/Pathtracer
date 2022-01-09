module;
#include <Windows.h>
#include <dxgi1_6.h>
#include <string>
#include <wrl/client.h>
#include <memory>
#include <DirectXMath.h>
export module DXCore;

import Camera;
import DXSampleHelper;
import DXCoreInterface;
import DeviceResourcesInterface;
import IDeviceNotify;

using namespace Microsoft::WRL;
using namespace std;
using Microsoft::WRL::ComPtr;

export class DXCore : public DXCoreInterface, public IDeviceNotify
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
    virtual void OnKeyDown(UINT8 key) {}
    virtual void OnKeyUp(UINT8 key) {}
    virtual void OnWindowMoved(int x, int y) {}
    virtual void OnMouseMove(int x, int y) {}
    virtual void OnLeftButtonDown(UINT x, UINT y) {}
    virtual void OnLeftButtonUp(UINT x, UINT y) {}
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
    DeviceResourcesInterface* GetDeviceResources() const { return m_deviceResources.get(); }

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
    void SetCustomWindowText(LPCWSTR text, HWND hwnd)
    {
        std::wstring windowText = m_title + L": " + text;
        SetWindowText(hwnd, windowText.c_str());
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
    std::shared_ptr<DeviceResourcesInterface> m_deviceResources;

    // Camera
    Camera m_camera;

private:
    // Root assets path.
    std::wstring m_assetsPath;

    // Window title.
    std::wstring m_title;
};