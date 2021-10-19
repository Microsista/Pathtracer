module;
#include <wrl/client.h>
#include <d3d12.h>
export module D3DTexture;

using namespace Microsoft::WRL;

export struct D3DTexture
{
    ComPtr<ID3D12Resource> resource;
    ComPtr<ID3D12Resource> upload;      // TODO: release after initialization
    D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptorHandle;
    UINT heapIndex = UINT_MAX;
};