module;
#include <wrl/client.h>
#include <d3d12.h>
export module AccelerationStructureBuffers;

using namespace Microsoft::WRL;

export struct AccelerationStructureBuffers
{
    ComPtr<ID3D12Resource> scratch;
    ComPtr<ID3D12Resource> accelerationStructure;
    ComPtr<ID3D12Resource> instanceDesc;
    UINT64 ResultDataMaxSizeInBytes;
};