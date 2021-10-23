module;
#include <d3d12.h>
#include <wrl/client.h>

#include "RaytracingSceneDefines.h"
export module TopLevelASComponent;

import AccelerationStructureBuffers;
import DeviceResources;
import DXSampleHelper;
import Helper;
import BottomLevelASComponent;

using namespace Microsoft::WRL;

export class TopLevelASComponent {
    DeviceResources* deviceResources;
    UINT NUM_BLAS;
    ID3D12Device5* dxrDevice;
    BottomLevelASComponent* bottomLevelASComponent;
    ID3D12GraphicsCommandList5* dxrCommandList;

public:
    TopLevelASComponent() {}

    AccelerationStructureBuffers BuildTopLevelAS(AccelerationStructureBuffers bottomLevelAS[BottomLevelASType::Count], D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE)
    {
        auto device = deviceResources->GetD3DDevice();
        auto commandList = deviceResources->GetCommandList();
        ComPtr<ID3D12Resource> scratch;
        ComPtr<ID3D12Resource> topLevelAS;

        // Get required sizes for an acceleration structure.
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelBuildDesc = {};
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& topLevelInputs = topLevelBuildDesc.Inputs;
        topLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
        topLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        topLevelInputs.Flags = buildFlags;
        topLevelInputs.NumDescs = NUM_BLAS;

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topLevelPrebuildInfo = {};
        dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&topLevelInputs, &topLevelPrebuildInfo);
        ThrowIfFalse(topLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

        AllocateUAVBuffer(device, topLevelPrebuildInfo.ScratchDataSizeInBytes, &scratch, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");
        {
            D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
            AllocateUAVBuffer(device, topLevelPrebuildInfo.ResultDataMaxSizeInBytes, &topLevelAS, initialResourceState, L"TopLevelAccelerationStructure");
        }

        // Create instance descs for the bottom-level acceleration structures.
        ComPtr<ID3D12Resource> instanceDescsResource;
        {
            D3D12_RAYTRACING_INSTANCE_DESC instanceDescs[BottomLevelASType::Count] = {};
            D3D12_GPU_VIRTUAL_ADDRESS bottomLevelASaddresses[BottomLevelASType::Count] =
            {
                bottomLevelAS[0].accelerationStructure->GetGPUVirtualAddress(),
                bottomLevelAS[1].accelerationStructure->GetGPUVirtualAddress(),
                bottomLevelAS[2].accelerationStructure->GetGPUVirtualAddress(),
                bottomLevelAS[3].accelerationStructure->GetGPUVirtualAddress(),
                bottomLevelAS[4].accelerationStructure->GetGPUVirtualAddress(),
                bottomLevelAS[5].accelerationStructure->GetGPUVirtualAddress(),
            };
            bottomLevelASComponent->BuildBottomLevelASInstanceDescs<D3D12_RAYTRACING_INSTANCE_DESC>(bottomLevelASaddresses, &instanceDescsResource);
        }

        // Top-level AS desc
        {
            topLevelBuildDesc.DestAccelerationStructureData = topLevelAS->GetGPUVirtualAddress();
            topLevelInputs.InstanceDescs = instanceDescsResource->GetGPUVirtualAddress();
            topLevelBuildDesc.ScratchAccelerationStructureData = scratch->GetGPUVirtualAddress();
        }

        // Build acceleration structure.
        dxrCommandList->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);

        AccelerationStructureBuffers topLevelASBuffers;
        topLevelASBuffers.accelerationStructure = topLevelAS;
        topLevelASBuffers.instanceDesc = instanceDescsResource;
        topLevelASBuffers.scratch = scratch;
        topLevelASBuffers.ResultDataMaxSizeInBytes = topLevelPrebuildInfo.ResultDataMaxSizeInBytes;
        return topLevelASBuffers;
    }
};