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

#include <ranges>
export module Core;

export extern "C" {
#include "Lua542/include/lua.h"
#include "Lua542/include/lauxlib.h"
#include "Lua542/include/lualib.h"
}

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

import RenderingComponent;

using namespace std;
using namespace DX;
using namespace std::views;
using namespace Microsoft::WRL;

export class Core : public DXCore {
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

    Microsoft::WRL::ComPtr<ID3D12Resource> buffers[Descriptors::COUNT];
    D3D12_GPU_DESCRIPTOR_HANDLE descriptors[Descriptors::COUNT];

    static inline const wchar_t* c_raygenShaderName = L"MyRaygenShader";
    static inline const wchar_t* c_closestHitShaderName = L"MyClosestHitShader_Triangle";
    static inline const wchar_t* c_missShaderNames[] = { L"MyMissShader", L"MyMissShader_ShadowRay" };
    static inline const wchar_t* c_hitGroupNames_TriangleGeometry[] = { L"MyHitGroup_Triangle", L"MyHitGroup_Triangle_ShadowRay" };

    Microsoft::WRL::ComPtr<ID3D12Resource> m_missShaderTable;
    UINT m_missShaderTableStrideInBytes;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_hitGroupShaderTable;
    UINT m_hitGroupShaderTableStrideInBytes;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_rayGenShaderTable;

    vector<DX::GPUTimer> m_gpuTimers(GpuTimers::Count);
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

    virtual void OnDestroy() {
        // Let GPU finish before releasing D3D resources.
        m_deviceResources->WaitForGpu();
        OnDeviceLost();
    }

    virtual void OnMouseMove(int x, int y) override {
        inputComponent.OnMouseMove(int x, int y);
    }

    virtual void OnLeftButtonDown(UINT x, UINT y) override {
        inputComponent.OnLeftButtonDown(UINT x, UINT y);
    }

    virtual IDXGISwapChain* GetSwapchain() { return m_deviceResources->GetSwapChain(); }
};