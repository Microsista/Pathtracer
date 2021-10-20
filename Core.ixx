module;
extern "C" {
    #include "Lua542/include/lua.h"
    #include "Lua542/include/lauxlib.h"
    #include "Lua542/include/lualib.h"
}

#include "RaytracingHlslCompat.hlsli"

#include "Obj/Debug/CompiledShaders/ViewRG.hlsl.h"
#include "Obj/Debug/CompiledShaders/RadianceCH.hlsl.h"
#include "Obj/Debug/CompiledShaders/RadianceMS.hlsl.h"
#include "Obj/Debug/CompiledShaders/ShadowMS.hlsl.h"
#include "Obj/Debug/CompiledShaders/CompositionCS.hlsl.h"
#include "Obj/Debug/CompiledShaders/BlurCS.hlsl.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "d3dx12.h"
#include <unordered_map>
#include <vector>
#include <sstream>
#include <array>
#include <iomanip>
#include <d3d12.h>
#include <string>

#include <wrl/client.h>

#include <ranges>
export module Core;

import DXCore;
import StepTimer;
import Texture;
import Helper;
import Mesh;
import RaytracingSceneDefines;
import Geometry;
import PerformanceTimers;
import Directions;
import Globals;
import Descriptors;
import ShaderTable;
import ShaderRecord;
import AccelerationStructureBuffers;
import D3DBuffer;
import DescriptorHeap;
import ConstantBuffer;
import StructuredBuffer;
import D3DTexture;
import Transform;
import DeviceResources;
import Application;
import DXSampleHelper;
import IDeviceNotify;

using namespace std;
using namespace DX;
using namespace std::views;
using namespace Microsoft::WRL;

export class Core : public DXCore {
public:
    Core(UINT width, UINT height) :
        DXCore{ width, height, L"" },
        m_animateGeometryTime(0.0f),
        m_animateCamera(false),
        m_animateGeometry(true),
        m_animateLight(false),
        m_descriptorsAllocated(0),
        m_descriptorSize(0),
        m_missShaderTableStrideInBytes(UINT_MAX),
        m_hitGroupShaderTableStrideInBytes(UINT_MAX)
    {
        UpdateForSizeChange(width, height);
    }

    virtual void OnDeviceLost() override {
        ReleaseWindowSizeDependentResources();
        ReleaseDeviceDependentResources();
    }
    virtual void OnDeviceRestored() override {
        CreateDeviceDependentResources();
        CreateWindowSizeDependentResources();
    }

    virtual void OnInit() {
        m_deviceResources = std::make_unique<DeviceResources>(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_UNKNOWN, FrameCount,
            D3D_FEATURE_LEVEL_11_0, DeviceResources::c_RequireTearingSupport, m_adapterIDoverride);
        m_deviceResources->RegisterDeviceNotify(this);
        m_deviceResources->SetWindow(Application::GetHwnd(), m_width, m_height);
        m_deviceResources->InitializeDXGIAdapter();

        ThrowIfFalse(IsDirectXRaytracingSupported(m_deviceResources->GetAdapter()),
            L"ERROR: DirectX Raytracing is not supported by your OS, GPU and/or driver.\n\n");

        m_deviceResources->CreateDeviceResources();
        m_deviceResources->CreateWindowSizeDependentResources();

        InitializeScene();
        CreateDeviceDependentResources();

        CreateWindowSizeDependentResources();
    }

    virtual void OnKeyDown(UINT8 key)
    {
        // rotation
        float elapsedTime = static_cast<float>(m_timer.GetElapsedSeconds());
        float secondsToRotateAround = 0.1f;
        float angleToRotateBy = -360.0f * (elapsedTime / secondsToRotateAround);
        const XMVECTOR& prevLightPosition = m_sceneCB->lightPosition;
        XMMATRIX rotate = XMMatrixRotationY(XMConvertToRadians(angleToRotateBy));
        XMMATRIX rotateClockwise = XMMatrixRotationY(XMConvertToRadians(-angleToRotateBy));

        auto speed = 100.0f;
        auto movementSpeed = 100.0f;
        if (GetKeyState(VK_SHIFT))
            movementSpeed *= 5;
        switch (key)
        {
        case 'W': m_camera.Walk(movementSpeed * elapsedTime); break;
        case 'S': m_camera.Walk(-movementSpeed * elapsedTime); break;
        case 'A': m_camera.Strafe(-movementSpeed * elapsedTime); break;
        case 'D': m_camera.Strafe(movementSpeed * elapsedTime); break;
        case 'Q': m_sceneCB->lightPosition = XMVector3Transform(prevLightPosition, rotate); break;
        case 'E': m_sceneCB->lightPosition = XMVector3Transform(prevLightPosition, rotateClockwise); break;
        case 'I': m_sceneCB->lightPosition += speed * Directions::FORWARD * elapsedTime; break;
        case 'J': m_sceneCB->lightPosition += speed * Directions::LEFT * elapsedTime; break;
        case 'K': m_sceneCB->lightPosition += speed * Directions::BACKWARD * elapsedTime; break;
        case 'L': m_sceneCB->lightPosition += speed * Directions::RIGHT * elapsedTime; break;
        case 'U': m_sceneCB->lightPosition += speed * Directions::DOWN * elapsedTime; break;
        case 'O': m_sceneCB->lightPosition += speed * Directions::UP * elapsedTime;  break;
        case '1':
            XMFLOAT4 equal;
            XMStoreFloat4(&equal, XMVectorEqual(m_sceneCB->lightPosition, XMVECTOR{ 0.0f, 0.0f, 0.0f }));
            equal.x ? m_sceneCB->lightPosition = XMVECTOR{ 0.0f, 18.0f, -20.0f, 0.0f } : m_sceneCB->lightPosition = XMVECTOR{ 0.0f, 0.0f, 0.0f, 0.0f };
            break;
        case '2': m_orbitalCamera = !m_orbitalCamera; break;
        }
        m_camera.UpdateViewMatrix();
        UpdateCameraMatrices();
    }

    virtual void OnMouseMove(int x, int y) override
    {
        float dx = XMConvertToRadians(0.25f * static_cast<float>(x - m_lastMousePosition.x));
        float dy = XMConvertToRadians(0.25f * static_cast<float>(y - m_lastMousePosition.y));

        m_camera.RotateY(dx);
        m_camera.Pitch(dy);


        m_camera.UpdateViewMatrix();
        UpdateCameraMatrices();

        m_lastMousePosition.x = x;
        m_lastMousePosition.y = y;
    }

    virtual void OnLeftButtonDown(UINT x, UINT y) override {
        m_lastMousePosition = { static_cast<int>(x), static_cast<int>(y) };
    }

    virtual void OnUpdate() {
        m_timer.Tick();
        CalculateFrameStats();
        float elapsedTime = static_cast<float>(m_timer.GetElapsedSeconds());
        auto frameIndex = m_deviceResources->GetCurrentFrameIndex();
        auto prevFrameIndex = m_deviceResources->GetPreviousFrameIndex();

        if (m_orbitalCamera)
        {
            float secondsToRotateAround = 48.0f;
            float angleToRotateBy = 360.0f * (elapsedTime / secondsToRotateAround);
            XMMATRIX rotate = XMMatrixRotationY(XMConvertToRadians(angleToRotateBy));
            m_eye = XMVector3Transform(m_eye, rotate);
            m_up = XMVector3Transform(m_up, rotate);
            m_at = XMVector3Transform(m_at, rotate);
            UpdateCameraMatrices();
        }

        if (m_animateLight)
        {
            float secondsToRotateAround = 8.0f;
            float angleToRotateBy = -360.0f * (elapsedTime / secondsToRotateAround);
            XMMATRIX rotate = XMMatrixRotationY(XMConvertToRadians(angleToRotateBy));
            const XMVECTOR& prevLightPosition = m_sceneCB->lightPosition;
            m_sceneCB->lightPosition = XMVector3Transform(prevLightPosition, rotate);
        }

        if (m_animateGeometry)
        {
            m_animateGeometryTime += elapsedTime;
        }
        m_sceneCB->elapsedTime = m_animateGeometryTime;

        auto outputSize = m_deviceResources->GetOutputSize();
        m_filterCB->textureDim = XMUINT2(outputSize.right, outputSize.bottom);
    }
    virtual void OnRender() {
        if (!m_deviceResources->IsWindowVisible())
        {
            return;
        }

        auto device = m_deviceResources->GetD3DDevice();
        auto commandList = m_deviceResources->GetCommandList();

        m_deviceResources->Prepare();
        for (auto& gpuTimer : m_gpuTimers)
        {
            gpuTimer.BeginFrame(commandList);
        }

        DoRaytracing();
        Compose();

        auto uav = CD3DX12_RESOURCE_BARRIER::UAV(buffers[PREV_FRAME].Get());
        m_dxrCommandList->ResourceBarrier(1, &uav);
        Blur();
        CopyRaytracingOutputToBackbuffer();

        for (auto& gpuTimer : m_gpuTimers)
        {
            gpuTimer.EndFrame(commandList);
        }

        m_deviceResources->Present(D3D12_RESOURCE_STATE_PRESENT);
    }

    virtual void Blur() {
        auto commandList = m_deviceResources->GetCommandList();
        auto device = m_deviceResources->GetD3DDevice();

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
        SerializeAndCreateRaytracingRootSignature(device, rootSignatureDesc, &m_blurRootSig, L"Root signature: BlurCS");

        // create pso
        D3D12_COMPUTE_PIPELINE_STATE_DESC descComputePSO = {};
        descComputePSO.pRootSignature = m_blurRootSig.Get();
        descComputePSO.CS = CD3DX12_SHADER_BYTECODE((void*)g_pBlurCS, ARRAYSIZE(g_pBlurCS));

        ThrowIfFailed(device->CreateComputePipelineState(&descComputePSO, IID_PPV_ARGS(&m_blurPSO[0])));
        m_blurPSO[0]->SetName(L"PSO: BlurCS");

        commandList->SetDescriptorHeaps(1, m_descriptorHeap->GetAddressOf());
        commandList->SetComputeRootSignature(m_blurRootSig.Get());
        commandList->SetPipelineState(m_blurPSO[0].Get());
        m_blurPSO[0]->AddRef();

        commandList->SetComputeRootDescriptorTable(0, descriptors[RAYTRACING]);
        commandList->SetComputeRootDescriptorTable(1, descriptors[REFLECTION]);
        commandList->SetComputeRootDescriptorTable(2, descriptors[SHADOW]);


        auto frameIndex = m_deviceResources->GetCurrentFrameIndex();
        m_filterCB.CopyStagingToGpu(frameIndex);
        commandList->SetComputeRootConstantBufferView(3, m_filterCB.GpuVirtualAddress(frameIndex));
        commandList->SetComputeRootDescriptorTable(4, descriptors[NORMAL_DEPTH]);
        commandList->SetComputeRootDescriptorTable(5, descriptors[PREV_FRAME]);
        commandList->SetComputeRootDescriptorTable(6, descriptors[PREV_REFLECTION]);
        commandList->SetComputeRootDescriptorTable(7, descriptors[PREV_SHADOW]);
        commandList->SetComputeRootDescriptorTable(8, descriptors[MOTION_VECTOR]);

        commandList->SetComputeRootConstantBufferView(9, m_sceneCB.GpuVirtualAddress(frameIndex));

        auto outputSize = m_deviceResources->GetOutputSize();
        commandList->Dispatch(outputSize.right / 8, outputSize.bottom / 8, 1);

        XMMATRIX prevView, prevProj;
        prevView = m_camera.GetView();
        prevProj = m_camera.GetProj();
        m_sceneCB->prevFrameViewProj = XMMatrixMultiply(prevView, prevProj);
    }

    virtual void Compose() {
        auto commandList = m_deviceResources->GetCommandList();
        auto device = m_deviceResources->GetD3DDevice();

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
        SerializeAndCreateRaytracingRootSignature(device, rootSignatureDesc, &m_composeRootSig, L"Root signature: CompositionCS");

        D3D12_COMPUTE_PIPELINE_STATE_DESC descComputePSO = {};
        descComputePSO.pRootSignature = m_composeRootSig.Get();
        descComputePSO.CS = CD3DX12_SHADER_BYTECODE((void*)g_pCompositionCS, ARRAYSIZE(g_pCompositionCS));

        ThrowIfFailed(device->CreateComputePipelineState(&descComputePSO, IID_PPV_ARGS(&m_composePSO[0])));
        m_composePSO[0]->SetName(L"PSO: CompositionCS");

        commandList->SetDescriptorHeaps(1, m_descriptorHeap->GetAddressOf());
        commandList->SetComputeRootSignature(m_composeRootSig.Get());
        commandList->SetPipelineState(m_composePSO[0].Get());
        m_composePSO[0]->AddRef();

        commandList->SetComputeRootDescriptorTable(0, descriptors[RAYTRACING]);
        commandList->SetComputeRootDescriptorTable(1, descriptors[REFLECTION]);
        commandList->SetComputeRootDescriptorTable(2, descriptors[SHADOW]);
     
        auto frameIndex = m_deviceResources->GetCurrentFrameIndex();
        m_filterCB.CopyStagingToGpu(frameIndex);
        commandList->SetComputeRootConstantBufferView(3, m_filterCB.GpuVirtualAddress(frameIndex));
        commandList->SetComputeRootDescriptorTable(4, descriptors[NORMAL_DEPTH]);
        commandList->SetComputeRootDescriptorTable(5, descriptors[PREV_FRAME]);
        commandList->SetComputeRootDescriptorTable(6, descriptors[PREV_REFLECTION]);
        commandList->SetComputeRootDescriptorTable(7, descriptors[PREV_SHADOW]);
        commandList->SetComputeRootDescriptorTable(8, descriptors[MOTION_VECTOR]);
        commandList->SetComputeRootConstantBufferView(9, m_sceneCB.GpuVirtualAddress(frameIndex));

        auto outputSize = m_deviceResources->GetOutputSize();
        commandList->Dispatch(outputSize.right / 8, outputSize.bottom / 8, 1);

        XMMATRIX prevView, prevProj;
        prevView = m_camera.GetView();
        prevProj = m_camera.GetProj();
        m_sceneCB->prevFrameViewProj = XMMatrixMultiply(prevView, prevProj);
    }
   
    virtual void OnSizeChanged(UINT width, UINT height, bool minimized) {
        if (!m_deviceResources->WindowSizeChanged(width, height, minimized))
        {
            return;
        }

        UpdateForSizeChange(width, height);

        ReleaseWindowSizeDependentResources();
        CreateWindowSizeDependentResources();
    }
    virtual void OnDestroy() {
        // Let GPU finish before releasing D3D resources.
        m_deviceResources->WaitForGpu();
        OnDeviceLost();
    }
    virtual IDXGISwapChain* GetSwapchain() { return m_deviceResources->GetSwapChain(); }

private:
    void UpdateCameraMatrices() {
        auto frameIndex = m_deviceResources->GetCurrentFrameIndex();

        if (m_orbitalCamera) {
            m_sceneCB->cameraPosition = m_eye;

            float fovAngleY = 45.0f;

            XMMATRIX view = XMMatrixLookAtLH(m_eye, m_at, m_up);
            XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(fovAngleY), m_aspectRatio, 0.01f, 125.0f);
            XMMATRIX viewProj = view * proj;
            m_sceneCB->projectionToWorld = XMMatrixInverse(nullptr, viewProj);
        }
        else {
            m_sceneCB->cameraPosition = m_camera.GetPosition();
            float fovAngleY = 45.0f;
            XMMATRIX view = m_camera.GetView();
            XMMATRIX proj = m_camera.GetProj();
            XMMATRIX viewProj = view * proj;
            m_sceneCB->projectionToWorld = XMMatrixInverse(nullptr, XMMatrixMultiply(m_camera.GetView(), m_camera.GetProj()));
        }
    }
    void InitializeScene() {
        auto frameIndex = m_deviceResources->GetCurrentFrameIndex();

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
                auto& attributes = m_triangleMaterialCB[primitiveIndex];
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
            m_camera.SetPosition(0.0f, 2.0f, -15.0f);
            m_eye = { 0.0f, 1.6f, -10.0f, 1.0f };
            m_at = { 0.0f, 0.0f, 0.0f, 1.0f };
            XMVECTOR right = { 1.0f, 0.0f, 0.0f, 0.0f };

            XMVECTOR direction = XMVector4Normalize(m_at - m_eye);
            m_up = XMVector3Normalize(XMVector3Cross(direction, right));

            XMMATRIX rotate = XMMatrixRotationY(XMConvertToRadians(45.0f));
            m_eye = XMVector3Transform(m_eye, rotate);
            m_up = XMVector3Transform(m_up, rotate);
            m_camera.UpdateViewMatrix();
            UpdateCameraMatrices();
        }

        // Setup lights.
        {
            XMFLOAT4 lightPosition;
            XMFLOAT4 lightAmbientColor;
            XMFLOAT4 lightDiffuseColor;

            lightPosition = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
            m_sceneCB->lightPosition = XMLoadFloat4(&lightPosition);

            lightAmbientColor = XMFLOAT4(0.25f, 0.25f, 0.25f, 1.0f);
            m_sceneCB->lightAmbientColor = XMLoadFloat4(&lightAmbientColor);

            float d = 0.6f;
            lightDiffuseColor = XMFLOAT4(d, d, d, 1.0f);
            m_sceneCB->lightDiffuseColor = XMLoadFloat4(&lightDiffuseColor);
        }
    }
    void RecreateD3D() {
        // Give GPU a chance to finish its execution in progress.
        try
        {
            m_deviceResources->WaitForGpu();
        }
        catch (HrException&)
        {
            // Do nothing, currently attached adapter is unresponsive.
        }
        m_deviceResources->HandleDeviceLost();
    }

    void DoRaytracing()
    {
        auto commandList = m_deviceResources->GetCommandList();
        auto frameIndex = m_deviceResources->GetCurrentFrameIndex();

        commandList->SetComputeRootSignature(m_raytracingGlobalRootSignature.Get());

        XMFLOAT3 tempEye;
        XMStoreFloat3(&tempEye, m_camera.GetPosition());

        // Copy dynamic buffers to GPU.
        {
            m_sceneCB.CopyStagingToGpu(frameIndex);
            commandList->SetComputeRootConstantBufferView(GlobalRootSignature::Slot::SceneConstant, m_sceneCB.GpuVirtualAddress(frameIndex));

            m_trianglePrimitiveAttributeBuffer.CopyStagingToGpu(frameIndex);
            commandList->SetComputeRootShaderResourceView(GlobalRootSignature::Slot::TriangleAttributeBuffer, m_trianglePrimitiveAttributeBuffer.GpuVirtualAddress(frameIndex));
        }

        // Bind the heaps, acceleration structure and dispatch rays.  
        D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
        SetCommonPipelineState(commandList);
        commandList->SetComputeRootShaderResourceView(GlobalRootSignature::Slot::AccelerationStructure, m_topLevelAS->GetGPUVirtualAddress());
        DispatchRays(m_dxrCommandList.Get(), m_dxrStateObject.Get(), &dispatchDesc, commandList);
        m_sceneCB->frameIndex = (m_sceneCB->frameIndex + 1) % 16;

        XMStoreFloat3(&m_sceneCB->prevFrameCameraPosition, m_camera.GetPosition());
        XMMATRIX prevViewCameraAtOrigin = XMMatrixLookAtLH(XMVectorSet(0, 0, 0, 1), XMVectorSetW(m_camera.GetLook() - m_camera.GetPosition(), 1), m_camera.GetUp());
        XMMATRIX prevView, prevProj;
        prevView = m_camera.GetView();
        prevProj = m_camera.GetProj();
        //m_sceneCB->prevFrameViewProj = XMMatrixMultiply(prevView, prevProj);
        XMMATRIX viewProjCameraAtOrigin = prevViewCameraAtOrigin * prevProj;
        m_sceneCB->prevFrameProjToViewCameraAtOrigin = XMMatrixInverse(nullptr, viewProjCameraAtOrigin);
    }

    void DispatchRays(auto* raytracingCommandList, auto* stateObject, auto* dispatchDesc, auto* commandList)
    {
        dispatchDesc->HitGroupTable.StartAddress = m_hitGroupShaderTable->GetGPUVirtualAddress();
        dispatchDesc->HitGroupTable.SizeInBytes = m_hitGroupShaderTable->GetDesc().Width;
        dispatchDesc->HitGroupTable.StrideInBytes = m_hitGroupShaderTableStrideInBytes;
        dispatchDesc->MissShaderTable.StartAddress = m_missShaderTable->GetGPUVirtualAddress();
        dispatchDesc->MissShaderTable.SizeInBytes = m_missShaderTable->GetDesc().Width;
        dispatchDesc->MissShaderTable.StrideInBytes = m_missShaderTableStrideInBytes;
        dispatchDesc->RayGenerationShaderRecord.StartAddress = m_rayGenShaderTable->GetGPUVirtualAddress();
        dispatchDesc->RayGenerationShaderRecord.SizeInBytes = m_rayGenShaderTable->GetDesc().Width;
        dispatchDesc->Width = m_width;
        dispatchDesc->Height = m_height;
        dispatchDesc->Depth = 1;
        raytracingCommandList->SetPipelineState1(stateObject);

        m_gpuTimers[GpuTimers::Raytracing].Start(commandList);
        raytracingCommandList->DispatchRays(dispatchDesc);
        m_gpuTimers[GpuTimers::Raytracing].Stop(commandList);
    };

    void SetCommonPipelineState(auto* descriptorSetCommandList)
    {
        descriptorSetCommandList->SetDescriptorHeaps(1, m_descriptorHeap->GetAddressOf());

        // Set index and successive vertex buffer decriptor tables.
        descriptorSetCommandList->SetComputeRootDescriptorTable(GlobalRootSignature::Slot::OutputView, descriptors[RAYTRACING]);
        descriptorSetCommandList->SetComputeRootDescriptorTable(GlobalRootSignature::Slot::ReflectionBuffer, descriptors[REFLECTION]);
        descriptorSetCommandList->SetComputeRootDescriptorTable(GlobalRootSignature::Slot::ShadowBuffer, descriptors[SHADOW]);
        descriptorSetCommandList->SetComputeRootDescriptorTable(GlobalRootSignature::Slot::NormalDepth, descriptors[NORMAL_DEPTH]);
        descriptorSetCommandList->SetComputeRootDescriptorTable(GlobalRootSignature::Slot::MotionVector, descriptors[MOTION_VECTOR]);
        descriptorSetCommandList->SetComputeRootDescriptorTable(GlobalRootSignature::Slot::PrevHitPosition, descriptors[PREV_HIT]);

    };

    void CreateConstantBuffers() {
        auto device = m_deviceResources->GetD3DDevice();
        auto frameCount = m_deviceResources->GetBackBufferCount();

        m_sceneCB.Create(device, frameCount, L"Scene Constant Buffer");
        m_filterCB.Create(device, frameCount, L"Filter Constant Buffer");
    }
    void CreateTrianglePrimitiveAttributesBuffers() {
        auto device = m_deviceResources->GetD3DDevice();
        auto frameCount = m_deviceResources->GetBackBufferCount();
        m_trianglePrimitiveAttributeBuffer.Create(device, NUM_BLAS, frameCount, L"Triangle primitive attributes");
    }
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
            auto& attributes = m_triangleMaterialCB[primitiveIndex];
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
            Material m = m_geometries[geoName]->DrawArgs[meshName].Material;
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
        for (auto& gpuTimer : m_gpuTimers)
        {
            gpuTimer.ReleaseDevice();
        }

        m_raytracingGlobalRootSignature.Reset();
        ResetComPtrArray(&m_raytracingLocalRootSignature);

        m_dxrDevice.Reset();
        m_dxrCommandList.Reset();
        m_dxrStateObject.Reset();

        m_raytracingGlobalRootSignature.Reset();
        ResetComPtrArray(&m_raytracingLocalRootSignature);

        m_descriptorHeap.reset();
        m_descriptorsAllocated = 0;
        m_sceneCB.Release();

        m_trianglePrimitiveAttributeBuffer.Release();

        ResetComPtrArray(&m_bottomLevelAS);
        m_topLevelAS.Reset();

        buffers[RAYTRACING].Reset();
        m_rayGenShaderTable.Reset();
        m_missShaderTable.Reset();
        m_hitGroupShaderTable.Reset();
    }

    void ReleaseWindowSizeDependentResources() {
        buffers[RAYTRACING].Reset();
    }

    void CreateRaytracingInterfaces() {
        auto device = m_deviceResources->GetD3DDevice();
        auto commandList = m_deviceResources->GetCommandList();

        ThrowIfFailed(device->QueryInterface(IID_PPV_ARGS(&m_dxrDevice)), L"Couldn't get DirectX Raytracing interface for the device.\n");
        ThrowIfFailed(commandList->QueryInterface(IID_PPV_ARGS(&m_dxrCommandList)), L"Couldn't get DirectX Raytracing interface for the command list.\n");
    }

    void SerializeAndCreateRaytracingRootSignature(ID3D12Device5* device, D3D12_ROOT_SIGNATURE_DESC& desc, Microsoft::WRL::ComPtr<ID3D12RootSignature>* rootSig, LPCWSTR resourceName = nullptr) {
        Microsoft::WRL::ComPtr<ID3DBlob> blob;
        Microsoft::WRL::ComPtr<ID3DBlob> error;

        ThrowIfFailed(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error), error ? static_cast<wchar_t*>(error->GetBufferPointer()) : nullptr);
        ThrowIfFailed(device->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&(*rootSig))));

        if (resourceName)
        {
            (*rootSig)->SetName(resourceName);
        }
    }

    void CreateRootSignatures() {
        auto device = m_deviceResources->GetD3DDevice();
        {
            CD3DX12_DESCRIPTOR_RANGE ranges[6] = {};
            ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
            ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1);
            ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 2);
            ranges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 3);
            ranges[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 4);
            ranges[5].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 5);

            CD3DX12_ROOT_PARAMETER rootParameters[GlobalRootSignature::Slot::Count];
            rootParameters[GlobalRootSignature::Slot::OutputView].InitAsDescriptorTable(1, &ranges[0]);
            rootParameters[GlobalRootSignature::Slot::AccelerationStructure].InitAsShaderResourceView(0);
            rootParameters[GlobalRootSignature::Slot::SceneConstant].InitAsConstantBufferView(0);
            rootParameters[GlobalRootSignature::Slot::TriangleAttributeBuffer].InitAsShaderResourceView(4);
            rootParameters[GlobalRootSignature::Slot::ReflectionBuffer].InitAsDescriptorTable(1, &ranges[1]);
            rootParameters[GlobalRootSignature::Slot::ShadowBuffer].InitAsDescriptorTable(1, &ranges[2]);
            rootParameters[GlobalRootSignature::Slot::NormalDepth].InitAsDescriptorTable(1, &ranges[3]);
            rootParameters[GlobalRootSignature::Slot::MotionVector].InitAsDescriptorTable(1, &ranges[4]);
            rootParameters[GlobalRootSignature::Slot::PrevHitPosition].InitAsDescriptorTable(1, &ranges[5]);

            CD3DX12_STATIC_SAMPLER_DESC staticSamplers[] = { CD3DX12_STATIC_SAMPLER_DESC(0, SAMPLER_FILTER) }; // LinearWrapSampler

            CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters, ARRAYSIZE(staticSamplers), staticSamplers);
            SerializeAndCreateRaytracingRootSignature(device, globalRootSignatureDesc, &m_raytracingGlobalRootSignature, L"Global root signature");
        }

        using namespace LocalRootSignature::Triangle;
        CD3DX12_DESCRIPTOR_RANGE ranges[Slot::Count] = {};
        ranges[Slot::IndexBuffer].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
        ranges[Slot::VertexBuffer].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
        ranges[Slot::DiffuseTexture].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3);
        ranges[Slot::NormalTexture].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5);
        ranges[Slot::SpecularTexture].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 6);
        ranges[Slot::EmissiveTexture].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 7);

        namespace RootSignatureSlots = LocalRootSignature::Triangle::Slot;
        CD3DX12_ROOT_PARAMETER rootParameters[RootSignatureSlots::Count] = {};

#define SizeOfInUint32(obj) ((sizeof(obj) - 1) / sizeof(UINT32) + 1)
        rootParameters[RootSignatureSlots::MaterialConstant].InitAsConstants(SizeOfInUint32(PrimitiveConstantBuffer), 1);
        rootParameters[RootSignatureSlots::GeometryIndex].InitAsConstants(SizeOfInUint32(PrimitiveInstanceConstantBuffer), 3);
        rootParameters[RootSignatureSlots::IndexBuffer].InitAsDescriptorTable(1, &ranges[Slot::IndexBuffer]);
        rootParameters[RootSignatureSlots::VertexBuffer].InitAsDescriptorTable(1, &ranges[Slot::VertexBuffer]);
        rootParameters[RootSignatureSlots::DiffuseTexture].InitAsDescriptorTable(1, &ranges[Slot::DiffuseTexture]);
        rootParameters[RootSignatureSlots::NormalTexture].InitAsDescriptorTable(1, &ranges[Slot::NormalTexture]);
        rootParameters[RootSignatureSlots::SpecularTexture].InitAsDescriptorTable(1, &ranges[Slot::SpecularTexture]);
        rootParameters[RootSignatureSlots::EmissiveTexture].InitAsDescriptorTable(1, &ranges[Slot::EmissiveTexture]);

        CD3DX12_ROOT_SIGNATURE_DESC localRootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters);
        localRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
        SerializeAndCreateRaytracingRootSignature(device, localRootSignatureDesc, &m_raytracingLocalRootSignature[LocalRootSignature::Type::Triangle], L"Local root signature");
    }
    void CreateDxilLibrarySubobjects(CD3DX12_STATE_OBJECT_DESC* raytracingPipeline) {
        const unsigned char* compiledShaderByteCode[] = { g_pViewRG, g_pRadianceCH, g_pRadianceMS, g_pShadowMS };
        const unsigned int compiledShaderByteCodeSizes[] = { ARRAYSIZE(g_pViewRG), ARRAYSIZE(g_pRadianceCH), ARRAYSIZE(g_pRadianceMS), ARRAYSIZE(g_pShadowMS) };
        auto size = sizeof(compiledShaderByteCode) / sizeof(compiledShaderByteCode[0]);
        for (int i = 0; i < size; i++) {
            auto lib = raytracingPipeline->CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
            auto libDXIL = CD3DX12_SHADER_BYTECODE((void*)compiledShaderByteCode[i], compiledShaderByteCodeSizes[i]);
            lib->SetDXILLibrary(&libDXIL);
        };

        // Use default shader exports for a DXIL library/collection subobject ~ surface all shaders.
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

    void CreateLocalRootSignatureSubobjects(CD3DX12_STATE_OBJECT_DESC* raytracingPipeline) {
        // Hit groups
        auto localRootSignature = raytracingPipeline->CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
        localRootSignature->SetRootSignature(m_raytracingLocalRootSignature[LocalRootSignature::Type::Triangle].Get());
        // Shader association
        auto rootSignatureAssociation = raytracingPipeline->CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
        rootSignatureAssociation->SetSubobjectToAssociate(*localRootSignature);
        rootSignatureAssociation->AddExports(c_hitGroupNames_TriangleGeometry);
    }

    void CreateRaytracingPipelineStateObject() {
        CD3DX12_STATE_OBJECT_DESC raytracingPipeline{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

        CreateDxilLibrarySubobjects(&raytracingPipeline);
        CreateHitGroupSubobjects(&raytracingPipeline);

        auto shaderConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();

        UINT payloadSize = max(sizeof(RayPayload), sizeof(ShadowRayPayload));

        UINT attributeSize = sizeof(struct ProceduralPrimitiveAttributes);

        shaderConfig->Config(payloadSize, attributeSize);

        CreateLocalRootSignatureSubobjects(&raytracingPipeline);

        auto globalRootSignature = raytracingPipeline.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
        globalRootSignature->SetRootSignature(m_raytracingGlobalRootSignature.Get());

        auto pipelineConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();

        UINT maxRecursionDepth = MAX_RAY_RECURSION_DEPTH;
        pipelineConfig->Config(maxRecursionDepth);

        PrintStateObjectDesc(raytracingPipeline);

        ThrowIfFailed(m_dxrDevice->CreateStateObject(raytracingPipeline, IID_PPV_ARGS(&m_dxrStateObject)), L"Couldn't create DirectX Raytracing state object.\n");
    }

    void CreateAuxilaryDeviceResources() {
        auto device = m_deviceResources->GetD3DDevice();
        auto commandQueue = m_deviceResources->GetCommandQueue();

        for (auto& gpuTimer : m_gpuTimers)
        {
            gpuTimer.RestoreDevice(device, commandQueue, FrameCount, m_deviceResources->GetCommandList(), m_deviceResources->GetCommandAllocator());
        }
    }

    void CreateDescriptorHeap() {
        auto device = m_deviceResources->GetD3DDevice();

        m_descriptorHeap = std::make_shared<DescriptorHeap>(device, 10000, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        m_descriptorSize = m_descriptorHeap->DescriptorSize();
    }
    void BuildModel(std::string path, UINT flags, bool usesTextures = false) {
        auto device = m_deviceResources->GetD3DDevice();

        GeometryGenerator geoGen;
        vector<MeshData> meshes = geoGen.LoadModel(path, flags);

        m_meshOffsets.push_back(m_meshOffsets.back() + m_meshSizes.back());
        m_meshSizes.push_back(meshes.size());

        for (auto j = 0; j < meshes.size(); j++) {
            MeshGeometry::Submesh submesh{};
            submesh.IndexCount = (UINT)meshes[j].Indices32.size();
            submesh.VertexCount = (UINT)meshes[j].Vertices.size();
            submesh.Material = meshes[j].Material;

            vector<VertexPositionNormalTextureTangent> vertices(meshes[j].Vertices.size());
            for (size_t i = 0; i < meshes[j].Vertices.size(); ++i) {
                vertices[i].position = meshes[j].Vertices[i].Position;
                vertices[i].normal = meshes[j].Vertices[i].Normal;
                vertices[i].textureCoordinate = usesTextures ? meshes[j].Vertices[i].TexC : XMFLOAT2{ 0.0f, 0.0f };
                vertices[i].tangent = usesTextures ? meshes[j].Vertices[i].TangentU : XMFLOAT3{ 1.0f, 0.0f, 0.0f };
            }

            vector<Index> indices;
            indices.insert(indices.end(), begin(meshes[j].Indices32), end(meshes[j].Indices32));

            AllocateUploadBuffer(device, &indices[0], indices.size() * sizeof(Index), &m_indexBuffer[j + m_geoOffset].resource);
            AllocateUploadBuffer(device, &vertices[0], vertices.size() * sizeof(VertexPositionNormalTextureTangent), &m_vertexBuffer[j + m_geoOffset].resource);

            CreateBufferSRV(&m_indexBuffer[j + m_geoOffset], indices.size(), sizeof(Index));
            CreateBufferSRV(&m_vertexBuffer[j + m_geoOffset], vertices.size(), sizeof(VertexPositionNormalTextureTangent));

            unique_ptr<MeshGeometry> geo = make_unique<MeshGeometry>();
            string tmp = "geo" + to_string(j + m_geoOffset);
            geo->Name = tmp;
            geo->VertexBufferGPU = m_vertexBuffer[j + m_geoOffset].resource;
            geo->IndexBufferGPU = m_indexBuffer[j + m_geoOffset].resource;
            geo->VertexByteStride = sizeof(VertexPositionNormalTextureTangent);
            geo->IndexFormat = DXGI_FORMAT_R32_UINT;
            geo->VertexBufferByteSize = (UINT)vertices.size() * sizeof(VertexPositionNormalTextureTangent);
            geo->IndexBufferByteSize = (UINT)indices.size() * sizeof(Index);
            tmp = "mesh" + to_string(j + m_geoOffset);
            geo->DrawArgs[tmp] = submesh;
            m_geometries[geo->Name] = move(geo);
        }

        m_geoOffset += (UINT)meshes.size();
    }

    void CreateRaytracingOutputResource() {
        auto device = m_deviceResources->GetD3DDevice();
        auto backbufferFormat = m_deviceResources->GetBackBufferFormat();

        auto uavDesc = CD3DX12_RESOURCE_DESC::Tex2D(backbufferFormat, m_width, m_height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        auto uavDesc2 = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_SINT, m_width, m_height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        auto defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

        UINT heapIndices[COUNT];
        fill_n(heapIndices, COUNT, UINT_MAX);

        for (auto i : iota(0, COUNT)) {
            ThrowIfFailed(device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, i == MOTION_VECTOR ? &uavDesc2 :
                &uavDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&buffers[i])));


            D3D12_CPU_DESCRIPTOR_HANDLE uavDescriptorHandle;
            heapIndices[i] = AllocateDescriptor(&uavDescriptorHandle, heapIndices[i]);
            D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
            UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            device->CreateUnorderedAccessView(buffers[i].Get(), nullptr, &UAVDesc, uavDescriptorHandle);
            descriptors[i] = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_descriptorHeap->GetGPUDescriptorHandleForHeapStart(), heapIndices[i], m_descriptorSize);
        }
    }

    void BuildGeometry()
    {
        auto device = m_deviceResources->GetD3DDevice();

        GeometryGenerator geoGen;
        Material nullMaterial;
        nullMaterial.id = 0;

        XMFLOAT3 sizes[] = {
            XMFLOAT3(30.0f, 15.0f, 30.0f),
            XMFLOAT3(20.0f, 0.01f, 0.01f),
            XMFLOAT3(0.0f, 0.0f, 0.0f)
        };

        lua_State* L = luaL_newstate();
        luaL_openlibs(L);

        lua_register(L, "HostFunction", lua_HostFunction);

        if (CheckLua(L, luaL_dofile(L, "LuaScripts/MyScript.lua"))) {
            lua_getglobal(L, "coordinatesSize");
            if (lua_istable(L, -1)) {
                lua_pushstring(L, "x");
                lua_gettable(L, -2);
                float x = (float)lua_tonumber(L, -1);
                lua_pop(L, 1);

                lua_pushstring(L, "y");
                lua_gettable(L, -2);
                float y = (float)lua_tonumber(L, -1);
                lua_pop(L, 1);

                lua_pushstring(L, "z");
                lua_gettable(L, -2);
                float z = (float)lua_tonumber(L, -1);
                lua_pop(L, 1);

                sizes[1] = XMFLOAT3(x, y, z);
            }
        }
        lua_close(L);

        MeshData(GeometryGenerator:: * createGeo[3])(float width, float height, float depth) = {
            &GeometryGenerator::CreateRoom, &GeometryGenerator::CreateCoordinates, &GeometryGenerator::CreateSkull
        };

        for (auto i : iota(0, 3)) {
            MeshData geo = (geoGen.*createGeo[i])(sizes[i].x, sizes[i].y, sizes[i].z);
            UINT roomVertexOffset = 0;
            UINT roomIndexOffset = 0;
            MeshGeometry::Submesh roomSubmesh;
            roomSubmesh.IndexCount = (UINT)geo.Indices32.size();
            roomSubmesh.StartIndexLocation = roomIndexOffset;
            roomSubmesh.VertexCount = (UINT)geo.Vertices.size();
            roomSubmesh.BaseVertexLocation = roomVertexOffset;
            roomSubmesh.Material = nullMaterial;
            std::vector<VertexPositionNormalTextureTangent> roomVertices(geo.Vertices.size());
            UINT k = 0;
            for (size_t i = 0; i < geo.Vertices.size(); ++i, ++k)
            {
                roomVertices[k].position = geo.Vertices[i].Position;
                roomVertices[k].normal = geo.Vertices[i].Normal;
                roomVertices[k].textureCoordinate = geo.Vertices[i].TexC;
                roomVertices[k].tangent = geo.Vertices[i].TangentU;
            }
            std::vector<std::uint32_t> roomIndices;
            roomIndices.insert(roomIndices.end(), begin(geo.Indices32), end(geo.Indices32));
            const UINT roomibByteSize = (UINT)roomIndices.size() * sizeof(std::uint32_t);
            const UINT roomvbByteSize = (UINT)roomVertices.size() * sizeof(VertexPositionNormalTextureTangent);
            AllocateUploadBuffer(device, &roomIndices[0], roomibByteSize, &m_indexBuffer[i].resource);
            AllocateUploadBuffer(device, &roomVertices[0], roomvbByteSize, &m_vertexBuffer[i].resource);
            CreateBufferSRV(&m_indexBuffer[i], roomIndices.size(), sizeof(uint32_t));
            CreateBufferSRV(&m_vertexBuffer[i], roomVertices.size(), sizeof(roomVertices[0]));
            auto roomGeo = std::make_unique<MeshGeometry>();

            string geoName = "geo" + to_string(i);
            string meshName = "mesh" + to_string(i);
            roomGeo->Name = geoName;
            roomGeo->VertexBufferGPU = m_vertexBuffer[i].resource;
            roomGeo->IndexBufferGPU = m_indexBuffer[i].resource;
            roomGeo->VertexByteStride = sizeof(VertexPositionNormalTextureTangent);
            roomGeo->IndexFormat = DXGI_FORMAT_R32_UINT;
            roomGeo->VertexBufferByteSize = roomvbByteSize;
            roomGeo->IndexBufferByteSize = roomibByteSize;
            roomGeo->DrawArgs[meshName] = roomSubmesh;
            m_geometries[roomGeo->Name] = std::move(roomGeo);
            m_geoOffset++;
        }

        m_meshOffsets.push_back(0);
        m_meshSizes.push_back(1);
        m_meshOffsets.push_back(m_meshOffsets.back() + m_meshSizes.back());
        m_meshSizes.push_back(1);
        m_meshOffsets.push_back(m_meshOffsets.back() + m_meshSizes.back());
        m_meshSizes.push_back(1);

        char dir[200];
        GetCurrentDirectoryA(sizeof(dir), dir);

        TCHAR buffer[MAX_PATH] = { 0 };
        GetModuleFileName(NULL, buffer, MAX_PATH);
        std::wstring::size_type pos = std::wstring(buffer).find_last_of(L"\\/");
        wstring path = std::wstring(buffer).substr(0, pos);

        string s(path.begin(), path.end());
        s.append("\\..\\..\\Models\\table.obj");
        string s2(path.begin(), path.end());
        s2.append("\\..\\..\\Models\\lamp.obj");
        string s3(path.begin(), path.end());
        s3.append("\\..\\..\\Models\\SunTemple\\SunTemple.fbx");

        BuildModel(s, aiProcess_Triangulate | aiProcess_FlipUVs, false);
        BuildModel(s2, aiProcess_Triangulate | aiProcess_FlipUVs, false);
        BuildModel(s3, aiProcess_Triangulate | aiProcess_FlipUVs, true);
    }

    void BuildGeometryDescsForBottomLevelAS(std::array<std::vector<D3D12_RAYTRACING_GEOMETRY_DESC>, BottomLevelASType::Count>& geometryDescs) {
        D3D12_RAYTRACING_GEOMETRY_DESC triangleDescTemplate{};
        triangleDescTemplate.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        triangleDescTemplate.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
        triangleDescTemplate.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
        triangleDescTemplate.Triangles.VertexBuffer.StrideInBytes = sizeof(VertexPositionNormalTextureTangent);
        triangleDescTemplate.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

        for (auto i : iota(2, 6)) {
            geometryDescs[i].resize(m_meshSizes[i], triangleDescTemplate);
            for (auto j : iota(0, m_meshSizes[i])) {
                string geoName = "geo" + to_string(m_meshOffsets[i] + j);
                string meshName = "mesh" + to_string(m_meshOffsets[i] + j);
                geometryDescs[i][j].Triangles.IndexBuffer = m_indexBuffer[m_meshOffsets[i] + j].resource->GetGPUVirtualAddress();
                geometryDescs[i][j].Triangles.VertexBuffer.StartAddress = m_vertexBuffer[m_meshOffsets[i] + j].resource->GetGPUVirtualAddress();
                geometryDescs[i][j].Triangles.IndexCount = m_geometries[geoName]->DrawArgs[meshName].IndexCount;
                geometryDescs[i][j].Triangles.VertexCount = m_geometries[geoName]->DrawArgs[meshName].VertexCount;
            }
        }
    }
   
    template <class InstanceDescType, class BLASPtrType> void BuildBottomLevelASInstanceDescs(BLASPtrType* bottomLevelASaddresses, Microsoft::WRL::ComPtr<ID3D12Resource>* instanceDescsResource) {
        Transform transforms[] = {
            { XMFLOAT3(500.0f, 0.0f, 0.0f),     XMFLOAT3(1.0f, 1.0f, 1.0f),     0 },
            { XMFLOAT3(0.0f, 0.0f, 0.0f),       XMFLOAT3(1.0f, 1.0f, 1.0f),     0 },
            { XMFLOAT3(0.5f, 0.6f, -0.5f),      XMFLOAT3(0.15f, 0.15f, 0.15f),  45 },
            { XMFLOAT3(-2.75f, -3.3f, -4.75f),  XMFLOAT3(0.05f, 0.05f, 0.05f),  0 },
            { XMFLOAT3(0.5f, 0.6f, 1.0f),       XMFLOAT3(0.05f, 0.05f, 0.05f),  180 },
            { XMFLOAT3(0.0f, -5.0f, 210.0f),    XMFLOAT3(0.05f, 0.05f, 0.05f),  180 }
        };

        vector<InstanceDescType> instanceDescs(NUM_BLAS);
        for (int i = 0; i < GeometryType::Count; i++) {
            instanceDescs[i] = {};
            instanceDescs[i].InstanceMask = 1;
            instanceDescs[i].InstanceContributionToHitGroupIndex = m_meshOffsets[i] * RayType::Count;
            instanceDescs[i].AccelerationStructure = bottomLevelASaddresses[i];

            XMMATRIX scale = XMMatrixScaling(transforms[i].scale.x, transforms[i].scale.y, transforms[i].scale.z);
            XMMATRIX rotation = XMMatrixRotationY(XMConvertToRadians(transforms[i].rotation));
            XMMATRIX translation = XMMatrixTranslationFromVector(XMLoadFloat3(&transforms[i].translation));
            XMStoreFloat3x4(reinterpret_cast<XMFLOAT3X4*>(instanceDescs[i].Transform), scale * rotation * translation);
        }

        UINT64 bufferSize = static_cast<UINT64>(instanceDescs.size() * sizeof(instanceDescs[0]));
        AllocateUploadBuffer(m_deviceResources->GetD3DDevice(), instanceDescs.data(), bufferSize, &(*instanceDescsResource), L"InstanceDescs");
    };

    AccelerationStructureBuffers BuildBottomLevelAS(const std::vector<D3D12_RAYTRACING_GEOMETRY_DESC>& geometryDescs, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE) {
        auto device = m_deviceResources->GetD3DDevice();
        auto commandList = m_deviceResources->GetCommandList();
        Microsoft::WRL::ComPtr<ID3D12Resource> scratch;
        Microsoft::WRL::ComPtr<ID3D12Resource> bottomLevelAS;

        // Get the size requirements for the scratch and AS buffers.
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& bottomLevelInputs = bottomLevelBuildDesc.Inputs;
        bottomLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
        bottomLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        bottomLevelInputs.Flags = buildFlags;
        bottomLevelInputs.NumDescs = static_cast<UINT>(geometryDescs.size());
        bottomLevelInputs.pGeometryDescs = geometryDescs.data();

        // Get maximum required space for data, scratch, and update buffers for these geometries with these parameters. You have to know this before creating the buffers, so that the are big enough.
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottomLevelPrebuildInfo = {};
        m_dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&bottomLevelInputs, &bottomLevelPrebuildInfo);
        ThrowIfFalse(bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

        // Create a scratch buffer.
        AllocateUAVBuffer(device, bottomLevelPrebuildInfo.ScratchDataSizeInBytes, &scratch, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");

        D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
        AllocateUAVBuffer(device, bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes, &bottomLevelAS, initialResourceState, L"BottomLevelAccelerationStructure");

        // bottom-level AS desc.
        bottomLevelBuildDesc.ScratchAccelerationStructureData = scratch->GetGPUVirtualAddress();
        bottomLevelBuildDesc.DestAccelerationStructureData = bottomLevelAS->GetGPUVirtualAddress();

        // Build the acceleration structure.
        m_dxrCommandList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);

        AccelerationStructureBuffers bottomLevelASBuffers;
        bottomLevelASBuffers.accelerationStructure = bottomLevelAS;
        bottomLevelASBuffers.scratch = scratch;
        bottomLevelASBuffers.ResultDataMaxSizeInBytes = bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes;
        return bottomLevelASBuffers;
    }

    AccelerationStructureBuffers BuildTopLevelAS(AccelerationStructureBuffers bottomLevelAS[BottomLevelASType::Count], D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE)
    {
        auto device = m_deviceResources->GetD3DDevice();
        auto commandList = m_deviceResources->GetCommandList();
        Microsoft::WRL::ComPtr<ID3D12Resource> scratch;
        Microsoft::WRL::ComPtr<ID3D12Resource> topLevelAS;

        // Get required sizes for an acceleration structure.
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelBuildDesc = {};
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& topLevelInputs = topLevelBuildDesc.Inputs;
        topLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
        topLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        topLevelInputs.Flags = buildFlags;
        topLevelInputs.NumDescs = NUM_BLAS;

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topLevelPrebuildInfo = {};
        m_dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&topLevelInputs, &topLevelPrebuildInfo);
        ThrowIfFalse(topLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

        AllocateUAVBuffer(device, topLevelPrebuildInfo.ScratchDataSizeInBytes, &scratch, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");
        {
            D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
            AllocateUAVBuffer(device, topLevelPrebuildInfo.ResultDataMaxSizeInBytes, &topLevelAS, initialResourceState, L"TopLevelAccelerationStructure");
        }

        // Create instance descs for the bottom-level acceleration structures.
        Microsoft::WRL::ComPtr<ID3D12Resource> instanceDescsResource;
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
            BuildBottomLevelASInstanceDescs<D3D12_RAYTRACING_INSTANCE_DESC>(bottomLevelASaddresses, &instanceDescsResource);
        }

        // Top-level AS desc
        {
            topLevelBuildDesc.DestAccelerationStructureData = topLevelAS->GetGPUVirtualAddress();
            topLevelInputs.InstanceDescs = instanceDescsResource->GetGPUVirtualAddress();
            topLevelBuildDesc.ScratchAccelerationStructureData = scratch->GetGPUVirtualAddress();
        }

        // Build acceleration structure.
        m_dxrCommandList->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);

        AccelerationStructureBuffers topLevelASBuffers;
        topLevelASBuffers.accelerationStructure = topLevelAS;
        topLevelASBuffers.instanceDesc = instanceDescsResource;
        topLevelASBuffers.scratch = scratch;
        topLevelASBuffers.ResultDataMaxSizeInBytes = topLevelPrebuildInfo.ResultDataMaxSizeInBytes;
        return topLevelASBuffers;
    }

    void BuildAccelerationStructures() {
        auto device = m_deviceResources->GetD3DDevice();
        auto commandList = m_deviceResources->GetCommandList();
        auto commandQueue = m_deviceResources->GetCommandQueue();
        auto commandAllocator = m_deviceResources->GetCommandAllocator();

        // Build bottom-level AS.
        AccelerationStructureBuffers bottomLevelAS[BottomLevelASType::Count];
        array<vector<D3D12_RAYTRACING_GEOMETRY_DESC>, BottomLevelASType::Count> geometryDescs;
        {
            BuildGeometryDescsForBottomLevelAS(geometryDescs);
            // Build all bottom-level AS.
            for (UINT i = 0; i < BottomLevelASType::Count; i++)
                bottomLevelAS[i] = BuildBottomLevelAS(geometryDescs[i]);
        }

        // Batch all resource barriers for bottom-level AS builds.
        D3D12_RESOURCE_BARRIER resourceBarriers[BottomLevelASType::Count];
        for (UINT i = 0; i < BottomLevelASType::Count; i++)
        {
            resourceBarriers[i] = CD3DX12_RESOURCE_BARRIER::UAV(bottomLevelAS[i].accelerationStructure.Get());
        }
        commandList->ResourceBarrier(BottomLevelASType::Count, resourceBarriers);

        // Build top-level AS.
        AccelerationStructureBuffers topLevelAS = BuildTopLevelAS(bottomLevelAS);

        TCHAR buffer[MAX_PATH] = { 0 };
        GetModuleFileName(NULL, buffer, MAX_PATH);
        std::wstring::size_type pos = std::wstring(buffer).find_last_of(L"\\/");
        wstring path = std::wstring(buffer).substr(0, pos);

        wstring s(path.begin(), path.end());
        s.append(L"\\..\\..\\Models\\SunTemple\\Textures\\M_Arch_Inst_0_BaseColor.dds");

        string s4(path.begin(), path.end());
        s4.append("\\..\\..\\Models\\SunTemple\\Textures\\");

        m_stoneTexture[0].heapIndex = 3000 + 1;
        LoadDDSTexture(device, commandList, s.c_str(), m_descriptorHeap.get(), &m_stoneTexture[0].resource, &m_stoneTexture[0].upload, &m_stoneTexture[0].heapIndex, &m_stoneTexture[0].cpuDescriptorHandle, &m_stoneTexture[0].gpuDescriptorHandle, D3D12_SRV_DIMENSION_TEXTURE2D);


        std::vector<int> included;
        for (UINT i = 0; i < 1056; i++)
        {
            string geoName = "geo" + to_string(i + 11);
            string meshName = "mesh" + to_string(i + 11);


            int key = m_geometries[geoName]->DrawArgs[meshName].Material.id;


            if (std::find(included.begin(), included.end(), key) != included.end()) {
            }
            else {
                m_materials[key] = m_geometries[geoName]->DrawArgs[meshName].Material;
                included.push_back(key);
            }
        }

        for (int i = 0; i < m_materials.size(); i++) {
            string base = "..\\..\\Models\\SunTemple\\Textures\\";
            base = s4;
            string add = m_materials[i].map_Kd;
            string normalMapPath = m_materials[i].map_Bump;
            string specularMapPath = m_materials[i].map_Ks;
            string emittanceMapPath = m_materials[i].map_Ke;

            std::size_t pos = add.find("\\");
            std::string str3 = add.substr(pos + 1);
            string path = base + str3;
            print(path);
            if (add != "")
                LoadDDSTexture(device, commandList, wstring(path.begin(), path.end()).c_str(), m_descriptorHeap.get(), &m_templeTextures[i]);

            std::size_t pos2 = normalMapPath.find("\\");
            std::string str4 = normalMapPath.substr(pos2 + 1);
            string path2 = base + str4;
            print(path2);
            if (normalMapPath != "")
                LoadDDSTexture(device, commandList, wstring(path2.begin(), path2.end()).c_str(), m_descriptorHeap.get(), &m_templeNormalTextures[i]);

            std::size_t pos3 = specularMapPath.find("\\");
            std::string str5 = specularMapPath.substr(pos3 + 1);
            string path3 = base + str5;
            print(path3);
            if (specularMapPath != "")
                LoadDDSTexture(device, commandList, wstring(path3.begin(), path3.end()).c_str(), m_descriptorHeap.get(), &m_templeSpecularTextures[i]);

            std::size_t pos4 = emittanceMapPath.find("\\");
            std::string str6 = emittanceMapPath.substr(pos4 + 1);
            string path4 = base + str6;
            print(path4);
            if (emittanceMapPath != "")
                LoadDDSTexture(device, commandList, wstring(path4.begin(), path4.end()).c_str(), m_descriptorHeap.get(), &m_templeEmittanceTextures[i]);
        }

        // Kick off acceleration structure construction.
        m_deviceResources->ExecuteCommandList();

        // Wait for GPU to finish as the locally created temporary GPU resources will get released once we go out of scope.
        m_deviceResources->WaitForGpu();

        // Store the AS buffers. The rest of the buffers will be released once we exit the function.
        for (UINT i = 0; i < BottomLevelASType::Count; i++)
        {
            m_bottomLevelAS[i] = bottomLevelAS[i].accelerationStructure;
        }
        m_topLevelAS = topLevelAS.accelerationStructure;
    }

    void BuildShaderTables()
    {
        auto device = m_deviceResources->GetD3DDevice();

        void* rayGenShaderID;
        void* missShaderIDs[RayType::Count];
        void* hitGroupShaderIDs_TriangleGeometry[RayType::Count];

        // A shader name look-up table for shader table debug print out.
        unordered_map<void*, wstring> shaderIdToStringMap;

        auto GetShaderIDs = [&](auto* stateObjectProperties)
        {
            rayGenShaderID = stateObjectProperties->GetShaderIdentifier(c_raygenShaderName);
            shaderIdToStringMap[rayGenShaderID] = c_raygenShaderName;

            for (UINT i = 0; i < RayType::Count; i++)
            {
                missShaderIDs[i] = stateObjectProperties->GetShaderIdentifier(c_missShaderNames[i]);
                shaderIdToStringMap[missShaderIDs[i]] = c_missShaderNames[i];
            }
            for (UINT i = 0; i < RayType::Count; i++)
            {
                hitGroupShaderIDs_TriangleGeometry[i] = stateObjectProperties->GetShaderIdentifier(c_hitGroupNames_TriangleGeometry[i]);
                shaderIdToStringMap[hitGroupShaderIDs_TriangleGeometry[i]] = c_hitGroupNames_TriangleGeometry[i];
            }
        };

        // Get shader identifiers.
        UINT shaderIDSize;
        {
            Microsoft::WRL::ComPtr<ID3D12StateObjectProperties> stateObjectProperties;
            ThrowIfFailed(m_dxrStateObject.As(&stateObjectProperties));
            GetShaderIDs(stateObjectProperties.Get());
            shaderIDSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
        }

        // RayGen shader table.
        {
            UINT numShaderRecords = 1;
            UINT shaderRecordSize = shaderIDSize; // No root arguments

            ShaderTable rayGenShaderTable(device, numShaderRecords, shaderRecordSize, L"RayGenShaderTable");
            rayGenShaderTable.push_back(ShaderRecord(rayGenShaderID, shaderRecordSize, nullptr, 0));
            rayGenShaderTable.DebugPrint(shaderIdToStringMap);
            m_rayGenShaderTable = rayGenShaderTable.GetResource();
        }

        // Miss shader table.
        {
            UINT numShaderRecords = RayType::Count;
            UINT shaderRecordSize = shaderIDSize; // No root arguments

            ShaderTable missShaderTable(device, numShaderRecords, shaderRecordSize, L"MissShaderTable");
            for (UINT i = 0; i < RayType::Count; i++)
            {
                missShaderTable.push_back(ShaderRecord(missShaderIDs[i], shaderIDSize, nullptr, 0));
            }
            missShaderTable.DebugPrint(shaderIdToStringMap);
            m_missShaderTableStrideInBytes = missShaderTable.GetShaderRecordSize();
            m_missShaderTable = missShaderTable.GetResource();
        }

        // Hit group shader table.
        {
            UINT numShaderRecords = NUM_BLAS * RayType::Count;
            UINT shaderRecordSize = shaderIDSize + LocalRootSignature::MaxRootArgumentsSize();
            ShaderTable hitGroupShaderTable(device, numShaderRecords, shaderRecordSize, L"HitGroupShaderTable");

            for (auto i : iota(0, 6))
            {
                LocalRootSignature::Triangle::RootArguments rootArgs;

                // Create a shader record for each primitive.
                for (UINT instanceIndex = 0; instanceIndex < m_meshSizes[i]; instanceIndex++)
                {
                    rootArgs.materialCb = m_triangleMaterialCB[instanceIndex + m_meshOffsets[i]];
                    rootArgs.triangleCB.instanceIndex = instanceIndex + m_meshOffsets[i];
                    auto ib = m_indexBuffer[instanceIndex + m_meshOffsets[i]].gpuDescriptorHandle;
                    auto vb = m_vertexBuffer[instanceIndex + m_meshOffsets[i]].gpuDescriptorHandle;
                    string geoName = "geo" + to_string(instanceIndex + m_meshOffsets[i]);
                    string meshName = "mesh" + to_string(instanceIndex + m_meshOffsets[i]);
                    Material m = m_geometries[geoName]->DrawArgs[meshName].Material;
                    auto texture = m_templeTextures[m.id].gpuDescriptorHandle;
                    auto normalTexture = m_templeNormalTextures[m.id].gpuDescriptorHandle;
                    auto specularTexture = m_templeSpecularTextures[m.id].gpuDescriptorHandle;
                    auto emittanceTexture = m_templeEmittanceTextures[m.id].gpuDescriptorHandle;
                    memcpy(&rootArgs.indexBufferGPUHandle, &ib, sizeof(ib));
                    memcpy(&rootArgs.vertexBufferGPUHandle, &vb, sizeof(ib));
                    memcpy(&rootArgs.diffuseTextureGPUHandle, &texture, sizeof(ib));
                    memcpy(&rootArgs.normalTextureGPUHandle, &normalTexture, sizeof(ib));
                    memcpy(&rootArgs.specularTextureGPUHandle, &specularTexture, sizeof(ib));
                    memcpy(&rootArgs.emittanceTextureGPUHandle, &emittanceTexture, sizeof(ib));

                    // Ray types
                    for (UINT r = 0; r < RayType::Count; r++)
                    {
                        auto& hitGroupShaderID = hitGroupShaderIDs_TriangleGeometry[r];
                        hitGroupShaderTable.push_back(ShaderRecord(hitGroupShaderID, shaderIDSize, &rootArgs, sizeof(rootArgs)));
                    }
                }
            }

            hitGroupShaderTable.DebugPrint(shaderIdToStringMap);
            m_hitGroupShaderTableStrideInBytes = hitGroupShaderTable.GetShaderRecordSize();
            m_hitGroupShaderTable = hitGroupShaderTable.GetResource();
        }
    }

    void UpdateForSizeChange(UINT width, UINT height) {
        DXCore::UpdateForSizeChange(width, height);
    }

    void CopyRaytracingOutputToBackbuffer() {
        auto commandList = m_deviceResources->GetCommandList();
        auto renderTarget = m_deviceResources->GetRenderTarget();

        D3D12_RESOURCE_BARRIER preCopyBarriers[2];
        preCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(renderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);
        preCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(buffers[RAYTRACING].Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
        commandList->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);

        commandList->CopyResource(renderTarget, buffers[RAYTRACING].Get());

        D3D12_RESOURCE_BARRIER postCopyBarriers[2];
        postCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(renderTarget, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);
        postCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(buffers[RAYTRACING].Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        commandList->ResourceBarrier(ARRAYSIZE(postCopyBarriers), postCopyBarriers);
    }

    void CalculateFrameStats() {
        static int frameCnt = 0;
        static double prevTime = 0.0f;
        double totalTime = m_timer.GetTotalSeconds();

        frameCnt++;

        // Compute averages over one second period.
        if ((totalTime - prevTime) >= 1.0f)
        {
            float diff = static_cast<float>(totalTime - prevTime);
            float fps = static_cast<float>(frameCnt) / diff; // Normalize to an exact second.

            frameCnt = 0;
            prevTime = totalTime;
            float raytracingTime = static_cast<float>(m_gpuTimers[GpuTimers::Raytracing].GetElapsedMS());
            float MRaysPerSecond = NumMRaysPerSecond(m_width, m_height, raytracingTime);

            wstringstream windowText;
            windowText << setprecision(2) << fixed
                << L"    FPS: " << fps
                << L"    Raytracing time: " << raytracingTime << " ms"
                << L"    Ray throughput: " << MRaysPerSecond << " MRPS"
                << L"    GPU[" << m_deviceResources->GetAdapterID() << L"]: " << m_deviceResources->GetAdapterDescription();
            /*SetCustomWindowText(windowText.str().c_str(), hwnd);*/
        }
    }

    UINT AllocateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* cpuDescriptor, UINT descriptorIndexToUse = UINT_MAX) {
        auto descriptorHeapCpuBase = m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();
        if (descriptorIndexToUse >= m_descriptorHeap->GetDesc().NumDescriptors)
        {
            ThrowIfFalse(m_descriptorsAllocated < m_descriptorHeap->GetDesc().NumDescriptors, L"Ran out of descriptors on the heap!");
            descriptorIndexToUse = m_descriptorsAllocated++;
        }
        *cpuDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(descriptorHeapCpuBase, descriptorIndexToUse, m_descriptorSize);
        return descriptorIndexToUse;
    }

    UINT CreateBufferSRV(D3DBuffer* buffer, UINT numElements, UINT elementSize) {
        auto device = m_deviceResources->GetD3DDevice();

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
        UINT descriptorIndex = AllocateDescriptor(&buffer->cpuDescriptorHandle);
        device->CreateShaderResourceView(buffer->resource.Get(), &srvDesc, buffer->cpuDescriptorHandle);
        buffer->gpuDescriptorHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_descriptorHeap->GetGPUDescriptorHandleForHeapStart(), descriptorIndex, m_descriptorSize);
        return descriptorIndex;
    }

    UINT CreateTextureSRV(UINT numElements, UINT elementSize) {
        auto device = m_deviceResources->GetD3DDevice();

        auto srvDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, 512, 512, 1, 1, 1, 0);

        auto stoneTex = m_textures["stoneTex"]->Resource;

        auto defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        ThrowIfFailed(device->CreateCommittedResource(
            &defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &srvDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&stoneTex)));

        D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
        SRVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        SRVDesc.Texture2D.MostDetailedMip = 0;
        SRVDesc.Texture2D.MipLevels = 1;
        SRVDesc.Texture2D.PlaneSlice = 0;
        SRVDesc.Texture2D.ResourceMinLODClamp = 0.0f;

        CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());

        UINT descriptorIndex = AllocateDescriptor(&hDescriptor);
        device->CreateShaderResourceView(stoneTex.Get(), &SRVDesc, hDescriptor);
        //texture->gpuDescriptorHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_descriptorHeap->GetGPUDescriptorHandleForHeapStart(), descriptorIndex, m_descriptorSize);
        return descriptorIndex;
    }

    static const UINT FrameCount = 3;

    static const UINT NUM_BLAS = 1090;
    const float c_aabbWidth = 2;
    const float c_aabbDistance = 2;
    
    UINT m_geoOffset = 0;
  
    Microsoft::WRL::ComPtr<ID3D12Device5> m_dxrDevice;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList5> m_dxrCommandList;
    Microsoft::WRL::ComPtr<ID3D12StateObject> m_dxrStateObject;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_composePSO[1];
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_blurPSO[1];

    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_raytracingGlobalRootSignature;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_raytracingLocalRootSignature[LocalRootSignature::Type::Count];
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_composeRootSig;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_blurRootSig;

    std::shared_ptr<DescriptorHeap> m_descriptorHeap;
    UINT m_descriptorsAllocated;
    UINT m_descriptorSize;

    ConstantBuffer<SceneConstantBuffer> m_sceneCB;
    ConstantBuffer<AtrousWaveletTransformFilterConstantBuffer> m_filterCB;
    StructuredBuffer<PrimitiveInstancePerFrameBuffer> m_aabbPrimitiveAttributeBuffer;
    StructuredBuffer<PrimitiveInstancePerFrameBuffer> m_trianglePrimitiveAttributeBuffer;
    std::vector<D3D12_RAYTRACING_AABB> m_aabbs;

    PrimitiveConstantBuffer m_triangleMaterialCB[NUM_BLAS];

    std::vector<Vertex> m_vertices;
    std::vector<Index> m_indices;
    D3DBuffer m_indexBuffer[NUM_BLAS];
    D3DBuffer m_vertexBuffer[NUM_BLAS];
    D3DBuffer m_aabbBuffer;
    std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> m_geometries;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_bottomLevelAS[BottomLevelASType::Count];
    Microsoft::WRL::ComPtr<ID3D12Resource> m_topLevelAS;

    Microsoft::WRL::ComPtr<ID3D12Resource> buffers[COUNT];
    D3D12_GPU_DESCRIPTOR_HANDLE descriptors[COUNT];
   
    static inline const wchar_t* c_raygenShaderName = L"MyRaygenShader";
    static inline const wchar_t* c_closestHitShaderName = L"MyClosestHitShader_Triangle";
    static inline const wchar_t* c_missShaderNames[] = { L"MyMissShader", L"MyMissShader_ShadowRay" };
    static inline const wchar_t* c_hitGroupNames_TriangleGeometry[] = { L"MyHitGroup_Triangle", L"MyHitGroup_Triangle_ShadowRay" };

    Microsoft::WRL::ComPtr<ID3D12Resource> m_missShaderTable;
    UINT m_missShaderTableStrideInBytes;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_hitGroupShaderTable;
    UINT m_hitGroupShaderTableStrideInBytes;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_rayGenShaderTable;

    DX::GPUTimer m_gpuTimers[GpuTimers::Count];
    StepTimer m_timer;
    float m_animateGeometryTime;
    bool m_animateGeometry;
    bool m_animateCamera;
    bool m_animateLight;
    XMVECTOR m_eye;
    XMVECTOR m_at;
    XMVECTOR m_up;
    bool m_orbitalCamera = false;

    POINT m_lastMousePosition = {};

    std::shared_ptr<DescriptorHeap> m_cbvSrvUavHeap;
    std::shared_ptr<DescriptorHeap> m_composeHeap;
    std::shared_ptr<DescriptorHeap> m_blurHeap;

    std::unordered_map<std::string, std::unique_ptr<Texture>> m_textures;
    D3DTexture m_stoneTexture[3];
    D3DTexture m_templeTextures[500];
    D3DTexture m_templeNormalTextures[500];
    D3DTexture m_templeSpecularTextures[500];
    D3DTexture m_templeEmittanceTextures[500];

    std::unordered_map<int, Material> m_materials;

    std::vector<int> m_meshSizes;
    std::vector<int> m_meshOffsets;
};