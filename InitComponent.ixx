module;
#include <memory>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <d3d12.h>
#include "RayTracingHlslCompat.hlsli"
export module InitComponent;

import DeviceResources;
import DXSampleHelper;
import Application;
import RenderingComponent;
import ConstantBuffer;
import Helper;
import Core;
import InitInterface;

using namespace std;
using namespace DirectX;
import ResourceComponent;

export class InitComponent : public InitInterface {
    shared_ptr<DeviceResources> deviceResources;
    UINT FrameCount;
    UINT adapterIDoverride;
    UINT width;
    UINT height;
    RenderingComponent* renderingComponent;
    ConstantBuffer<SceneConstantBuffer>* sceneCB;
    PrimitiveConstantBuffer* triangleMaterialCB;
    Camera camera;

    XMVECTOR at;
    XMVECTOR up;
    XMVECTOR eye;

    XMFLOAT4 lightAmbientColor;
    XMFLOAT4 lightDiffuseColor;

    ID3D12Device5* dxrDevice;
    Core* core;

    ResourceComponent* resourceComponent;

public:
    InitComponent() {}

    virtual void OnInit() {
        deviceResources = std::make_unique<DeviceResources>(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_UNKNOWN, FrameCount,
            D3D_FEATURE_LEVEL_11_0, DeviceResources::c_RequireTearingSupport, adapterIDoverride);
        deviceResources->RegisterDeviceNotify(core);
        deviceResources->SetWindow(Application::GetHwnd(), width, height);
        deviceResources->InitializeDXGIAdapter();

        ThrowIfFalse(IsDirectXRaytracingSupported(deviceResources->GetAdapter()),
            L"ERROR: DirectX Raytracing is not supported by your OS, GPU and/or driver.\n\n");

        deviceResources->CreateDeviceResources();
        deviceResources->CreateWindowSizeDependentResources();

        InitializeScene();
        resourceComponent->CreateDeviceDependentResources();

        CreateWindowSizeDependentResources();

        renderingComponent = new RenderingComponent{
           deviceResources.get(),
           hitGroupShaderTable.Get(),
           missShaderTable.Get(),
           rayGenShaderTable.Get(),
           hitGroupShaderTableStrideInBytes,
           missShaderTableStrideInBytes,
           width,
           height,
           gpuTimers,
           descriptors,
           descriptorHeap.get(),
           &camera,
           raytracingGlobalRootSignature.Get(),
           topLevelAS.Get(),
           dxrCommandList.Get(),
           dxrStateObject.Get(),
           &sceneCB,
           &trianglePrimitiveAttributeBuffer
        };
    }

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
            UpdateCameraMatrices();
        }

        // Setup lights.
        {
            XMFLOAT4 lp;
            XMFLOAT4 lac;
            XMFLOAT4 ldc;

            lightPosition = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
            sceneCB->lightPosition = XMLoadFloat4(&lp);

            lightAmbientColor = XMFLOAT4(0.25f, 0.25f, 0.25f, 1.0f);
            sceneCB->lightAmbientColor = XMLoadFloat4(&lac);

            float d = 0.6f;
            lightDiffuseColor = XMFLOAT4(d, d, d, 1.0f);
            sceneCB->lightDiffuseColor = XMLoadFloat4(&ldc);
        }
    }

    void CreateRaytracingInterfaces() {
        auto device = deviceResources->GetD3DDevice();
        auto commandList = deviceResources->GetCommandList();

        ThrowIfFailed(device->QueryInterface(IID_PPV_ARGS(&dxrDevice)), L"Couldn't get DirectX Raytracing interface for the device.\n");
        ThrowIfFailed(commandList->QueryInterface(IID_PPV_ARGS(&dxrCommandList)), L"Couldn't get DirectX Raytracing interface for the command list.\n");
    }
};