#pragma once

#include <ResourceUploadBatch.h>

#include "GpuUploadBufferC.h"

export template <class T>
class ConstantBufferC : public GpuUploadBufferC {
    uint8_t* m_mappedConstantData;
    UINT m_alignedInstanceSize;
    UINT m_numInstances;

public:
    ConstantBufferC();

    void Create(ID3D12Device* device, UINT numInstances = 1, LPCWSTR resourceName = nullptr);

    void CopyStagingToGpu(UINT instanceIndex = 0);

    // Accessors
    T staging;
    T* operator->() { return &staging; }
    UINT NumInstances() { return m_numInstances; }
    D3D12_GPU_VIRTUAL_ADDRESS GpuVirtualAddress(UINT instanceIndex = 0)
    {
        return m_resource->GetGPUVirtualAddress() + instanceIndex * m_alignedInstanceSize;
    }
};