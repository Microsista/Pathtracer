module;
#include <Windows.h>
#include <DirectXMath.h>
#include <string>
#include <vector>
#include "RayTracingHlslCompat.hlsli"
#include <memory>
#include <unordered_map>
#include <wrl/client.h>
export module ResourceComponent;

import RootSignatureComponent;
import PsoComponent;
import DescriptorComponent;
import GeometryComponent;
import AccelerationStructureComponent;
import OutputComponent;
import ShaderTableComponent;
import AccelerationStructureComponent;
import GeometryComponent;
import InitInterface;
import RootSignatureComponent;
import PsoComponent;
import BufferComponent;
import DeviceResources;
import Descriptors;
import PerformanceTimers;
import CameraComponent;
import DXSampleHelper;
import RaytracingSceneDefines;

using namespace DirectX;
using namespace std;
using namespace Microsoft::WRL;

export class ResourceComponent {
    PrimitiveConstantBuffer* triangleMaterialCB;
    UINT FrameCount;
    vector<DX::GPUTimer> gpuTimers;
    OutputComponent* outputComponent;
    ShaderTableComponent* shaderTableComponent;
    AccelerationStructureComponent* asComponent;
    GeometryComponent* geometryComponent;
    InitInterface* initComponent;
    RootSignatureComponent* rootSignatureComponent;
    PsoComponent* psoComponent;
    DescriptorComponent* descriptorComponent;
    BufferComponent* bufferComponent;
    DeviceResources* deviceResources;
    unordered_map<string, unique_ptr<MeshGeometry>> geometries;
    CameraComponent* cameraComponent;
    ComPtr<ID3D12RootSignature> raytracingGlobalRootSignature;
    ComPtr<ID3D12RootSignature> raytracingLocalRootSignature[LocalRootSignature::Type::Count];
    ComPtr<ID3D12Device5> dxrDevice;
    ComPtr<ID3D12GraphicsCommandList5> dxrCommandList;
    ComPtr<ID3D12StateObject> dxrStateObject;
    shared_ptr<DescriptorHeap> descriptorHeap;
    UINT descriptorsAllocated;
    ConstantBuffer<SceneConstantBuffer> sceneCB;
    StructuredBuffer<PrimitiveInstancePerFrameBuffer> trianglePrimitiveAttributeBuffer;
    ComPtr<ID3D12Resource> bottomLevelAS[BottomLevelASType::Count];
    ComPtr<ID3D12Resource> topLevelAS;
    ComPtr<ID3D12Resource>* buffers;
    ComPtr<ID3D12Resource> rayGenShaderTable;
    ComPtr<ID3D12Resource> missShaderTable;
    ComPtr<ID3D12Resource> hitGroupShaderTable;

public:
    ResourceComponent() {}

    void CreateDeviceDependentResources() {
        CreateAuxilaryDeviceResources();
        initComponent->CreateRaytracingInterfaces();
        rootSignatureComponent->CreateRootSignatures();
        psoComponent->CreateRaytracingPipelineStateObject();
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
            Material m = geometries[geoName]->DrawArgs[meshName].Material;
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
        for (auto& gpuTimer : gpuTimers)
        {
            gpuTimer.ReleaseDevice();
        }

        raytracingGlobalRootSignature.Reset();
        ResetComPtrArray(&raytracingLocalRootSignature);

        dxrDevice.Reset();
        dxrCommandList.Reset();
        dxrStateObject.Reset();


        descriptorHeap.reset();
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
};