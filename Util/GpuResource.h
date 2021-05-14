#pragma once

class GpuResource
{
public:
    enum RWFlags {
        None = 0x0,
        AllowRead = 0x1,
        AllowWrite = 0x2,
    };

    UINT rwFlags = RWFlags::AllowRead | RWFlags::AllowWrite;
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptorReadAccess = { UINT64_MAX };
    D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptorWriteAccess = { UINT64_MAX };
    UINT srvDescriptorHeapIndex = UINT_MAX;
    UINT uavDescriptorHeapIndex = UINT_MAX;
    D3D12_RESOURCE_STATES m_UsageState = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES m_TransitioningState = (D3D12_RESOURCE_STATES)-1;

    ID3D12Resource* GetResource() { return resource.Get(); }
};
