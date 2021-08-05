#pragma once

#include "DXCore.h"
#include "StepTimer.h"
#include "RaytracingSceneDefines.h"
#include "DirectXRaytracingHelper.h"
#include "PerformanceTimers.h"
#include "Mesh.h"
#include "Texture.h"

// assimp includes
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace App {
    static const UINT FrameCount = 3;
}

class Core : public DXCore {
public:
    Core(UINT width, UINT height, std::wstring name);

    virtual void OnDeviceLost() override;
    virtual void OnDeviceRestored() override;

    virtual void OnInit();
    virtual void OnKeyDown(UINT8 key);
    virtual void OnMouseMove(int x, int y) override;
    virtual void OnLeftButtonDown(UINT x, UINT y) override;
    virtual void OnUpdate();
    virtual void OnRender();
    virtual void Compose();
    virtual void OnSizeChanged(UINT width, UINT height, bool minimized);
    virtual void OnDestroy();
    virtual IDXGISwapChain* GetSwapchain() { return m_deviceResources->GetSwapChain(); }

private:
    void UpdateCameraMatrices();
    void InitializeScene();
    void RecreateD3D();
    void DoRaytracing();
    void CreateConstantBuffers();
    void CreateTrianglePrimitiveAttributesBuffers();
    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();
    void ReleaseDeviceDependentResources();
    void ReleaseWindowSizeDependentResources();
    void CreateRaytracingInterfaces();
    void SerializeAndCreateRaytracingRootSignature(ID3D12Device5* device, D3D12_ROOT_SIGNATURE_DESC& desc, ComPtr<ID3D12RootSignature>* rootSig, LPCWSTR resourceName);
    void CreateRootSignatures();
    void CreateDxilLibrarySubobjects(CD3DX12_STATE_OBJECT_DESC* raytracingPipeline);
    void CreateHitGroupSubobjects(CD3DX12_STATE_OBJECT_DESC* raytracingPipeline);
    void CreateLocalRootSignatureSubobjects(CD3DX12_STATE_OBJECT_DESC* raytracingPipeline);
    void CreateRaytracingPipelineStateObject();
    void CreateAuxilaryDeviceResources();
    void CreateDescriptorHeap();
    void BuildModel(std::string path, UINT flags, bool usesTextures);
    void CreateRaytracingOutputResource();
    void BuildGeometry();
    void BuildGeometryDescsForBottomLevelAS(std::array<std::vector<D3D12_RAYTRACING_GEOMETRY_DESC>, BottomLevelASType::Count>& geometryDescs);
    template <class InstanceDescType, class BLASPtrType>
    void BuildBottomLevelASInstanceDescs(BLASPtrType* bottomLevelASaddresses, ComPtr<ID3D12Resource>* instanceDescsResource);
    AccelerationStructureBuffers BuildBottomLevelAS(const std::vector<D3D12_RAYTRACING_GEOMETRY_DESC>& geometryDesc, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE);
    AccelerationStructureBuffers BuildTopLevelAS(AccelerationStructureBuffers bottomLevelAS[BottomLevelASType::Count], D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE);
    void BuildAccelerationStructures();
    void BuildShaderTables();
    void UpdateForSizeChange(UINT clientWidth, UINT clientHeight);
    void CopyRaytracingOutputToBackbuffer();
    void CalculateFrameStats();
    UINT AllocateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* cpuDescriptor, UINT descriptorIndexToUse = UINT_MAX);
    UINT CreateBufferSRV(D3DBuffer* buffer, UINT numElements, UINT elementSize);
    UINT CreateTextureSRV(UINT numElements, UINT elementSize);

    static const UINT FrameCount = 3;

    // Constants.
    static const UINT NUM_BLAS = 1090;          // Triangle + AABB bottom-level AS.
    const float c_aabbWidth = 2;      // AABB width.
    const float c_aabbDistance = 2;   // Distance between AABBs.
    
    UINT m_geoOffset = 0;
    // DirectX Raytracing (DXR) attributes
    ComPtr<ID3D12Device5> m_dxrDevice;
    ComPtr<ID3D12GraphicsCommandList5> m_dxrCommandList;
    ComPtr<ID3D12StateObject> m_dxrStateObject;
    ComPtr<ID3D12PipelineState> m_composePSO[1];

    // Root signatures
    ComPtr<ID3D12RootSignature> m_raytracingGlobalRootSignature;
    ComPtr<ID3D12RootSignature> m_raytracingLocalRootSignature[LocalRootSignature::Type::Count];
    ComPtr<ID3D12RootSignature> m_composeRootSig;


    // Descriptors
    std::shared_ptr<DX::DescriptorHeap> m_descriptorHeap;
    UINT m_descriptorsAllocated;
    UINT m_descriptorSize;

    // Raytracing scene
    ConstantBuffer<SceneConstantBuffer> m_sceneCB;
    ConstantBuffer<AtrousWaveletTransformFilterConstantBuffer> m_filterCB;
    StructuredBuffer<PrimitiveInstancePerFrameBuffer> m_aabbPrimitiveAttributeBuffer;
    StructuredBuffer<PrimitiveInstancePerFrameBuffer> m_trianglePrimitiveAttributeBuffer;
    std::vector<D3D12_RAYTRACING_AABB> m_aabbs;

    // Root constants
    PrimitiveConstantBuffer m_triangleMaterialCB[NUM_BLAS];

    // Geometry
    std::vector<Vertex> m_vertices;
    std::vector<Index> m_indices;
    D3DBuffer m_indexBuffer[NUM_BLAS];
    D3DBuffer m_vertexBuffer[NUM_BLAS];
    D3DBuffer m_aabbBuffer;
    std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> m_geometries;

    // Acceleration structure
    ComPtr<ID3D12Resource> m_bottomLevelAS[BottomLevelASType::Count];
    ComPtr<ID3D12Resource> m_topLevelAS;

    // Raytracing output
    ComPtr<ID3D12Resource> m_raytracingOutput;
    ComPtr<ID3D12Resource> m_reflectionBuffer;
    ComPtr<ID3D12Resource> m_shadowBuffer;
    ComPtr<ID3D12Resource> m_normalDepth;
    ComPtr<ID3D12Resource> m_prevFrame;
    ComPtr<ID3D12Resource> m_prevReflection;
    ComPtr<ID3D12Resource> m_prevShadow;
    ComPtr<ID3D12Resource> m_normalMap;
    ComPtr<ID3D12Resource> m_specularMap;
    ComPtr<ID3D12Resource> m_emissiveMap;
    D3D12_GPU_DESCRIPTOR_HANDLE m_raytracingOutputResourceUAVGpuDescriptor;
    D3D12_GPU_DESCRIPTOR_HANDLE m_reflectionBufferResourceUAVGpuDescriptor;
    D3D12_GPU_DESCRIPTOR_HANDLE m_shadowBufferResourceUAVGpuDescriptor;
    D3D12_GPU_DESCRIPTOR_HANDLE m_normalDepthResourceUAVGpuDescriptor;
    D3D12_GPU_DESCRIPTOR_HANDLE m_prevFrameResourceUAVGpuDescriptor;
    D3D12_GPU_DESCRIPTOR_HANDLE m_prevReflectionResourceUAVGpuDescriptor;
    D3D12_GPU_DESCRIPTOR_HANDLE m_prevShadowResourceUAVGpuDescriptor;
    D3D12_GPU_DESCRIPTOR_HANDLE m_normalMapResourceUAVGpuDescriptor;
    D3D12_GPU_DESCRIPTOR_HANDLE m_specularMapResourceUAVGpuDescriptor;
    D3D12_GPU_DESCRIPTOR_HANDLE m_emissiveMapResourceUAVGpuDescriptor;
    UINT m_raytracingOutputResourceUAVDescriptorHeapIndex;
    UINT m_reflectionBufferResourceUAVDescriptorHeapIndex;
    UINT m_shadowBufferResourceUAVDescriptorHeapIndex;
    UINT m_normalDepthResourceUAVDescriptorHeapIndex;
    UINT m_prevFrameResourceUAVDescriptorHeapIndex;
    UINT m_prevReflectionResourceUAVDescriptorHeapIndex;
    UINT m_prevShadowResourceUAVDescriptorHeapIndex;
    UINT m_normalMapResourceUAVDescriptorHeapIndex;
    UINT m_specularMapResourceUAVDescriptorHeapIndex;
    UINT m_emissiveMapResourceUAVDescriptorHeapIndex;

    // Shader tables
    static const wchar_t* c_hitGroupNames_TriangleGeometry[RayType::Count];
    static const wchar_t* c_raygenShaderName;
    static const wchar_t* c_closestHitShaderName;
    static const wchar_t* c_missShaderNames[RayType::Count];

    ComPtr<ID3D12Resource> m_missShaderTable;
    UINT m_missShaderTableStrideInBytes;
    ComPtr<ID3D12Resource> m_hitGroupShaderTable;
    UINT m_hitGroupShaderTableStrideInBytes;
    ComPtr<ID3D12Resource> m_rayGenShaderTable;

    // Application state
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

    std::shared_ptr<DX::DescriptorHeap> m_cbvSrvUavHeap;
    std::shared_ptr<DX::DescriptorHeap> m_composeHeap;

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
