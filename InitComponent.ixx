module;
#include <memory>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <d3d12.h>
#include "RayTracingHlslCompat.hlsli"
#include <wrl/client.h>
#include <vector>
#include "PerformanceTimers.h"
export module InitComponent;

import DeviceResources;
import DeviceResourcesInterface;
import DXSampleHelper;
import Application;
import RenderingComponent;
import ConstantBuffer;
import StructuredBuffer;

import Helper;
import InitInterface;
import ResourceComponent;
import CameraComponent;
import DescriptorHeap;
import CoreInterface;

using namespace std;
using namespace DirectX;
using namespace Microsoft::WRL;

export class InitComponent : public InitInterface {
    shared_ptr<DeviceResourcesInterface> deviceResources;
    UINT FrameCount;
    UINT adapterIDoverride;
    UINT width;
    UINT height;
    RenderingComponent* renderingComponent;
    ConstantBuffer<SceneConstantBuffer>* sceneCB;
    PrimitiveConstantBuffer* triangleMaterialCB;
    Camera camera;

    CameraComponent* cameraComponent;

    XMVECTOR at;
    XMVECTOR up;
    XMVECTOR eye;

    XMFLOAT4 lightAmbientColor;
    XMFLOAT4 lightDiffuseColor;

    ID3D12Device5** dxrDevice;
    CoreInterface* core;

    ResourceComponent* resourceComponent;

    ComPtr<ID3D12Resource> rayGenShaderTable;
    ComPtr<ID3D12Resource> hitGroupShaderTable;
    ComPtr<ID3D12Resource> missShaderTable;

    UINT hitGroupShaderTableStrideInBytes;
    UINT missShaderTableStrideInBytes;

    vector<DX::GPUTimer>* gpuTimers;
    D3D12_GPU_DESCRIPTOR_HANDLE* descriptors;
    DescriptorHeap* descriptorHeap;

    ID3D12RootSignature* raytracingGlobalRootSignature;
    ID3D12Resource* topLevelAS;
    ID3D12GraphicsCommandList5** dxrCommandList;
    ID3D12StateObject* dxrStateObject;

    StructuredBuffer<PrimitiveInstancePerFrameBuffer>* trianglePrimitiveAttributeBuffer;

    XMFLOAT4 lightPosition;

    bool orbitalCamera;
    float aspectRatio;
    XMMATRIX* projectionToWorld;
    XMFLOAT3* cameraPosition;

public:
    InitComponent(
        DeviceResourcesInterface* deviceResources,
        PrimitiveConstantBuffer* triangleMaterialCB,
        XMFLOAT3* cameraPosition,
        UINT FrameCount,
        UINT adapterIDoverride,
        UINT width,
        UINT height,
        RenderingComponent* renderingComponent,
        ConstantBuffer<SceneConstantBuffer>* sceneCB,
        Camera camera,
        CameraComponent* cameraComponent,
        XMVECTOR at,
        XMVECTOR up,
        XMVECTOR eye,
        CoreInterface* core,
        ResourceComponent* resourceComponent,
        ComPtr<ID3D12Resource> rayGenShaderTable,
        ComPtr<ID3D12Resource> hitGroupShaderTable,
        ComPtr<ID3D12Resource> missShaderTable,
        UINT hitGroupShaderTableStrideInBytes,
        UINT missShaderTableStrideInBytes,
        vector<DX::GPUTimer>* gpuTimers,
        D3D12_GPU_DESCRIPTOR_HANDLE* descriptors,
        DescriptorHeap* descriptorHeap,
        ID3D12RootSignature* raytracingGlobalRootSignature,
        ID3D12Resource* topLevelAS,
        ID3D12StateObject* dxrStateObject,
        StructuredBuffer<PrimitiveInstancePerFrameBuffer>* trianglePrimitiveAttributeBuffer,
        bool orbitalCamera,
        float aspectRatio
    ) :
        deviceResources{ deviceResources },
        triangleMaterialCB{ triangleMaterialCB },
        cameraPosition{ cameraPosition },
        FrameCount{ FrameCount },
        adapterIDoverride{ adapterIDoverride },
        width{ width },
        height{ height },
        renderingComponent{ renderingComponent },
        sceneCB{ sceneCB },
        camera{ camera },
        cameraComponent{ cameraComponent },
        at{ at },
        up{ up },
        eye{ eye },
        core{ core },
        resourceComponent{ resourceComponent },
        rayGenShaderTable{ rayGenShaderTable },
        hitGroupShaderTable{ hitGroupShaderTable },
        missShaderTable{ missShaderTable },
        hitGroupShaderTableStrideInBytes{ hitGroupShaderTableStrideInBytes },
        missShaderTableStrideInBytes{ missShaderTableStrideInBytes },
        gpuTimers{ gpuTimers },
        descriptors{ descriptors },
        descriptorHeap{ descriptorHeap },
        raytracingGlobalRootSignature{ raytracingGlobalRootSignature },
        topLevelAS{ topLevelAS },
        dxrStateObject{ dxrStateObject },
        trianglePrimitiveAttributeBuffer{ trianglePrimitiveAttributeBuffer },
        orbitalCamera{ orbitalCamera },
        aspectRatio{ aspectRatio }
    {}

    void InitializeScene() {
        auto frameIndex = deviceResources->GetCurrentFrameIndex();

        // Setup materials.
        {
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

            // Albedos
            XMFLOAT4 green = XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f);
            XMFLOAT4 red = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);
            XMFLOAT4 yellow = XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f);
            XMFLOAT4 white = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);


            SetAttributes2(SkullGeometry::Skull + 2, XMFLOAT4(1.000f, 0.8f, 0.836f, 1.000f), 0.3f, 0.5f, 1.0f, 1.0f);

            // Table
            for (int i = 0; i < 4; i++)
            {
                SetAttributes2(i + 3, XMFLOAT4(1.000f, 0.0f, 0.0f, 1.000f), 0.8f, 0.01f, 1.0f, 1.0f);
            }

            // Lamp
            for (int i = 0; i < 4; i++)
            {
                SetAttributes2(i + 7, XMFLOAT4(0.000f, 0.5f, 0.836f, 1.000f), 0.8f, 0.5f, 1.0f, 1.0f);
            }
        }

        // Setup camera.
        {
            camera.SetPosition(0.0f, 2.0f, -15.0f);
            eye = { 0.0f, 1.6f, -10.0f, 1.0f };
            at = { 0.0f, 0.0f, 0.0f, 1.0f };
            XMVECTOR right = { 1.0f, 0.0f, 0.0f, 0.0f };

            XMVECTOR direction = XMVector4Normalize(at - eye);
            up = XMVector3Normalize(XMVector3Cross(direction, right));

            XMMATRIX rotate = XMMatrixRotationY(XMConvertToRadians(45.0f));
            eye = XMVector3Transform(eye, rotate);
            up = XMVector3Transform(up, rotate);
            camera.UpdateViewMatrix();
            cameraComponent = new CameraComponent(
                deviceResources.get(),
                sceneCB,
                orbitalCamera,
                &eye,
                &at,
                &up,
                aspectRatio,
                projectionToWorld,
                cameraPosition,
                &camera
            );
            cameraComponent->UpdateCameraMatrices();
        }

        // Setup lights.
        {
            XMFLOAT4 lp;
            XMFLOAT4 lac;
            XMFLOAT4 ldc;

            lightPosition = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
            (*sceneCB)->lightPosition = XMLoadFloat4(&lp);

            lightAmbientColor = XMFLOAT4(0.25f, 0.25f, 0.25f, 1.0f);
            (*sceneCB)->lightAmbientColor = XMLoadFloat4(&lac);

            float d = 0.6f;
            lightDiffuseColor = XMFLOAT4(d, d, d, 1.0f);
            (*sceneCB)->lightDiffuseColor = XMLoadFloat4(&ldc);
        }
    }
};