module;
#include <d3d12.h>
#include <DirectXMath.h>
#include "RayTracingHlslCompat.hlsli"
export module RenderingComponent;

import DeviceResourcesInterface;
import PerformanceTimers;
import RaytracingSceneDefines;
import Descriptors;
import DescriptorHeap;
import Camera;
import ConstantBuffer;
import StructuredBuffer;

using namespace DirectX;

export class RenderingComponent {
    DeviceResourcesInterface* deviceResources;

    ID3D12Resource* rayGenShaderTable;
    ID3D12Resource* hitGroupShaderTable;
    ID3D12Resource* missShaderTable;

    UINT hitGroupShaderTableStrideInBytes;
    UINT missShaderTableStrideInBytes;

    UINT width;
    UINT height;

    DX::GPUTimer* gpuTimers;
    D3D12_GPU_DESCRIPTOR_HANDLE* descriptors;
    DescriptorHeap* descriptorHeap;
    Camera* camera;
    ID3D12RootSignature* raytracingGlobalRootSignature;
    ID3D12Resource* topLevelAS;
    ID3D12GraphicsCommandList5* dxrCommandList;
    ID3D12StateObject* dxrStateObject;
    ConstantBuffer<SceneConstantBuffer>* sceneCB;
    StructuredBuffer<PrimitiveInstancePerFrameBuffer>* trianglePrimitiveAttributeBuffer;

public:
    RenderingComponent(
        DeviceResourcesInterface* deviceResources,
        ID3D12Resource* hitGroupShaderTable,
        ID3D12Resource* missShaderTable,
        ID3D12Resource* rayGenShaderTable,
        UINT hitGroupShaderTableStrideInBytes,
        UINT missShaderTableStrideInBytes,
        UINT width,
        UINT height,
        DX::GPUTimer* gpuTimers,
        D3D12_GPU_DESCRIPTOR_HANDLE* descriptors,
        DescriptorHeap* descriptorHeap,
        Camera* camera,
        ID3D12RootSignature* raytracingGlobalRootSignature,
        ID3D12Resource* topLevelAS,
        ID3D12GraphicsCommandList5* dxrCommandList,
        ID3D12StateObject* dxrStateObject,
        ConstantBuffer<SceneConstantBuffer>* sceneCB,
        StructuredBuffer<PrimitiveInstancePerFrameBuffer>* trianglePrimitiveAttributeBuffer
    ) :
        deviceResources{ deviceResources },
        hitGroupShaderTable{ hitGroupShaderTable },
        rayGenShaderTable{ rayGenShaderTable },
        missShaderTable{ missShaderTable },
        hitGroupShaderTableStrideInBytes{ hitGroupShaderTableStrideInBytes },
        missShaderTableStrideInBytes{ missShaderTableStrideInBytes },
        width{ width },
        height{ height },
        gpuTimers{ gpuTimers },
        descriptors{ descriptors },
        descriptorHeap{ descriptorHeap },
        camera{ camera },
        raytracingGlobalRootSignature{ raytracingGlobalRootSignature },
        topLevelAS{ topLevelAS },
        dxrCommandList{ dxrCommandList },
        dxrStateObject{ dxrStateObject },
        sceneCB{ sceneCB },
        trianglePrimitiveAttributeBuffer{ trianglePrimitiveAttributeBuffer }
    {}

    void DoRaytracing() {
        auto commandList = deviceResources->GetCommandList();

        commandList->SetComputeRootSignature(raytracingGlobalRootSignature);

        XMFLOAT3 tempEye;
        XMStoreFloat3(&tempEye, camera->GetPosition());

        copyDynamicBuffersToGpu(commandList);

        // Bind the heaps, acceleration structure and dispatch rays.  
        D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
        SetCommonPipelineState(commandList);
        commandList->SetComputeRootShaderResourceView(GlobalRootSignature::Slot::AccelerationStructure, topLevelAS->GetGPUVirtualAddress());
        DispatchRays(dxrCommandList, dxrStateObject, &dispatchDesc, commandList);
        (*sceneCB)->frameIndex = ((*sceneCB)->frameIndex + 1) % 16;

        XMStoreFloat3(&(*sceneCB)->prevFrameCameraPosition, camera->GetPosition());
        XMMATRIX prevViewCameraAtOrigin = XMMatrixLookAtLH(XMVectorSet(0, 0, 0, 1), XMVectorSetW(camera->GetLook() - camera->GetPosition(), 1), camera->GetUp());
        XMMATRIX prevView, prevProj;
        prevView = camera->GetView();
        prevProj = camera->GetProj();
        XMMATRIX viewProjCameraAtOrigin = prevViewCameraAtOrigin * prevProj;
        (*sceneCB)->prevFrameProjToViewCameraAtOrigin = XMMatrixInverse(nullptr, viewProjCameraAtOrigin);
    }

    void copyDynamicBuffersToGpu(auto commandList) {
        auto frameIndex = deviceResources->GetCurrentFrameIndex();

        sceneCB->CopyStagingToGpu(frameIndex);
        commandList->SetComputeRootConstantBufferView(GlobalRootSignature::Slot::SceneConstant, sceneCB->GpuVirtualAddress(frameIndex));

        trianglePrimitiveAttributeBuffer->CopyStagingToGpu(frameIndex);
        commandList->SetComputeRootShaderResourceView(GlobalRootSignature::Slot::TriangleAttributeBuffer, trianglePrimitiveAttributeBuffer->GpuVirtualAddress(frameIndex));
    }

    void DispatchRays(auto* raytracingCommandList, auto* stateObject, auto* dispatchDesc, auto* commandList) {
        dispatchDesc->HitGroupTable.StartAddress = hitGroupShaderTable->GetGPUVirtualAddress();
        dispatchDesc->HitGroupTable.SizeInBytes = hitGroupShaderTable->GetDesc().Width;
        dispatchDesc->HitGroupTable.StrideInBytes = hitGroupShaderTableStrideInBytes;
        dispatchDesc->MissShaderTable.StartAddress = missShaderTable->GetGPUVirtualAddress();
        dispatchDesc->MissShaderTable.SizeInBytes = missShaderTable->GetDesc().Width;
        dispatchDesc->MissShaderTable.StrideInBytes = missShaderTableStrideInBytes;
        dispatchDesc->RayGenerationShaderRecord.StartAddress = rayGenShaderTable->GetGPUVirtualAddress();
        dispatchDesc->RayGenerationShaderRecord.SizeInBytes = rayGenShaderTable->GetDesc().Width;
        dispatchDesc->Width = width;
        dispatchDesc->Height = height;
        dispatchDesc->Depth = 1;
        raytracingCommandList->SetPipelineState1(stateObject);

        gpuTimers[GpuTimers::Raytracing].Start(commandList);
        raytracingCommandList->DispatchRays(dispatchDesc);
        gpuTimers[GpuTimers::Raytracing].Stop(commandList);
    };

    void SetCommonPipelineState(auto* descriptorSetCommandList) {
        descriptorSetCommandList->SetDescriptorHeaps(1, descriptorHeap->GetAddressOf());

        // Set index and successive vertex buffer decriptor tables.
        descriptorSetCommandList->SetComputeRootDescriptorTable(GlobalRootSignature::Slot::OutputView, descriptors[RAYTRACING]);
        descriptorSetCommandList->SetComputeRootDescriptorTable(GlobalRootSignature::Slot::ReflectionBuffer, descriptors[REFLECTION]);
        descriptorSetCommandList->SetComputeRootDescriptorTable(GlobalRootSignature::Slot::ShadowBuffer, descriptors[SHADOW]);
        descriptorSetCommandList->SetComputeRootDescriptorTable(GlobalRootSignature::Slot::NormalDepth, descriptors[NORMAL_DEPTH]);
        descriptorSetCommandList->SetComputeRootDescriptorTable(GlobalRootSignature::Slot::MotionVector, descriptors[MOTION_VECTOR]);
        descriptorSetCommandList->SetComputeRootDescriptorTable(GlobalRootSignature::Slot::PrevHitPosition, descriptors[PREV_HIT]);
    };
};