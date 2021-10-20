#pragma once

#include <wrl/client.h>
#include <d3d12.h>

class AccelerationStructureBuffersC
{
    Microsoft::WRL::ComPtr<ID3D12Resource> scratch;
    Microsoft::WRL::ComPtr<ID3D12Resource> accelerationStructure;
    Microsoft::WRL::ComPtr<ID3D12Resource> instanceDesc;
    UINT64 ResultDataMaxSizeInBytes;
};