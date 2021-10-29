module;
#include <vector>
#include <d3d12.h>
export module CoreInterface;

import AccelerationStructureBuffers;
import RaytracingSceneDefines;

using namespace std;

export struct CoreInterface {
    //virtual void OnDeviceLost() 
    //virtual void OnDeviceRestored()
    //virtual void OnInit()
    //virtual void OnKeyDown(UINT8 key)

    //virtual void OnMouseMove(int x, int y)

    //virtual void OnLeftButtonDown(UINT x, UINT y) 

    //virtual void OnUpdate()
    //virtual void OnRender()

    //virtual void Blur()

    //virtual void Compose()
    //virtual void OnSizeChanged(UINT width, UINT height, bool minimized)
    //virtual void OnDestroy()
    //virtual IDXGISwapChain* GetSwapchain()
    //void UpdateCameraMatrices()
    //void InitializeScene()
    //void RecreateD3D()

    //void CreateConstantBuffers()
    //void CreateTrianglePrimitiveAttributesBuffers()
    //void CreateDeviceDependentResources()
    //void CreateWindowSizeDependentResources()
    //void ReleaseDeviceDependentResources()

    //void ReleaseWindowSizeDependentResources()
    //void CreateRaytracingInterfaces()

    //void SerializeAndCreateRaytracingRootSignature(ID3D12Device5* device, D3D12_ROOT_SIGNATURE_DESC& desc, Microsoft::WRL::ComPtr<ID3D12RootSignature>* rootSig, LPCWSTR resourceName = nullptr)

    //void CreateRootSignatures()
    //void CreateDxilLibrarySubobjects(CD3DX12_STATE_OBJECT_DESC* raytracingPipeline)
    //void CreateHitGroupSubobjects(CD3DX12_STATE_OBJECT_DESC* raytracingPipeline)

    //void CreateLocalRootSignatureSubobjects(CD3DX12_STATE_OBJECT_DESC* raytracingPipeline)

    //void CreateRaytracingPipelineStateObject()

    //void CreateAuxilaryDeviceResources()

    //void CreateDescriptorHeap()
    //void BuildModel(std::string path, UINT flags, bool usesTextures = false)

    //void CreateRaytracingOutputResource()

    //void BuildGeometry()

    virtual void BuildGeometryDescsForBottomLevelAS(std::array<std::vector<D3D12_RAYTRACING_GEOMETRY_DESC>, BottomLevelASType::Count>& geometryDescs) = 0;

    //template <class InstanceDescType, class BLASPtrType> void BuildBottomLevelASInstanceDescs(BLASPtrType* bottomLevelASaddresses, Microsoft::WRL::ComPtr<ID3D12Resource>* instanceDescsResource)

    virtual AccelerationStructureBuffers BuildBottomLevelAS(const std::vector<D3D12_RAYTRACING_GEOMETRY_DESC>& geometryDescs, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE) = 0;

    virtual AccelerationStructureBuffers BuildTopLevelAS(AccelerationStructureBuffers* bottomLevelAS, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE) = 0;

    //void BuildShaderTables()

    //void UpdateForSizeChange(UINT width, UINT height)

    //void CopyRaytracingOutputToBackbuffer()

    //void CalculateFrameStats()

    //UINT AllocateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* cpuDescriptor, UINT descriptorIndexToUse = UINT_MAX)

    //UINT CreateBufferSRV(D3DBuffer* buffer, UINT numElements, UINT elementSize)

    //UINT CreateTextureSRV(UINT numElements, UINT elementSize)
};