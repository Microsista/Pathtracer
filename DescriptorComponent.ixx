module;
#include <Windows.h>
#include <memory>
#include <d3d12.h>
export module DescriptorComponent;

import DescriptorHeap;
import DeviceResourcesInterface;
import DXSampleHelper;

using namespace std;

export class DescriptorComponent {
    DeviceResourcesInterface* deviceResources;
    shared_ptr<DescriptorHeap> descriptorHeap;
    UINT descriptorSize;
    UINT descriptorsAllocated;

public:
    DescriptorComponent(
        DeviceResourcesInterface* deviceResources,
        shared_ptr<DescriptorHeap> descriptorHeap,
        UINT descriptorSize,
        UINT descriptorsAllocated)
        :
        deviceResources{ deviceResources },
        descriptorHeap{ descriptorHeap },
        descriptorSize{ descriptorSize },
        descriptorsAllocated{ descriptorsAllocated }
    {}

   
};