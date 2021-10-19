module;
#include <wrl/client.h>
#include <d3d12.h>
export module D3DBuffer;

using namespace Microsoft::WRL;

export struct D3DBuffer
{
    ComPtr<ID3D12Resource> resource;
    D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptorHandle;
};