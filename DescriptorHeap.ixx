module;
#include "d3dx12.h"
#include <wrl/client.h>
#include <d3d12.h>
export module DescriptorHeap;

import DXSampleHelper;
import DescriptorHeapInterface;

using namespace Microsoft::WRL;

#define NAME_D3D12_OBJECT(x) SetName((x).Get(), L#x)
#define NAME_D3D12_OBJECT_INDEXED(x, n) SetNameIndexed((x)[n].Get(), L#x, n)

export class DescriptorHeap : public DescriptorHeapInterface
{
    ComPtr<ID3D12DescriptorHeap> m_descriptorHeap;
    UINT m_descriptorsAllocated;
    UINT m_descriptorSize;

public:
    D3D12_DESCRIPTOR_HEAP_DESC GetDesc() {
        return m_descriptorHeap->GetDesc();
    }
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandleForHeapStart()
    {
        return m_descriptorHeap->GetGPUDescriptorHandleForHeapStart();
    }
    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandleForHeapStart()
    {
        return m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    }

    DescriptorHeap() {}

    DescriptorHeap(ID3D12Device5* device, UINT numDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE type)
    {
        m_descriptorsAllocated = 0;

        D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
        descriptorHeapDesc.NumDescriptors = numDescriptors;
        descriptorHeapDesc.Type = type;
        descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        descriptorHeapDesc.NodeMask = 0;
        device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&m_descriptorHeap));
        NAME_D3D12_OBJECT(m_descriptorHeap);

        m_descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }

    ID3D12DescriptorHeap* GetHeap() { return m_descriptorHeap.Get(); }
    ID3D12DescriptorHeap** GetAddressOf() { return m_descriptorHeap.GetAddressOf(); }
    UINT DescriptorSize() { return m_descriptorSize; }

    UINT AllocateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* cpuDescriptor, UINT descriptorIndexToUse = UINT_MAX)
    {
        if (descriptorIndexToUse == UINT_MAX)
        {
            ThrowIfFalse(m_descriptorsAllocated < m_descriptorHeap->GetDesc().NumDescriptors, L"Ran out of descriptors on the heap!");
            descriptorIndexToUse = m_descriptorsAllocated++;
        }
        else
        {
            ThrowIfFalse(descriptorIndexToUse < m_descriptorHeap->GetDesc().NumDescriptors, L"Requested descriptor index is out of bounds!");
            m_descriptorsAllocated = max(descriptorIndexToUse + 1, m_descriptorsAllocated);
        }

        auto descriptorHeapCpuBase = m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();
        *cpuDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(descriptorHeapCpuBase, descriptorIndexToUse, m_descriptorSize);
        return descriptorIndexToUse;
    }

    UINT AllocateDescriptorIndices(UINT numDescriptors, UINT firstDescriptorIndexToUse = UINT_MAX)
    {
        auto handle = D3D12_CPU_DESCRIPTOR_HANDLE();
        firstDescriptorIndexToUse = AllocateDescriptor(&handle, firstDescriptorIndexToUse);

        for (UINT i = 1; i < numDescriptors; i++)
        {
            AllocateDescriptor(&handle, firstDescriptorIndexToUse + i);
        }
        return firstDescriptorIndexToUse;
    }
};