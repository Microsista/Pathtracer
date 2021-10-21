module;
#include <Windows.h>
export module UpdateInterface;

export struct UpdateInterface {
    virtual void OnUpdate() = 0;
    virtual void OnDeviceLost() = 0;
    virtual void OnDeviceRestored() = 0;
    virtual void OnSizeChanged(UINT width, UINT height, bool minimized) = 0;
    virtual void RecreateD3D() = 0;
    virtual void UpdateForSizeChange(UINT width, UINT height) = 0;
};