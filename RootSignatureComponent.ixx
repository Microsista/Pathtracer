module;
#include <wrl/client.h>
#include <d3dcompiler.h>
#include <d3d12.h>
#include "d3dx12.h"
#include "RayTracingHlslCompat.hlsli"
#include <vector>
#include "RaytracingSceneDefines.h"
#include <memory>
#include <string>
export module RootSignatureComponent;

import DXSampleHelper;
import DeviceResourcesInterface;

using namespace Microsoft::WRL;
using namespace std;

export class RootSignatureComponent {
    const shared_ptr<DeviceResourcesInterface>& deviceResources;
    vector<ComPtr<ID3D12RootSignature>>& raytracingLocalRootSignature;
    ComPtr<ID3D12RootSignature>& raytracingGlobalRootSignature;
    const vector<const wchar_t*>& c_hitGroupNames_TriangleGeometry;

public:
    RootSignatureComponent(
        const shared_ptr<DeviceResourcesInterface>& deviceResources,
        vector<ComPtr<ID3D12RootSignature>>& raytracingLocalRootSignature,
        ComPtr<ID3D12RootSignature>& raytracingGlobalRootSignature,
        const vector<const wchar_t*>& c_hitGroupNames_TriangleGeometry
    ) :
        deviceResources{ deviceResources },
        raytracingLocalRootSignature{ raytracingLocalRootSignature },
        raytracingGlobalRootSignature{ raytracingGlobalRootSignature },
        c_hitGroupNames_TriangleGeometry{ c_hitGroupNames_TriangleGeometry }
    {}

    void CreateRootSignatures() {
        createGlobalRootSignature();
        createLocalRootSignature();
    }

    void createGlobalRootSignature() {
        auto device = deviceResources->GetD3DDevice();

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

        CD3DX12_STATIC_SAMPLER_DESC staticSamplers[] = { CD3DX12_STATIC_SAMPLER_DESC(0, SAMPLER_FILTER) };

        CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters, ARRAYSIZE(staticSamplers), staticSamplers);
        SerializeAndCreateRaytracingRootSignature(device, globalRootSignatureDesc, raytracingGlobalRootSignature, L"Global root signature");
    }

    void createLocalRootSignature() {
        auto device = deviceResources->GetD3DDevice();

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
        SerializeAndCreateRaytracingRootSignature(device, localRootSignatureDesc, raytracingLocalRootSignature[LocalRootSignature::Type::Triangle], L"Local root signature");
    }

    void SerializeAndCreateRaytracingRootSignature(ID3D12Device5* device, D3D12_ROOT_SIGNATURE_DESC& desc, ComPtr<ID3D12RootSignature>& rootSig, LPCWSTR resourceName = nullptr) {
        ComPtr<ID3DBlob> blob;
        ComPtr<ID3DBlob> error;

        ThrowIfFailed(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error), error ? static_cast<const wchar_t*>(error->GetBufferPointer()) : nullptr);
        ThrowIfFailed(device->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rootSig)));

        if (resourceName)
        {
            rootSig->SetName(resourceName);
        }
    }

    void CreateLocalRootSignatureSubobjects(CD3DX12_STATE_OBJECT_DESC* raytracingPipeline) {
        // Hit groups
        auto localRootSignature = raytracingPipeline->CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
        localRootSignature->SetRootSignature(raytracingLocalRootSignature[LocalRootSignature::Type::Triangle].Get());
        // Shader association
        auto rootSignatureAssociation = raytracingPipeline->CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
        rootSignatureAssociation->SetSubobjectToAssociate(*localRootSignature);
        rootSignatureAssociation->AddExports(c_hitGroupNames_TriangleGeometry.data(), 2);
    }
};