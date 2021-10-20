module;
#include <d3d12.h>
#include "d3dx12.h"
#include <dxgi1_6.h>
export module DeviceResourcesInterface;

import IDeviceNotify;

export struct DeviceResourcesInterface {
    virtual void SetAdapterOverride(UINT adapterID) = 0;
    virtual void InitializeDXGIAdapter() = 0;
    virtual void CreateDeviceResources() = 0;
    virtual void CreateWindowSizeDependentResources() = 0;
    virtual void SetWindow(HWND window, int width, int height) = 0;
    virtual bool WindowSizeChanged(int width, int height, bool minimized) = 0;
    virtual void HandleDeviceLost() = 0;
    virtual void RegisterDeviceNotify(IDeviceNotify* deviceNotify) = 0;
    virtual void Prepare(D3D12_RESOURCE_STATES beforeState = D3D12_RESOURCE_STATE_PRESENT) = 0;
    virtual void Present(D3D12_RESOURCE_STATES beforeState) = 0;
    virtual void ExecuteCommandList() = 0;
    virtual void WaitForGpu() = 0;
    virtual RECT GetOutputSize() const = 0;
    virtual bool IsWindowVisible() const = 0;
    virtual bool IsTearingSupported() const = 0;
    virtual IDXGIAdapter1* GetAdapter() const = 0;
    virtual ID3D12Device5* GetD3DDevice() const = 0;
    virtual IDXGIFactory4* GetDXGIFactory() const = 0;
    virtual IDXGISwapChain3* GetSwapChain() const = 0;
    virtual D3D_FEATURE_LEVEL GetDeviceFeatureLevel() const = 0;
    virtual ID3D12Resource* GetRenderTarget() const = 0;
    virtual ID3D12Resource* GetDepthStencil() const = 0;
    virtual ID3D12CommandQueue* GetCommandQueue() const = 0;
    virtual ID3D12CommandAllocator* GetCommandAllocator() const = 0;
    virtual ID3D12GraphicsCommandList5* GetCommandList() const = 0;
    virtual DXGI_FORMAT GetBackBufferFormat() const = 0;
    virtual DXGI_FORMAT GetDepthBufferFormat() const = 0;
    virtual D3D12_VIEWPORT GetScreenViewport() const = 0;
    virtual D3D12_RECT GetScissorRect() const = 0;
    virtual UINT GetCurrentFrameIndex() const = 0;
    virtual UINT GetPreviousFrameIndex() const = 0;
    virtual UINT GetBackBufferCount() const = 0;
    virtual unsigned int GetDeviceOptions() const = 0;
    virtual LPCWSTR GetAdapterDescription() const = 0;
    virtual UINT GetAdapterID() const = 0;
    virtual CD3DX12_CPU_DESCRIPTOR_HANDLE GetRenderTargetView() const = 0;
    virtual CD3DX12_CPU_DESCRIPTOR_HANDLE GetDepthStencilView() const = 0;
    virtual inline DXGI_FORMAT NoSRGB(DXGI_FORMAT fmt) = 0;
    virtual void MoveToNextFrame() = 0;
    virtual void InitializeAdapter(IDXGIAdapter1** ppAdapter) = 0;
};