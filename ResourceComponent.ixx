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
import DeviceResources;
import DXSampleHelper;

//import PsoComponent;
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
import SrvComponent;
import BottomLevelASComponent;
import TopLevelASComponent;

using namespace DirectX;
using namespace std;
using namespace Microsoft::WRL;

export class ResourceComponent {
    vector<PrimitiveConstantBuffer>& triangleMaterialCB;
    const UINT& FrameCount;
    vector<DX::GPUTimer>& gpuTimers;
    ShaderTableComponent*& shaderTableComponent;
    InitInterface*& initComponent;
    RootSignatureComponent*& rootSignatureComponent;
    DeviceResourcesInterface* deviceResources;
    unordered_map<string, unique_ptr<MeshGeometry>>& geometries;
    CameraComponent*& cameraComponent;
    ComPtr<ID3D12RootSignature>& raytracingGlobalRootSignature;
    vector<ComPtr<ID3D12RootSignature>>& raytracingLocalRootSignature;
    ComPtr<ID3D12Device5>& dxrDevice;
    ComPtr<ID3D12GraphicsCommandList5>& dxrCommandList;
    ComPtr<ID3D12StateObject>& dxrStateObject;
    DescriptorHeap*& descriptorHeap;
    UINT& descriptorsAllocated;
    ConstantBuffer<SceneConstantBuffer>& sceneCB;
    StructuredBuffer<PrimitiveInstancePerFrameBuffer>& trianglePrimitiveAttributeBuffer;
    vector<ComPtr<ID3D12Resource>>& bottomLevelAS;
    ComPtr<ID3D12Resource>& topLevelAS;
    vector<ComPtr<ID3D12Resource>>& buffers;
    ComPtr<ID3D12Resource>& rayGenShaderTable;
    ComPtr<ID3D12Resource>& missShaderTable;
    ComPtr<ID3D12Resource>& hitGroupShaderTable;
    vector<int>& meshOffsets;
    vector<int>& meshSizes;
    UINT& geoOffset;
    vector<D3DBuffer>& indexBuffer;
    vector<D3DBuffer>& vertexBuffer;
    SrvComponent*& srvComponent;
    UINT& descriptorSize;
    unordered_map<string, unique_ptr<Texture>>& textures;
    const UINT& NUM_BLAS;
    vector<D3DTexture>& stoneTexture;
    unordered_map<int, Material>& materials;
    vector<D3DTexture>& templeTextures;
    vector<D3DTexture>& templeNormalTextures;
    vector<D3DTexture>& templeSpecularTextures;
    vector<D3DTexture>& templeEmittanceTextures;
    BottomLevelASComponent*& bottomLevelAsComponent;
    TopLevelASComponent*& topLevelAsComponent;

    OutputComponent*& outputComponent;
    GeometryComponent*& geometryComponent;
    AccelerationStructureComponent*& asComponent;
    ConstantBuffer<AtrousWaveletTransformFilterConstantBuffer>& filterCB;

    vector<const wchar_t*>& c_hitGroupNames_TriangleGeometry;
    vector<const wchar_t*>& c_missShaderNames;
    const wchar_t*& c_closestHitShaderName;
    const wchar_t*& c_raygenShaderName;
    UINT& missShaderTableStrideInBytes;
    UINT& hitGroupShaderTableStrideInBytes;

    UINT& width;
    UINT& height;
    vector<D3D12_GPU_DESCRIPTOR_HANDLE>& descriptors;


    ShaderComponent* shaderComponent;


public:
    ResourceComponent(
        vector<PrimitiveConstantBuffer>& triangleMaterialCB,
        const UINT& FrameCount,
        vector<DX::GPUTimer>& gpuTimers,
        ShaderTableComponent*& shaderTableComponent,
        InitInterface*& initComponent,
        RootSignatureComponent*& rootSignatureComponent,
        DeviceResourcesInterface* deviceResources,
        unordered_map<string, unique_ptr<MeshGeometry>>& geometries,
        CameraComponent*& cameraComponent,
        ComPtr<ID3D12RootSignature>& raytracingGlobalRootSignature,

        vector<ComPtr<ID3D12RootSignature>>& raytracingLocalRootSignature,
        ComPtr<ID3D12Device5>& dxrDevice,
        ComPtr<ID3D12GraphicsCommandList5>& dxrCommandList,
        ComPtr<ID3D12StateObject>& dxrStateObject,
        DescriptorHeap*& descriptorHeap,
        UINT& descriptorsAllocated,
        ConstantBuffer<SceneConstantBuffer>& sceneCB,
        StructuredBuffer<PrimitiveInstancePerFrameBuffer>& trianglePrimitiveAttributeBuffer,
        vector<ComPtr<ID3D12Resource>>& bottomLevelAS,
        ComPtr<ID3D12Resource>& topLevelAS,

        vector<ComPtr<ID3D12Resource>>& buffers,
        ComPtr<ID3D12Resource>& rayGenShaderTable,
        ComPtr<ID3D12Resource>& missShaderTable,
        ComPtr<ID3D12Resource>& hitGroupShaderTable,
        OutputComponent*& outputComponent,
        GeometryComponent*& geometryComponent,
        AccelerationStructureComponent*& asComponent,
        vector<int>& meshOffsets,
        vector<int>& meshSizes,

        UINT& geoOffset,
        vector<D3DBuffer>& indexBuffer,
        vector<D3DBuffer>& vertexBuffer,
        SrvComponent*& srvComponent,
        UINT& descriptorSize,
        unordered_map<string, unique_ptr<Texture>>& textures,
        const UINT& NUM_BLAS,
        unordered_map<int, Material>& materials,
        vector<D3DTexture>& stoneTexture,
        vector<D3DTexture>& templeTextures,

        vector<D3DTexture>& templeNormalTextures,
        vector<D3DTexture>& templeSpecularTextures,
        vector<D3DTexture>& templeEmittanceTextures,
        BottomLevelASComponent*& bottomLevelAsComponent,
        TopLevelASComponent*& topLevelAsComponent,
        ConstantBuffer<AtrousWaveletTransformFilterConstantBuffer>& filterCB,

        vector<const wchar_t*>& c_hitGroupNames_TriangleGeometry,
        vector<const wchar_t*>& c_missShaderNames,
        const wchar_t*& c_closestHitShaderName,
        const wchar_t*& c_raygenShaderName,
        UINT& missShaderTableStrideInBytes,
        UINT& hitGroupShaderTableStrideInBytes,

        UINT& width,
        UINT& height,
        vector<D3D12_GPU_DESCRIPTOR_HANDLE>& descriptors
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
        asComponent{ asComponent },
        meshOffsets{ meshOffsets },
        meshSizes{ meshSizes },
        geoOffset{ geoOffset },
        indexBuffer{ indexBuffer },
        vertexBuffer{ vertexBuffer },
        srvComponent{ srvComponent },
        descriptorSize{ descriptorSize },
        textures{ textures },
        NUM_BLAS{ NUM_BLAS },
        stoneTexture{ stoneTexture },
        materials{ materials },
        templeTextures{ templeTextures },
        templeNormalTextures{ templeNormalTextures },
        templeSpecularTextures{ templeSpecularTextures },
        templeEmittanceTextures{ templeEmittanceTextures },
        bottomLevelAsComponent{ bottomLevelAsComponent },
        topLevelAsComponent{ topLevelAsComponent },
        filterCB{ filterCB },
        c_hitGroupNames_TriangleGeometry{ c_hitGroupNames_TriangleGeometry },
        c_missShaderNames{ c_missShaderNames },
        c_closestHitShaderName{ c_closestHitShaderName },
        c_raygenShaderName{ c_raygenShaderName },
        missShaderTableStrideInBytes{ missShaderTableStrideInBytes },
        hitGroupShaderTableStrideInBytes{ hitGroupShaderTableStrideInBytes },

        width{ width },
        height{ height },
        descriptors{ descriptors }
    {}

    void CreateDeviceDependentResources() {
        CreateAuxilaryDeviceResources();
        CreateRaytracingInterfaces();
        rootSignatureComponent->CreateRootSignatures();
       
        CreateRaytracingPipelineStateObject();

        shaderTableComponent = new ShaderTableComponent(
            deviceResources,
            c_hitGroupNames_TriangleGeometry,
            c_missShaderNames,
            c_closestHitShaderName,
            c_raygenShaderName,
            hitGroupShaderTable,
            dxrStateObject,
            missShaderTableStrideInBytes,
            meshOffsets,
            indexBuffer,

            vertexBuffer,
            geometries,
            meshSizes,
            templeTextures,
            templeNormalTextures,
            templeSpecularTextures,
            templeEmittanceTextures,
            NUM_BLAS,
            hitGroupShaderTableStrideInBytes,
            triangleMaterialCB,

            rayGenShaderTable,
            missShaderTable
        );
        CreateDescriptorHeap();
        srvComponent = new SrvComponent(
            descriptorSize,
            deviceResources,
            textures,
            descriptorHeap
        );
        geometryComponent = new GeometryComponent(
            deviceResources,
            meshOffsets,
            meshSizes,
            geoOffset,
            geometries,
            indexBuffer,
            vertexBuffer,
            srvComponent
        );
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
            Material m = geometries[geoName]->DrawArgs[meshName].Material;
            SetAttributes2(i + 11, XMFLOAT4(m.Kd.x, m.Kd.y, m.Kd.z, 1.0f), m.Ks.x, m.Ns, 1.0f, 1.0f);
        }
        bottomLevelAsComponent = new BottomLevelASComponent(
            deviceResources,
            dxrDevice,
            dxrCommandList,
            meshSizes,
            indexBuffer,
            vertexBuffer,
            geometries,
            meshOffsets,
            NUM_BLAS
        );
        topLevelAsComponent = new TopLevelASComponent(
            deviceResources,
            NUM_BLAS,
            dxrDevice,
            bottomLevelAsComponent,
            dxrCommandList
        );
        asComponent = new AccelerationStructureComponent(
            deviceResources,
            dxrDevice,
            NUM_BLAS,
            dxrCommandList,
            bottomLevelAS,
            topLevelAS,
            stoneTexture,
            materials,
            meshOffsets,
            geometries,

            descriptorHeap,
            meshSizes,
            templeTextures,
            templeNormalTextures,
            templeSpecularTextures,
            templeEmittanceTextures,
            bottomLevelAsComponent,
            topLevelAsComponent
        );

        asComponent->BuildAccelerationStructures();
     
        CreateConstantBuffers();
        CreateTrianglePrimitiveAttributesBuffers();

        
        shaderTableComponent->BuildShaderTables();
        outputComponent = new OutputComponent(
            deviceResources,
            width,
            height,
            descriptorHeap,
            descriptors,
            buffers,
            descriptorSize
        );
        outputComponent->CreateRaytracingOutputResource();
    }

    void CreateWindowSizeDependentResources() {
        outputComponent->CreateRaytracingOutputResource();
        cameraComponent->UpdateCameraMatrices();
    }

    void ReleaseDeviceDependentResources() {
        for (auto& gpuTimer : gpuTimers)
        {
            gpuTimer.ReleaseDevice();
        }

        raytracingGlobalRootSignature.Reset();
        ResetComPtrArray(&raytracingLocalRootSignature);

        dxrDevice.Reset();
 
        dxrCommandList.Reset();
        dxrStateObject.Reset();


        //descriptorHeap.reset();
        descriptorsAllocated = 0;
        sceneCB.Release();

        trianglePrimitiveAttributeBuffer.Release();

        ResetComPtrArray(&bottomLevelAS);
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

        for (auto& gpuTimer : gpuTimers)
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
        CreateHitGroupSubobjects(&raytracingPipeline);

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

    void CreateDescriptorHeap() {
        auto device = deviceResources->GetD3DDevice();

        descriptorHeap = new DescriptorHeap(device, 10000, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        descriptorSize = descriptorHeap->DescriptorSize();
    }

    void CreateHitGroupSubobjects(CD3DX12_STATE_OBJECT_DESC* raytracingPipeline) {
        for (UINT rayType = 0; rayType < RayType::Count; rayType++)
        {
            auto hitGroup = raytracingPipeline->CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
            if (rayType == RayType::Radiance)
            {
                hitGroup->SetClosestHitShaderImport(c_closestHitShaderName);
            }
            hitGroup->SetHitGroupExport(c_hitGroupNames_TriangleGeometry[rayType]);
            hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);
        }
    }

    void CreateConstantBuffers() {
        auto device = deviceResources->GetD3DDevice();
        auto frameCount = deviceResources->GetBackBufferCount();

        sceneCB.Create(device, frameCount, L"Scene Constant Buffer");
        filterCB.Create(device, frameCount, L"Filter Constant Buffer");
    }

    void CreateTrianglePrimitiveAttributesBuffers() {
        auto device = deviceResources->GetD3DDevice();
        auto frameCount = deviceResources->GetBackBufferCount();

        trianglePrimitiveAttributeBuffer.Create(device, NUM_BLAS, frameCount, L"Triangle primitive attributes");
    }
};