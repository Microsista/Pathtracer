module;
#include <stdint.h>
#include <dxgi1_6.h>
#include <d3d12.h>
#include "d3dx12.h"
#include <unordered_map>
#include <string>
#include <memory>
export module SrvComponent;

import DescriptorHeap;
import DeviceResourcesInterface;
import D3DBuffer;
import Texture;
import DXSampleHelper;
import D3DTexture;
import DescriptorHeapInterface;

using namespace std;

export class SrvComponent {
    UINT& descriptorSize;
    shared_ptr<DeviceResourcesInterface>& deviceResources;
    unordered_map<string, unique_ptr<Texture>>& textures;
    DescriptorHeap*& descriptorHeap;

public:
    SrvComponent(
        UINT& descriptorSize,
        shared_ptr<DeviceResourcesInterface>& deviceResources,
        unordered_map<string, unique_ptr<Texture>>& textures,
        DescriptorHeap*& descriptorHeap
    ) :
        descriptorSize{ descriptorSize },
        deviceResources{ deviceResources },
        textures{ textures },
        descriptorHeap{ descriptorHeap }
    {}

    UINT CreateBufferSRV(D3DBuffer* buffer, UINT numElements, UINT elementSize) {
        auto device = deviceResources->GetD3DDevice();

        // SRV
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Buffer.NumElements = numElements;
        if (elementSize == 0)
        {
            srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
            srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
            srvDesc.Buffer.StructureByteStride = 0;
        }
        else
        {
            srvDesc.Format = DXGI_FORMAT_UNKNOWN;
            srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
            srvDesc.Buffer.StructureByteStride = elementSize;
        }
        UINT descriptorIndex = descriptorHeap->AllocateDescriptor(&buffer->cpuDescriptorHandle);
        device->CreateShaderResourceView(buffer->resource.Get(), &srvDesc, buffer->cpuDescriptorHandle);
        buffer->gpuDescriptorHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(descriptorHeap->GetGPUDescriptorHandleForHeapStart(), descriptorIndex, descriptorSize);
        return descriptorIndex;
    }

    UINT CreateTextureSRV(UINT numElements, UINT elementSize) {
        auto device = deviceResources->GetD3DDevice();

        auto srvDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, 512, 512, 1, 1, 1, 0);

        auto defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        ThrowIfFailed(device->CreateCommittedResource(
            &defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &srvDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&textures["stoneTex"].get()->Resource)));

        D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
        SRVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        SRVDesc.Texture2D.MostDetailedMip = 0;
        SRVDesc.Texture2D.MipLevels = 1;
        SRVDesc.Texture2D.PlaneSlice = 0;
        SRVDesc.Texture2D.ResourceMinLODClamp = 0.0f;

        CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(descriptorHeap->GetCPUDescriptorHandleForHeapStart());

        UINT descriptorIndex = descriptorHeap->AllocateDescriptor(&hDescriptor);
        device->CreateShaderResourceView(textures["stoneTex"].get()->Resource.Get(), &SRVDesc, hDescriptor);
        //texture->gpuDescriptorHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(descriptorHeap->GetGPUDescriptorHandleForHeapStart(), descriptorIndex, descriptorSize);
        return descriptorIndex;
    }
};