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
import ShaderTableComponent;
import SrvComponent;
import BottomLevelASComponent;
import TopLevelASComponent;
import UpdateComponent;
import PerformanceComponent;
import ShaderComponent;

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
    vector<ComPtr<ID3D12PipelineState>> m_composePSO;
    vector<ComPtr<ID3D12PipelineState>> m_blurPSO;

    ComPtr<ID3D12RootSignature> m_raytracingGlobalRootSignature;
    vector<ComPtr<ID3D12RootSignature>> m_raytracingLocalRootSignature;
    ComPtr<ID3D12RootSignature> m_composeRootSig;
    ComPtr<ID3D12RootSignature> m_blurRootSig;

    DescriptorHeap* m_descriptorHeap;
    UINT m_descriptorsAllocated;
    UINT m_descriptorSize;

    ConstantBuffer<SceneConstantBuffer> m_sceneCB;
    ConstantBuffer<AtrousWaveletTransformFilterConstantBuffer> m_filterCB;
    StructuredBuffer<PrimitiveInstancePerFrameBuffer> m_aabbPrimitiveAttributeBuffer;
    StructuredBuffer<PrimitiveInstancePerFrameBuffer> m_trianglePrimitiveAttributeBuffer;
    std::vector<D3D12_RAYTRACING_AABB> m_aabbs;

    vector<PrimitiveConstantBuffer> m_triangleMaterialCB;

    std::vector<Vertex> m_vertices;
    std::vector<Index> m_indices;
    vector<D3DBuffer> m_indexBuffer;
    vector<D3DBuffer> m_vertexBuffer;
    D3DBuffer m_aabbBuffer;
    std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> m_geometries;

    vector<ComPtr<ID3D12Resource>> m_bottomLevelAS;
    ComPtr<ID3D12Resource> m_topLevelAS;

    vector<ComPtr<ID3D12Resource>> buffers;
    vector<D3D12_GPU_DESCRIPTOR_HANDLE> descriptors;

    inline static const wchar_t* c_raygenShaderName =L"MyRaygenShader";
    inline static const wchar_t* c_closestHitShaderName =L"MyClosestHitShader_Triangle";
    vector<const wchar_t*> c_missShaderNames = { L"MyMissShader", L"MyMissShader_ShadowRay" };
    vector<const wchar_t*> c_hitGroupNames_TriangleGeometry = { L"MyHitGroup_Triangle", L"MyHitGroup_Triangle_ShadowRay" };

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
    vector<D3DTexture> m_stoneTexture;
    vector<D3DTexture> m_templeTextures;
    vector<D3DTexture> m_templeNormalTextures;
    vector<D3DTexture> m_templeSpecularTextures;
    vector<D3DTexture> m_templeEmittanceTextures;

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
    SrvComponent* srvComponent;
    BottomLevelASComponent* bottomLevelASComponent;
    TopLevelASComponent* topLevelASComponent;
    PerformanceComponent* performanceComponent;
    ShaderComponent* shaderComponent;

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
        m_hitGroupShaderTableStrideInBytes(UINT_MAX),
        descriptors(Descriptors::COUNT),
        m_bottomLevelAS(BottomLevelASType::Count),
        m_gpuTimers(GpuTimers::Count),
        m_composePSO(1),
        m_blurPSO(1),
        buffers(Descriptors::COUNT),
        m_triangleMaterialCB(NUM_BLAS),
        m_indexBuffer(NUM_BLAS),
        m_vertexBuffer(NUM_BLAS),
        m_stoneTexture(3),
        m_templeTextures(500),
        m_templeNormalTextures(500),
        m_templeSpecularTextures(500),
        m_templeEmittanceTextures(500)
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
            m_deviceResources,
            m_raytracingLocalRootSignature,
            m_raytracingGlobalRootSignature,
            c_hitGroupNames_TriangleGeometry
        );
        m_deviceResources->CreateDeviceResources();
        cameraComponent = new CameraComponent(
            m_deviceResources,
            m_sceneCB,
            m_orbitalCamera,
            m_eye,
            m_at,
            m_up,
            m_aspectRatio,
            m_camera
        );

        m_deviceResources->CreateWindowSizeDependentResources();
        initComponent = new InitComponent(
            m_deviceResources,
            m_triangleMaterialCB,
            pos,
            FrameCount,
            m_adapterIDoverride,
            m_width,
            m_height,
            renderingComponent,
            m_sceneCB,
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
            m_gpuTimers,
            descriptors,
            m_descriptorHeap,
            m_raytracingGlobalRootSignature.Get(),
            m_topLevelAS.Get(),
            m_dxrStateObject.Get(),
            m_trianglePrimitiveAttributeBuffer,
            m_orbitalCamera,
            m_aspectRatio
        );

  
        resourceComponent = new ResourceComponent(
            m_triangleMaterialCB,
            FrameCount,
            m_gpuTimers,
            shaderTableComponent,
            initComponent,
            rootSignatureComponent,
            m_deviceResources.get(),
            m_geometries,
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
            accelerationStructureComponent,
            m_meshOffsets,
            m_meshSizes,
            m_geoOffset,

            m_indexBuffer,
            m_vertexBuffer,
            srvComponent,
            m_descriptorSize,
            m_textures,
            NUM_BLAS,
            m_materials,
            m_stoneTexture,
            m_templeTextures,
            m_templeNormalTextures,

            m_templeSpecularTextures,
            m_templeEmittanceTextures,
            bottomLevelASComponent,
            topLevelASComponent,
            m_filterCB,
            c_hitGroupNames_TriangleGeometry,
            c_missShaderNames,
            c_closestHitShaderName,
            c_raygenShaderName,

            m_missShaderTableStrideInBytes,
            m_hitGroupShaderTableStrideInBytes,
            m_width,
            m_height,
            descriptors,
            shaderComponent
        );

        performanceComponent = new PerformanceComponent(
            m_deviceResources.get(),
            m_width,
            m_height,
            m_timer,
            m_gpuTimers
        );

        
        
        initComponent->InitializeScene();
     
        resourceComponent->CreateDeviceDependentResources();

        resourceComponent->CreateWindowSizeDependentResources();

        updateComponent = new UpdateComponent(
            m_timer,
            m_deviceResources.get(),
            m_orbitalCamera,
            m_eye,
            m_up,
            m_at,
            m_animateLight,
            m_animateGeometry,
            m_animateGeometryTime,
            m_sceneCB,
            m_filterCB,
            performanceComponent,
            cameraComponent,
            resourceComponent,
            this
        );

        renderingComponent = new RenderingComponent{
           m_deviceResources.get(),
           m_hitGroupShaderTable,
           m_missShaderTable,
           m_rayGenShaderTable,
           m_hitGroupShaderTableStrideInBytes,
           m_missShaderTableStrideInBytes,
           m_width,
           m_height,
           m_gpuTimers,
           descriptors,

           m_descriptorHeap,
           m_camera,
           m_raytracingGlobalRootSignature,
           m_topLevelAS,
           m_dxrCommandList,
           m_dxrStateObject,
           m_sceneCB,
           m_trianglePrimitiveAttributeBuffer,
           m_composePSO,
           m_blurPSO,
           m_filterCB,
           buffers,
           outputComponent,
           m_blurRootSig,
           m_composeRootSig,
           rootSignatureComponent
        };

        inputComponent = new InputComponent(
            m_camera,
            m_timer,
            m_sceneCB,
            m_orbitalCamera,
            m_lastMousePosition,
            cameraComponent
        );
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