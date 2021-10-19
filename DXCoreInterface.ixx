module;
#include <Windows.h>
#include <string>
#include <dxgi1_6.h>
export module DXCoreInterface;

import DeviceResourcesInterface;

export struct DXCoreInterface {
	virtual void OnInit() = 0;
	virtual void OnUpdate() = 0;
	virtual void OnRender() = 0;
	virtual void OnSizeChanged(UINT width, UINT height, bool minimized) = 0;
	virtual void OnDestroy() = 0;

	virtual void OnKeyDown(UINT8 key) = 0;
	virtual void OnKeyUp(UINT8 key) = 0;
	virtual void OnWindowMoved(int x, int y) = 0;
	virtual void OnMouseMove(int x, int y) = 0;
	virtual void OnLeftButtonDown(UINT x, UINT y) = 0;
	virtual void OnLeftButtonUp(UINT x, UINT y) = 0;
	virtual void OnDisplayChanged() = 0;

	virtual void ParseCommandLineArgs(_In_reads_(argc) WCHAR* argv[], int argc) = 0;

	virtual UINT GetWidth() const = 0;
	virtual UINT GetHeight() const = 0;
	virtual const WCHAR* GetTitle() const = 0;
	virtual RECT GetWindowsBounds() const = 0;
	virtual IDXGISwapChain* GetSwapchain() = 0;
	virtual DeviceResourcesInterface* GetDeviceResources() const = 0;

	virtual void UpdateForSizeChange(UINT clientWidth, UINT clientHeight) = 0;
	virtual void SetWindowBounds(int left, int top, int right, int bottom) = 0;
	virtual std::wstring GetAssetFullPath(LPCWSTR assetName) = 0;

	virtual void SetCustomWindowText(LPCWSTR text) = 0;

	virtual void OnDeviceLost() = 0;
	virtual void OnDeviceRestored() = 0;
};