module;
#include "RaytracingHlslCompat.hlsli"
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
#include <Windows.h>
#include "RaytracingSceneDefines.h"
#include "PerformanceTimers.h"
export module Core;

import DXCore;
import StepTimer;
import Texture;
import Helper;
import Mesh;
import Directions;
import Descriptors;
import D3DBuffer;
import DescriptorHeap;
import ConstantBuffer;
import StructuredBuffer;
import D3DTexture;
import DeviceResources;
import Application;
import DXSampleHelper;
import Material;
import RenderingComponent;
import InputComponent;
import UpdateInterface;
import InitComponent;
import InitInterface;
import ResourceComponent;
import CameraComponent;
import CoreInterface;
import ShaderTableComponent;
import RootSignatureComponent;
import OutputComponent;
import GeometryComponent;
import AccelerationStructureComponent;
import DescriptorComponent;
import BufferComponent;
import ShaderTableComponent;

using namespace std;
using namespace Microsoft::WRL;

export class Core : public DXCore, public CoreInterface {
    static const UINT FrameCount = 3;

    static const UINT NUM_BLAS = 1090;
    const float c_aabbWidth = 2;
    const float c_aabbDistance = 2;

    UINT m_geoOffset = 0;

    ComPtr<ID3D12Device5> m_dxrDevice;
    ComPtr<ID3D12GraphicsCommandList5> m_dxrCommandList;
    ComPtr<ID3D12StateObject> m_dxrStateObject;
    ComPtr<ID3D12PipelineState> m_composePSO[1];
    ComPtr<ID3D12PipelineState> m_blurPSO[1];

    ComPtr<ID3D12RootSignature> m_raytracingGlobalRootSignature;
    vector<ComPtr<ID3D12RootSignature>> m_raytracingLocalRootSignature;
    ComPtr<ID3D12RootSignature> m_composeRootSig;
    ComPtr<ID3D12RootSignature> m_blurRootSig;

    shared_ptr<DescriptorHeap> m_descriptorHeap;
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

    ComPtr<ID3D12Resource> m_bottomLevelAS[BottomLevelASType::Count];
    ComPtr<ID3D12Resource> m_topLevelAS;

    ComPtr<ID3D12Resource> buffers[Descriptors::COUNT];
    D3D12_GPU_DESCRIPTOR_HANDLE descriptors[Descriptors::COUNT];

    static inline const wchar_t* c_raygenShaderName = L"MyRaygenShader";
    static inline const wchar_t* c_closestHitShaderName = L"MyClosestHitShader_Triangle";
    static inline const wchar_t* c_missShaderNames[] = { L"MyMissShader", L"MyMissShader_ShadowRay" };
    static inline const wchar_t* c_hitGroupNames_TriangleGeometry[] = { L"MyHitGroup_Triangle", L"MyHitGroup_Triangle_ShadowRay" };

    ComPtr<ID3D12Resource> m_missShaderTable;
    UINT m_missShaderTableStrideInBytes;
    ComPtr<ID3D12Resource> m_hitGroupShaderTable;
    UINT m_hitGroupShaderTableStrideInBytes;
    ComPtr<ID3D12Resource> m_rayGenShaderTable;

    vector<DX::GPUTimer> m_gpuTimers;
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

    RenderingComponent* renderingComponent;
    InputComponent* inputComponent;
    InitInterface* initComponent;
    UpdateInterface* updateComponent;
    ResourceComponent* resourceComponent;
    CameraComponent* cameraComponent;
    ShaderTableComponent* shaderTableComponent;
    RootSignatureComponent* rootSignatureComponent;
    OutputComponent* outputComponent;
    GeometryComponent* geometryComponent;
    AccelerationStructureComponent* accelerationStructureComponent;
    DescriptorComponent* descriptorComponent;
    BufferComponent* bufferComponent;

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
        m_raytracingLocalRootSignature.resize(LocalRootSignature::Type::Count);
        UpdateForSizeChange(width, height);
    }

    virtual void OnInit() {
        m_deviceResources = std::make_unique<DeviceResources>(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_UNKNOWN, FrameCount,
            D3D_FEATURE_LEVEL_11_0, DeviceResources::c_RequireTearingSupport, m_adapterIDoverride);
        m_deviceResources->RegisterDeviceNotify(this);
        m_deviceResources->SetWindow(Application::GetHwnd(), m_width, m_height);
        m_deviceResources->InitializeDXGIAdapter();

        ThrowIfFalse(IsDirectXRaytracingSupported(m_deviceResources->GetAdapter()),
            L"ERROR: DirectX Raytracing is not supported by your OS, GPU and/or driver.\n\n");
        XMFLOAT3 pos;
        XMStoreFloat3(&pos, m_camera.GetPosition());
        rootSignatureComponent = new RootSignatureComponent(
            m_deviceResources.get(),
            m_raytracingLocalRootSignature,
            m_raytracingGlobalRootSignature,
            c_hitGroupNames_TriangleGeometry
        );
        m_deviceResources->CreateDeviceResources();
        m_deviceResources->CreateWindowSizeDependentResources();
        initComponent = new InitComponent(
            m_deviceResources.get(),
            m_triangleMaterialCB,
            &pos,
            FrameCount,
            m_adapterIDoverride,
            m_width,
            m_height,
            renderingComponent,
            &m_sceneCB,
            m_camera,
            cameraComponent,
            m_at,
            m_up,
            m_eye,
            this,
            resourceComponent,
            m_rayGenShaderTable,
            m_hitGroupShaderTable,
            m_missShaderTable,
            m_hitGroupShaderTableStrideInBytes,
            m_missShaderTableStrideInBytes,
            &m_gpuTimers,
            descriptors,
            m_descriptorHeap.get(),
            m_raytracingGlobalRootSignature.Get(),
            m_topLevelAS.Get(),
            m_dxrStateObject.Get(),
            &m_trianglePrimitiveAttributeBuffer,
            m_orbitalCamera,
            m_aspectRatio
        );
        shaderTableComponent = new ShaderTableComponent(
            m_deviceResources.get(),
            c_hitGroupNames_TriangleGeometry,
            c_missShaderNames,
            c_closestHitShaderName,
            c_raygenShaderName,
            m_hitGroupShaderTable,
            m_dxrStateObject,
            m_missShaderTableStrideInBytes,
            &m_meshOffsets,
            m_indexBuffer,
            m_vertexBuffer,
            &m_geometries,
            &m_meshSizes,
            m_templeTextures,
            m_templeNormalTextures,
            m_templeSpecularTextures,
            m_templeEmittanceTextures,
            NUM_BLAS,
            m_hitGroupShaderTableStrideInBytes,
            m_triangleMaterialCB,
            m_rayGenShaderTable,
            m_missShaderTable
        );
        resourceComponent = new ResourceComponent(
            m_triangleMaterialCB,
            FrameCount,
            &m_gpuTimers,
            shaderTableComponent,
            initComponent,
            rootSignatureComponent,
            m_deviceResources.get(),
            &m_geometries,
            cameraComponent,
            m_raytracingGlobalRootSignature,
            m_raytracingLocalRootSignature,
            m_dxrDevice,
            m_dxrCommandList,
            m_dxrStateObject,
            m_descriptorHeap,
            m_descriptorsAllocated,
            m_sceneCB,
            m_trianglePrimitiveAttributeBuffer,
            m_bottomLevelAS,
            m_topLevelAS,
            buffers,
            m_rayGenShaderTable,
            m_missShaderTable,
            m_hitGroupShaderTable,
            outputComponent,
            geometryComponent,
            bufferComponent,
            accelerationStructureComponent,
            descriptorComponent
        );
        
        initComponent->InitializeScene();
        resourceComponent->CreateDeviceDependentResources();

        resourceComponent->CreateWindowSizeDependentResources();

        renderingComponent = new RenderingComponent{
           m_deviceResources.get(),
           m_hitGroupShaderTable.Get(),
           m_missShaderTable.Get(),
           m_rayGenShaderTable.Get(),
           m_hitGroupShaderTableStrideInBytes,
           m_missShaderTableStrideInBytes,
           m_width,
           m_height,
           &m_gpuTimers,
           descriptors,
           m_descriptorHeap.get(),
           &m_camera,
           m_raytracingGlobalRootSignature.Get(),
           m_topLevelAS.Get(),
           m_dxrCommandList.Get(),
           m_dxrStateObject.Get(),
           &m_sceneCB,
           &m_trianglePrimitiveAttributeBuffer
        };
    }

    virtual void OnUpdate() {
        updateComponent->OnUpdate();
    }

    virtual void OnRender() {
        renderingComponent->OnRender();
    }

    virtual void OnSizeChanged(UINT width, UINT height, bool minimized) {
        updateComponent->OnSizeChanged(width, height, minimized);
    }

    virtual void OnDestroy() {
        // Let GPU finish before releasing D3D resources.
        m_deviceResources->WaitForGpu();
        updateComponent->OnDeviceLost();
    }

    void OnMouseMove(int x, int y) override {
        inputComponent->OnMouseMove(x, y);
    }

    void OnLeftButtonDown(UINT x, UINT y) override {
        inputComponent->OnLeftButtonDown(x, y);
    }

    virtual IDXGISwapChain* GetSwapchain() { return m_deviceResources->GetSwapChain(); }

    virtual void OnDeviceLost() {
        updateComponent->OnDeviceLost();
    }

    virtual void OnDeviceRestored() {
        updateComponent->OnDeviceRestored();
    }
};