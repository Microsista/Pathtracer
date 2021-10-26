module;
#include "d3dx12.h"
#include <d3d12.h>
#include <algorithm>
#include <wrl/client.h>
#include <vector>

export module OutputComponent;

import DeviceResourcesInterface;
import Descriptors;
import DescriptorHeap;
import DXSampleHelper;

using namespace std;
using namespace Microsoft::WRL;


export class OutputComponent {
    DeviceResourcesInterface* deviceResources;
    UINT& width;
    UINT& height;
    DescriptorHeap*& descriptorHeap;
    vector<D3D12_GPU_DESCRIPTOR_HANDLE>& descriptors;
    vector<ComPtr<ID3D12Resource>>& buffers;
    UINT& descriptorSize;

public:
    OutputComponent(
        DeviceResourcesInterface* deviceResources,
        UINT& width,
        UINT& height,
        DescriptorHeap*& descriptorHeap,
        vector<D3D12_GPU_DESCRIPTOR_HANDLE>& descriptors,
        vector<ComPtr<ID3D12Resource>>& buffers,
        UINT& descriptorSize
    ) :
        deviceResources{ deviceResources },
        width{ width },
        height{ height },
        descriptorHeap{ descriptorHeap },
        descriptors{ descriptors },
        buffers{ buffers },
        descriptorSize{ descriptorSize }
    {}

    void CreateRaytracingOutputResource() {
        auto device = deviceResources->GetD3DDevice();
        auto backbufferFormat = deviceResources->GetBackBufferFormat();

        auto uavDesc = CD3DX12_RESOURCE_DESC::Tex2D(backbufferFormat, width, height, 1, 1, 1, 0,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        auto uavDesc2 = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_SINT, width, height, 1, 1, 1, 0,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        auto defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

        UINT heapIndices[Descriptors::COUNT];
        fill_n(heapIndices, Descriptors::COUNT, UINT_MAX);

        for (int i = 0; i < 12; i++) {
            ThrowIfFailed(device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, i == Descriptors::MOTION_VECTOR ?
                &uavDesc2 : &uavDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&buffers[i])));

            D3D12_CPU_DESCRIPTOR_HANDLE uavDescriptorHandle;
            heapIndices[i] = descriptorHeap->AllocateDescriptor(&uavDescriptorHandle, heapIndices[i]);
            D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
            UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            device->CreateUnorderedAccessView(buffers[i].Get(), nullptr, &UAVDesc, uavDescriptorHandle);
            descriptors[i] = CD3DX12_GPU_DESCRIPTOR_HANDLE(descriptorHeap->GetGPUDescriptorHandleForHeapStart(), heapIndices[i],
                descriptorSize);
        }
    }


    void CopyRaytracingOutputToBackbuffer() {
        auto commandList = deviceResources->GetCommandList();
        auto renderTarget = deviceResources->GetRenderTarget();

        auto buffer = buffers[Descriptors::RAYTRACING].Get();

        D3D12_RESOURCE_BARRIER preCopyBarriers[2];
        preCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(renderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);
        preCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(buffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
        commandList->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);

        commandList->CopyResource(renderTarget, buffer);

        D3D12_RESOURCE_BARRIER postCopyBarriers[2];
        postCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(renderTarget, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);
        postCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(buffer, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        commandList->ResourceBarrier(ARRAYSIZE(postCopyBarriers), postCopyBarriers);
    }
};