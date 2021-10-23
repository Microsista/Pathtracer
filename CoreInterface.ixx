module;
#include <dxgi1_6.h>
#include <Windows.h>
export module CoreInterface;

export struct CoreInterface {
    virtual void OnInit() = 0;
    virtual void OnUpdate() = 0;
    virtual void OnRender() = 0;
    virtual void OnSizeChanged(UINT width, UINT height, bool minimized) = 0;
    virtual void OnDestroy() = 0;
    virtual void OnMouseMove(int x, int y) = 0;
    virtual void OnLeftButtonDown(UINT x, UINT y) = 0;
    virtual IDXGISwapChain* GetSwapchain() = 0;
    virtual void OnDeviceLost() = 0;
    virtual void OnDeviceRestored() = 0;
};