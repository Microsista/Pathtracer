module;
#include <Windows.h>
#include <memory>
#include <d3d12.h>
export module DescriptorComponent;

import DescriptorHeap;
import DeviceResources;
import DXSampleHelper;

using namespace std;

export class DescriptorComponent {
    DeviceResources* deviceResources;
    shared_ptr<DescriptorHeap> descriptorHeap;
    UINT descriptorSize;
    UINT descriptorsAllocated;

public:
    DescriptorComponent(
        DeviceResources* deviceResources,
        shared_ptr<DescriptorHeap> descriptorHeap,
        UINT descriptorSize,
        UINT descriptorsAllocated)
        :
        deviceResources{ deviceResources },
        descriptorHeap{ descriptorHeap },
        descriptorSize{ descriptorSize },
        descriptorsAllocated{ descriptorsAllocated }
    {}

    void CreateDescriptorHeap() {
        auto device = deviceResources->GetD3DDevice();

        descriptorHeap = std::make_shared<DescriptorHeap>(device, 10000, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        descriptorSize = descriptorHeap->DescriptorSize();
    }
};