module;
#include <Windows.h>
#include <DirectXMath.h>
#include <string>
export module ResourceComponent;

import InitComponent;
import RootSignatureComponent;
import PsoComponent;
import DescriptorComponent;
import GeometryComponent;
import AccelerationStructureComponent;

using namespace DirectX;
using namespace std;

export class ResourceComponent {
    PrimitiveConstantBuffer* triangleMaterialCB;
    UINT FrameCount;
    DX::GPUTimer** gpuTimers;

public:
    ResourceComponent() {}

    void CreateDeviceDependentResources() {
        CreateAuxilaryDeviceResources();
        CreateRaytracingInterfaces();
        CreateRootSignatures();
        CreateRaytracingPipelineStateObject();
        CreateDescriptorHeap();
        BuildGeometry();

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

        BuildAccelerationStructures();
        CreateConstantBuffers();
        CreateTrianglePrimitiveAttributesBuffers();
        BuildShaderTables();
        CreateRaytracingOutputResource();
    }

    void CreateWindowSizeDependentResources() {
        CreateRaytracingOutputResource();
        UpdateCameraMatrices();
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

        raytracingGlobalRootSignature.Reset();
        ResetComPtrArray(&raytracingLocalRootSignature);

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