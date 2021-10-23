module;
#include <Windows.h>
#include <DirectXMath.h>
#include <string>
#include <vector>
#include "RayTracingHlslCompat.hlsli"
#include <memory>
#include <unordered_map>
#include <wrl/client.h>
#include <d3d12.h>

#include "RaytracingSceneDefines.h"
#include "PerformanceTimers.h"
export module ResourceComponent;

import RootSignatureComponent;
import BufferComponent;
import DeviceResources;
import DXSampleHelper;

//import PsoComponent;
import DescriptorComponent;
import GeometryComponent;
import AccelerationStructureComponent;
import OutputComponent;
import ShaderTableComponent;
import InitInterface;
import Descriptors;
import Helper;

import CameraComponent;
import Mesh;
import DescriptorHeap;
import ConstantBuffer;
import StructuredBuffer;
import Material;
import ShaderComponent;

using namespace DirectX;
using namespace std;
using namespace Microsoft::WRL;

export class ResourceComponent {
    PrimitiveConstantBuffer* triangleMaterialCB;
    UINT FrameCount;
    vector<DX::GPUTimer>* gpuTimers;
    ShaderTableComponent* shaderTableComponent;
    InitInterface* initComponent;
    RootSignatureComponent* rootSignatureComponent;
    //PsoComponent* psoComponent;
    DeviceResourcesInterface* deviceResources;
    unordered_map<string, unique_ptr<MeshGeometry>>* geometries;
    CameraComponent* cameraComponent;
    ComPtr<ID3D12RootSignature>& raytracingGlobalRootSignature;
    vector<ComPtr<ID3D12RootSignature>>& raytracingLocalRootSignature;
    ComPtr<ID3D12Device5> dxrDevice;
    ComPtr<ID3D12GraphicsCommandList5> dxrCommandList;
    ComPtr<ID3D12StateObject> dxrStateObject;
    shared_ptr<DescriptorHeap> descriptorHeap;
    UINT descriptorsAllocated;
    ConstantBuffer<SceneConstantBuffer> sceneCB;
    StructuredBuffer<PrimitiveInstancePerFrameBuffer> trianglePrimitiveAttributeBuffer;
    ComPtr<ID3D12Resource>* bottomLevelAS;
    ComPtr<ID3D12Resource> topLevelAS;
    ComPtr<ID3D12Resource>* buffers;
    ComPtr<ID3D12Resource> rayGenShaderTable;
    ComPtr<ID3D12Resource> missShaderTable;
    ComPtr<ID3D12Resource> hitGroupShaderTable;

    OutputComponent* outputComponent;
    GeometryComponent* geometryComponent;
    BufferComponent* bufferComponent;
    AccelerationStructureComponent* asComponent;
    DescriptorComponent* descriptorComponent;
    ShaderComponent* shaderComponent;

public:
    ResourceComponent(
        PrimitiveConstantBuffer* triangleMaterialCB,
        UINT FrameCount,
        vector<DX::GPUTimer>* gpuTimers,
        ShaderTableComponent* shaderTableComponent,
        InitInterface* initComponent,
        RootSignatureComponent* rootSignatureComponent,
        DeviceResourcesInterface* deviceResources,
        unordered_map<string, unique_ptr<MeshGeometry>>* geometries,
        CameraComponent* cameraComponent,
        ComPtr<ID3D12RootSignature>& raytracingGlobalRootSignature,
        vector<ComPtr<ID3D12RootSignature>>& raytracingLocalRootSignature,
        ComPtr<ID3D12Device5> dxrDevice,
        ComPtr<ID3D12GraphicsCommandList5> dxrCommandList,
        ComPtr<ID3D12StateObject> dxrStateObject,
        shared_ptr<DescriptorHeap> descriptorHeap,
        UINT descriptorsAllocated,
        ConstantBuffer<SceneConstantBuffer> sceneCB,
        StructuredBuffer<PrimitiveInstancePerFrameBuffer> trianglePrimitiveAttributeBuffer,
        ComPtr<ID3D12Resource>* bottomLevelAS,
        ComPtr<ID3D12Resource> topLevelAS,
        ComPtr<ID3D12Resource>* buffers,
        ComPtr<ID3D12Resource> rayGenShaderTable,
        ComPtr<ID3D12Resource> missShaderTable,
        ComPtr<ID3D12Resource> hitGroupShaderTable,
        OutputComponent* outputComponent,
        GeometryComponent* geometryComponent,
        BufferComponent* bufferComponent,
        AccelerationStructureComponent* asComponent,
        DescriptorComponent* descriptorComponent
    ) :
        triangleMaterialCB{ triangleMaterialCB },
        FrameCount{ FrameCount },
        gpuTimers{ gpuTimers },
        shaderTableComponent{ shaderTableComponent },
        initComponent{ initComponent },
        rootSignatureComponent{ rootSignatureComponent },
        deviceResources{ deviceResources },
        geometries{ geometries },
        cameraComponent{ cameraComponent },
        raytracingGlobalRootSignature{ raytracingGlobalRootSignature },
        raytracingLocalRootSignature{ raytracingLocalRootSignature },
        dxrDevice{ dxrDevice },
        dxrCommandList{ dxrCommandList },
        dxrStateObject{ dxrStateObject },
        descriptorHeap{ descriptorHeap },
        descriptorsAllocated{ descriptorsAllocated },
        sceneCB{ sceneCB },
        trianglePrimitiveAttributeBuffer{ trianglePrimitiveAttributeBuffer },
        bottomLevelAS{ bottomLevelAS },
        topLevelAS{ topLevelAS },
        buffers{ buffers },
        rayGenShaderTable{ rayGenShaderTable },
        missShaderTable{ missShaderTable },
        hitGroupShaderTable{ hitGroupShaderTable },
        outputComponent{ outputComponent },
        geometryComponent{ geometryComponent },
        bufferComponent{ bufferComponent },
        asComponent{ asComponent },
        descriptorComponent{ descriptorComponent }
    {}

    void CreateDeviceDependentResources() {
        CreateAuxilaryDeviceResources();
        CreateRaytracingInterfaces();
        rootSignatureComponent->CreateRootSignatures();
        CreateRaytracingPipelineStateObject();
        descriptorComponent->CreateDescriptorHeap();
        geometryComponent->BuildGeometry();

        auto SetAttributes2 = [&](
            UINT primitiveIndex,
            const XMFLOAT4& albedo,
            float metalness = 0.7f,
            float roughness = 0.0f,
            float stepScale = 1.0f,
            float shaded = 1.0f)
        {
            auto& attributes = triangleMaterialCB[primitiveIndex];
            attributes.albedo = albedo;
            attributes.reflectanceCoef = metalness;
            attributes.diffuseCoef = 1 - metalness;
            attributes.metalness = metalness;
            attributes.roughness = roughness;
            attributes.stepScale = stepScale;
            attributes.shaded = shaded;
        };

        for (int i = 0; i < 1056; i++) {
            string geoName = "geo" + to_string(i + 11);
            string meshName = "mesh" + to_string(i + 11);
            Material m = (*geometries)[geoName]->DrawArgs[meshName].Material;
            SetAttributes2(i + 11, XMFLOAT4(m.Kd.x, m.Kd.y, m.Kd.z, 1.0f), m.Ks.x, m.Ns, 1.0f, 1.0f);
        }

        asComponent->BuildAccelerationStructures();
        bufferComponent->CreateConstantBuffers();
        bufferComponent->CreateTrianglePrimitiveAttributesBuffers();
        shaderTableComponent->BuildShaderTables();
        outputComponent->CreateRaytracingOutputResource();
    }

    void CreateWindowSizeDependentResources() {
        outputComponent->CreateRaytracingOutputResource();
        cameraComponent->UpdateCameraMatrices();
    }

    void ReleaseDeviceDependentResources() {
        for (auto& gpuTimer : *gpuTimers)
        {
            gpuTimer.ReleaseDevice();
        }

        raytracingGlobalRootSignature.Reset();
        //ResetComPtrArray(&raytracingLocalRootSignature);

        dxrDevice.Reset();
        dxrCommandList.Reset();
        dxrStateObject.Reset();


        descriptorHeap.reset();
        descriptorsAllocated = 0;
        sceneCB.Release();

        trianglePrimitiveAttributeBuffer.Release();

        //ResetComPtrArray(&bottomLevelAS);
        topLevelAS.Reset();

        buffers[Descriptors::RAYTRACING].Reset();
        rayGenShaderTable.Reset();
        missShaderTable.Reset();
        hitGroupShaderTable.Reset();
    }

    void ReleaseWindowSizeDependentResources() {
        buffers[Descriptors::RAYTRACING].Reset();
    }

    void CreateAuxilaryDeviceResources() {
        auto device = deviceResources->GetD3DDevice();
        auto commandQueue = deviceResources->GetCommandQueue();

        for (auto& gpuTimer : *gpuTimers)
        {
            gpuTimer.RestoreDevice(device, commandQueue, FrameCount, deviceResources->GetCommandList(), deviceResources->GetCommandAllocator());
        }
    }

    void CreateRaytracingInterfaces() {
        auto device = deviceResources->GetD3DDevice();
        auto commandList = deviceResources->GetCommandList();

        ThrowIfFailed(device->QueryInterface(IID_PPV_ARGS(&dxrDevice)), L"Couldn't get DirectX Raytracing interface for the device.\n");
        ThrowIfFailed(commandList->QueryInterface(IID_PPV_ARGS(&dxrCommandList)), L"Couldn't get DirectX Raytracing interface for the command list.\n");
    }

    void CreateRaytracingPipelineStateObject() {
        CD3DX12_STATE_OBJECT_DESC raytracingPipeline{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };
        shaderComponent = new ShaderComponent();
        shaderComponent->CreateDxilLibrarySubobjects(&raytracingPipeline);
        shaderTableComponent->CreateHitGroupSubobjects(&raytracingPipeline);

        auto shaderConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();

        UINT payloadSize = max(sizeof(RayPayload), sizeof(ShadowRayPayload));

        UINT attributeSize = sizeof(struct ProceduralPrimitiveAttributes);

        shaderConfig->Config(payloadSize, attributeSize);

        rootSignatureComponent->CreateLocalRootSignatureSubobjects(&raytracingPipeline);

        auto globalRootSignature = raytracingPipeline.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
        globalRootSignature->SetRootSignature(raytracingGlobalRootSignature.Get());

        auto pipelineConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();

        UINT maxRecursionDepth = MAX_RAY_RECURSION_DEPTH;
        pipelineConfig->Config(maxRecursionDepth);

        PrintStateObjectDesc(raytracingPipeline);

        ThrowIfFailed(dxrDevice->CreateStateObject(raytracingPipeline, IID_PPV_ARGS(&dxrStateObject)), L"Couldn't create DirectX Raytracing state object.\n");
    }
};