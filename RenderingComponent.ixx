module;
#include <d3d12.h>
#include <DirectXMath.h>
#include "RayTracingHlslCompat.hlsli"
#include <vector>
#include "d3dx12.h"
#include <wrl/client.h>

#include "Obj/Debug/CompiledShaders/ViewRG.hlsl.h"
#include "Obj/Debug/CompiledShaders/RadianceCH.hlsl.h"
#include "Obj/Debug/CompiledShaders/RadianceMS.hlsl.h"
#include "Obj/Debug/CompiledShaders/ShadowMS.hlsl.h"
#include "Obj/Debug/CompiledShaders/CompositionCS.hlsl.h"
#include "Obj/Debug/CompiledShaders/BlurCS.hlsl.h"

#include "RaytracingSceneDefines.h"
#include "PerformanceTimers.h"
export module RenderingComponent;

import DeviceResourcesInterface;

import Descriptors;
import DescriptorHeap;
import Camera;
import ConstantBuffer;
import StructuredBuffer;
import OutputComponent;
import RootSignatureComponent;
import DXSampleHelper;

using namespace DirectX;
using namespace std;
using namespace Microsoft::WRL;

export class RenderingComponent {
    shared_ptr<DeviceResourcesInterface>& deviceResources;
    ComPtr<ID3D12Resource>& rayGenShaderTable;
    ComPtr<ID3D12Resource>& hitGroupShaderTable;
    ComPtr<ID3D12Resource>& missShaderTable;
    UINT& hitGroupShaderTableStrideInBytes;
    UINT& missShaderTableStrideInBytes;
    UINT& width;
    UINT& height;
    vector<DX::GPUTimer>& gpuTimers;
    vector<D3D12_GPU_DESCRIPTOR_HANDLE>& descriptors;
    DescriptorHeap*& descriptorHeap;
    Camera& camera;
    ComPtr<ID3D12RootSignature>& raytracingGlobalRootSignature;
    ComPtr<ID3D12Resource>& topLevelAS;
    ComPtr<ID3D12GraphicsCommandList5>& dxrCommandList;
    ComPtr<ID3D12StateObject>& dxrStateObject;
    ConstantBuffer<SceneConstantBuffer>& sceneCB;
    StructuredBuffer<PrimitiveInstancePerFrameBuffer>& trianglePrimitiveAttributeBuffer;

    ConstantBuffer<AtrousWaveletTransformFilterConstantBuffer>& filterCB;
    vector<ComPtr<ID3D12Resource>>& buffers;
    OutputComponent*& outputComponent;
    ComPtr<ID3D12RootSignature>& blurRootSig;
    ComPtr<ID3D12RootSignature>& composeRootSig;
    vector<ComPtr<ID3D12PipelineState>>& composePSO;
    vector<ComPtr<ID3D12PipelineState>>& blurPSO;
    RootSignatureComponent*& rootSignatureComponent;

public:
    RenderingComponent(
        shared_ptr<DeviceResourcesInterface>& deviceResources,
        ComPtr<ID3D12Resource>& hitGroupShaderTable,
        ComPtr<ID3D12Resource>& missShaderTable,
        ComPtr<ID3D12Resource>& rayGenShaderTable,
        UINT& hitGroupShaderTableStrideInBytes,
        UINT& missShaderTableStrideInBytes,
        UINT& width,
        UINT& height,
        vector<DX::GPUTimer>& gpuTimers,
        vector<D3D12_GPU_DESCRIPTOR_HANDLE>& descriptors,

        DescriptorHeap*& descriptorHeap,
        Camera& camera,
        ComPtr<ID3D12RootSignature>& raytracingGlobalRootSignature,
        ComPtr<ID3D12Resource>& topLevelAS,
        ComPtr<ID3D12GraphicsCommandList5>& dxrCommandList,
        ComPtr<ID3D12StateObject>& dxrStateObject,
        ConstantBuffer<SceneConstantBuffer>& sceneCB,
        StructuredBuffer<PrimitiveInstancePerFrameBuffer>& trianglePrimitiveAttributeBuffer,
        vector<ComPtr<ID3D12PipelineState>>& composePSO,
        vector<ComPtr<ID3D12PipelineState>>& blurPSO,

        ConstantBuffer<AtrousWaveletTransformFilterConstantBuffer>& filterCB,
        vector<ComPtr<ID3D12Resource>>& buffers,
        OutputComponent*& outputComponent,
        ComPtr<ID3D12RootSignature>& blurRootSig,
        ComPtr<ID3D12RootSignature>& composeRootSig,
        RootSignatureComponent*& rootSignatureComponent
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
        trianglePrimitiveAttributeBuffer{ trianglePrimitiveAttributeBuffer },
        composePSO{ composePSO },
        blurPSO{ blurPSO },
        filterCB{filterCB},
        buffers{buffers},
        outputComponent{ outputComponent },
        blurRootSig{ blurRootSig },
        composeRootSig{ composeRootSig },
        rootSignatureComponent{ rootSignatureComponent }
    {}

    void DoRaytracing() {
        auto commandList = deviceResources->GetCommandList();

        commandList->SetComputeRootSignature(raytracingGlobalRootSignature.Get());

        XMFLOAT3 tempEye;
        XMStoreFloat3(&tempEye, camera.GetPosition());

        copyDynamicBuffersToGpu(commandList);

        // Bind the heaps, acceleration structure and dispatch rays.  
        D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
        SetCommonPipelineState(commandList);
        commandList->SetComputeRootShaderResourceView(GlobalRootSignature::
            Slot::AccelerationStructure, topLevelAS->GetGPUVirtualAddress());
        DispatchRays(dxrCommandList.Get(), dxrStateObject.Get(), &dispatchDesc, commandList);
        sceneCB->frameIndex = (sceneCB->frameIndex + 1) % 16;

        XMStoreFloat3(&sceneCB->prevFrameCameraPosition, camera.GetPosition());
        XMMATRIX prevViewCameraAtOrigin = XMMatrixLookAtLH(XMVectorSet(0, 0, 0, 1),
            XMVectorSetW(camera.GetLook() - camera.GetPosition(), 1), camera.GetUp());
        XMMATRIX prevView, prevProj;
        prevView = camera.GetView();
        prevProj = camera.GetProj();
        XMMATRIX viewProjCameraAtOrigin = prevViewCameraAtOrigin * prevProj;
        sceneCB->prevFrameProjToViewCameraAtOrigin = XMMatrixInverse(nullptr,
            viewProjCameraAtOrigin);
    }

    void copyDynamicBuffersToGpu(auto commandList) {
        auto frameIndex = deviceResources->GetCurrentFrameIndex();

        sceneCB.CopyStagingToGpu(frameIndex);
        commandList->SetComputeRootConstantBufferView(GlobalRootSignature::Slot::SceneConstant, sceneCB.GpuVirtualAddress(frameIndex));

        trianglePrimitiveAttributeBuffer.CopyStagingToGpu(frameIndex);
        commandList->SetComputeRootShaderResourceView(GlobalRootSignature
            ::Slot::TriangleAttributeBuffer,
            trianglePrimitiveAttributeBuffer.GpuVirtualAddress(frameIndex));
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
        descriptorSetCommandList->SetComputeRootDescriptorTable(GlobalRootSignature::Slot::OutputView, descriptors[Descriptors::RAYTRACING]);
        descriptorSetCommandList->SetComputeRootDescriptorTable(GlobalRootSignature::Slot::ReflectionBuffer, descriptors[Descriptors::REFLECTION]);
        descriptorSetCommandList->SetComputeRootDescriptorTable(GlobalRootSignature::Slot::ShadowBuffer, descriptors[Descriptors::SHADOW]);
        descriptorSetCommandList->SetComputeRootDescriptorTable(GlobalRootSignature::Slot::NormalDepth, descriptors[Descriptors::NORMAL_DEPTH]);
        descriptorSetCommandList->SetComputeRootDescriptorTable(GlobalRootSignature::Slot::MotionVector, descriptors[Descriptors::MOTION_VECTOR]);
        descriptorSetCommandList->SetComputeRootDescriptorTable(GlobalRootSignature::Slot::PrevHitPosition, descriptors[Descriptors::PREV_HIT]);
    };

    virtual void OnRender() {
        if (!deviceResources->IsWindowVisible())
        {
            return;
        }

        auto device = deviceResources->GetD3DDevice();
        auto commandList = deviceResources->GetCommandList();

        deviceResources->Prepare(D3D12_RESOURCE_STATE_PRESENT);
        for (auto& gpuTimer : gpuTimers)
        {
            gpuTimer.BeginFrame(commandList);
        }

        DoRaytracing();
        Compose();

        auto uav = CD3DX12_RESOURCE_BARRIER::UAV(buffers[Descriptors::PREV_FRAME].Get());
        dxrCommandList->ResourceBarrier(1, &uav);
        Blur();
      
        outputComponent->CopyRaytracingOutputToBackbuffer();

        for (auto& gpuTimer : gpuTimers)
        {
            gpuTimer.EndFrame(commandList);
        }

        deviceResources->Present(D3D12_RESOURCE_STATE_PRESENT);
    }

    virtual void Blur() {
        auto commandList = deviceResources->GetCommandList();
        auto device = deviceResources->GetD3DDevice();

        CD3DX12_DESCRIPTOR_RANGE ranges[8];
        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
        ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
        ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
        ranges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);

        ranges[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1);
        ranges[5].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 2);
        ranges[6].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 3);
        ranges[7].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 4);

        CD3DX12_ROOT_PARAMETER rootParameters[10];
        rootParameters[0].InitAsDescriptorTable(1, &ranges[0]);
        rootParameters[1].InitAsDescriptorTable(1, &ranges[1]);
        rootParameters[2].InitAsDescriptorTable(1, &ranges[2]);
        rootParameters[3].InitAsConstantBufferView(0, 0);
        rootParameters[4].InitAsDescriptorTable(1, &ranges[3]);
        rootParameters[5].InitAsDescriptorTable(1, &ranges[4]);
        rootParameters[6].InitAsDescriptorTable(1, &ranges[5]);
        rootParameters[7].InitAsDescriptorTable(1, &ranges[6]);
        rootParameters[8].InitAsDescriptorTable(1, &ranges[7]);
        rootParameters[9].InitAsConstantBufferView(1);

        CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters);
        rootSignatureComponent->SerializeAndCreateRaytracingRootSignature(device, rootSignatureDesc, blurRootSig, L"Root signature: BlurCS");

        // create pso
        D3D12_COMPUTE_PIPELINE_STATE_DESC descComputePSO = {};
        descComputePSO.pRootSignature = blurRootSig.Get();
        descComputePSO.CS = CD3DX12_SHADER_BYTECODE((void*)g_pBlurCS, ARRAYSIZE(g_pBlurCS));

        ThrowIfFailed(device->CreateComputePipelineState(&descComputePSO, IID_PPV_ARGS(&blurPSO[0])));
        blurPSO[0]->SetName(L"PSO: BlurCS");

        commandList->SetDescriptorHeaps(1, descriptorHeap->GetAddressOf());
        commandList->SetComputeRootSignature(blurRootSig.Get());
        commandList->SetPipelineState(blurPSO[0].Get());
        blurPSO[0]->AddRef();

        commandList->SetComputeRootDescriptorTable(0, descriptors[Descriptors::RAYTRACING]);
        commandList->SetComputeRootDescriptorTable(1, descriptors[Descriptors::REFLECTION]);
        commandList->SetComputeRootDescriptorTable(2, descriptors[Descriptors::SHADOW]);


        auto frameIndex = deviceResources->GetCurrentFrameIndex();
        filterCB.CopyStagingToGpu(frameIndex);
        commandList->SetComputeRootConstantBufferView(3, filterCB.GpuVirtualAddress(frameIndex));
        commandList->SetComputeRootDescriptorTable(4, descriptors[Descriptors::NORMAL_DEPTH]);
        commandList->SetComputeRootDescriptorTable(5, descriptors[Descriptors::PREV_FRAME]);
        commandList->SetComputeRootDescriptorTable(6, descriptors[Descriptors::PREV_REFLECTION]);
        commandList->SetComputeRootDescriptorTable(7, descriptors[Descriptors::PREV_SHADOW]);
        commandList->SetComputeRootDescriptorTable(8, descriptors[Descriptors::MOTION_VECTOR]);

        commandList->SetComputeRootConstantBufferView(9, sceneCB.GpuVirtualAddress(frameIndex));

        auto outputSize = deviceResources->GetOutputSize();
        commandList->Dispatch(outputSize.right / 8, outputSize.bottom / 8, 1);

        XMMATRIX prevView, prevProj;
        prevView = camera.GetView();
        prevProj = camera.GetProj();
        sceneCB->prevFrameViewProj = XMMatrixMultiply(prevView, prevProj);
    }

    virtual void Compose() {
        auto commandList = deviceResources->GetCommandList();
        auto device = deviceResources->GetD3DDevice();

        CD3DX12_DESCRIPTOR_RANGE ranges[8];
        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
        ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
        ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
        ranges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
        ranges[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1);
        ranges[5].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 2);
        ranges[6].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 3);
        ranges[7].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 4); // motion vector

        CD3DX12_ROOT_PARAMETER rootParameters[10];
        rootParameters[0].InitAsDescriptorTable(1, &ranges[0]);
        rootParameters[1].InitAsDescriptorTable(1, &ranges[1]);
        rootParameters[2].InitAsDescriptorTable(1, &ranges[2]);
        rootParameters[3].InitAsConstantBufferView(0, 0);
        rootParameters[4].InitAsDescriptorTable(1, &ranges[3]);
        rootParameters[5].InitAsDescriptorTable(1, &ranges[4]);
        rootParameters[6].InitAsDescriptorTable(1, &ranges[5]);
        rootParameters[7].InitAsDescriptorTable(1, &ranges[6]);
        rootParameters[8].InitAsDescriptorTable(1, &ranges[7]);
        rootParameters[9].InitAsConstantBufferView(1);

        CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters);
        rootSignatureComponent->SerializeAndCreateRaytracingRootSignature(device, rootSignatureDesc, composeRootSig, L"Root signature: CompositionCS");

        D3D12_COMPUTE_PIPELINE_STATE_DESC descComputePSO = {};
        descComputePSO.pRootSignature = composeRootSig.Get();
        descComputePSO.CS = CD3DX12_SHADER_BYTECODE((void*)g_pCompositionCS, ARRAYSIZE(g_pCompositionCS));

        ThrowIfFailed(device->CreateComputePipelineState(&descComputePSO, IID_PPV_ARGS(&composePSO[0])));
        composePSO[0]->SetName(L"PSO: CompositionCS");

        commandList->SetDescriptorHeaps(1, descriptorHeap->GetAddressOf());
        commandList->SetComputeRootSignature(composeRootSig.Get());
        commandList->SetPipelineState(composePSO[0].Get());
        composePSO[0]->AddRef();

        commandList->SetComputeRootDescriptorTable(0, descriptors[Descriptors::RAYTRACING]);
        commandList->SetComputeRootDescriptorTable(1, descriptors[Descriptors::REFLECTION]);
        commandList->SetComputeRootDescriptorTable(2, descriptors[Descriptors::SHADOW]);

        auto frameIndex = deviceResources->GetCurrentFrameIndex();
        filterCB.CopyStagingToGpu(frameIndex);
        commandList->SetComputeRootConstantBufferView(3, filterCB.GpuVirtualAddress(frameIndex));
        commandList->SetComputeRootDescriptorTable(4, descriptors[Descriptors::NORMAL_DEPTH]);
        commandList->SetComputeRootDescriptorTable(5, descriptors[Descriptors::PREV_FRAME]);
        commandList->SetComputeRootDescriptorTable(6, descriptors[Descriptors::PREV_REFLECTION]);
        commandList->SetComputeRootDescriptorTable(7, descriptors[Descriptors::PREV_SHADOW]);
        commandList->SetComputeRootDescriptorTable(8, descriptors[Descriptors::MOTION_VECTOR]);
        commandList->SetComputeRootConstantBufferView(9, sceneCB.GpuVirtualAddress(frameIndex));

        auto outputSize = deviceResources->GetOutputSize();
        commandList->Dispatch(outputSize.right / 8, outputSize.bottom / 8, 1);

        XMMATRIX prevView, prevProj;
        prevView = camera.GetView();
        prevProj = camera.GetProj();
        sceneCB->prevFrameViewProj = XMMatrixMultiply(prevView, prevProj);
    }

};