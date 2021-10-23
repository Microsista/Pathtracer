module;
#include <wrl/client.h>
#include <vector>
#include <array>
#include <d3d12.h>

#include "RaytracingSceneDefines.h"
#include <string>
#include <unordered_map>
#include <memory>
export module BottomLevelASComponent;

import AccelerationStructureBuffers;
import DeviceResources;
import DXSampleHelper;
import Helper;
import D3DBuffer;
import Mesh;
import Transform;

using namespace Microsoft::WRL;
using namespace std;

export class BottomLevelASComponent {
    DeviceResources* deviceResources;
    ID3D12Device5* dxrDevice;
    ID3D12GraphicsCommandList5* dxrCommandList;
    vector<int>* meshSizes;
    D3DBuffer* indexBuffer;
    D3DBuffer* vertexBuffer;
    unordered_map<string, unique_ptr<MeshGeometry>>* geometries;
    UINT* meshOffsets;
    UINT NUM_BLAS;

public:
    BottomLevelASComponent() {}

    AccelerationStructureBuffers BuildBottomLevelAS(const vector<D3D12_RAYTRACING_GEOMETRY_DESC>& geometryDescs,
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags =
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE)
    {
        auto device = deviceResources->GetD3DDevice();
        auto commandList = deviceResources->GetCommandList();
        ComPtr<ID3D12Resource> scratch;
        ComPtr<ID3D12Resource> bottomLevelAS;

        // Get the size requirements for the scratch and AS buffers.
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& bottomLevelInputs = bottomLevelBuildDesc.Inputs;
        bottomLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
        bottomLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        bottomLevelInputs.Flags = buildFlags;
        bottomLevelInputs.NumDescs = static_cast<UINT>(geometryDescs.size());
        bottomLevelInputs.pGeometryDescs = geometryDescs.data();

        // Get maximum required space for data, scratch, and update buffers for these geometries with these parameters.
        // You have to know this before creating the buffers, so that the are big enough.
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottomLevelPrebuildInfo = {};
        dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&bottomLevelInputs, &bottomLevelPrebuildInfo);
        ThrowIfFalse(bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

        // Create a scratch buffer.
        AllocateUAVBuffer(device, bottomLevelPrebuildInfo.ScratchDataSizeInBytes, &scratch, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            L"ScratchResource");

        D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
        AllocateUAVBuffer(device, bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes, &bottomLevelAS, initialResourceState,
            L"BottomLevelAccelerationStructure");

        // bottom-level AS desc.
        bottomLevelBuildDesc.ScratchAccelerationStructureData = scratch->GetGPUVirtualAddress();
        bottomLevelBuildDesc.DestAccelerationStructureData = bottomLevelAS->GetGPUVirtualAddress();

        // Build the acceleration structure.
        dxrCommandList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);

        AccelerationStructureBuffers bottomLevelASBuffers;
        bottomLevelASBuffers.accelerationStructure = bottomLevelAS;
        bottomLevelASBuffers.scratch = scratch;
        bottomLevelASBuffers.ResultDataMaxSizeInBytes = bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes;
        return bottomLevelASBuffers;
    }

    void BuildGeometryDescsForBottomLevelAS(array<vector<D3D12_RAYTRACING_GEOMETRY_DESC>, BottomLevelASType::Count>& geometryDescs) {
        D3D12_RAYTRACING_GEOMETRY_DESC triangleDescTemplate{};
        triangleDescTemplate.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        triangleDescTemplate.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
        triangleDescTemplate.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
        triangleDescTemplate.Triangles.VertexBuffer.StrideInBytes = sizeof(VertexPositionNormalTextureTangent);
        triangleDescTemplate.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

        for (auto i = 2; i < 6; ++i) {
            geometryDescs[i].resize((*meshSizes)[i], triangleDescTemplate);
            for (auto j = 0; j < (*meshSizes)[i]; ++j) {
                string geoName = "geo" + to_string(meshOffsets[i] + j);
                string meshName = "mesh" + to_string(meshOffsets[i] + j);
                geometryDescs[i][j].Triangles.IndexBuffer = indexBuffer[meshOffsets[i] + j].resource->GetGPUVirtualAddress();
                geometryDescs[i][j].Triangles.VertexBuffer.StartAddress =
                    vertexBuffer[meshOffsets[i] + j].resource->GetGPUVirtualAddress();
                geometryDescs[i][j].Triangles.IndexCount = (*geometries)[geoName]->DrawArgs[meshName].IndexCount;
                geometryDescs[i][j].Triangles.VertexCount = (*geometries)[geoName]->DrawArgs[meshName].VertexCount;
            }
        }
    }

    template <class InstanceDescType, class BLASPtrType> void BuildBottomLevelASInstanceDescs(BLASPtrType* bottomLevelASaddresses,
        ComPtr<ID3D12Resource>* instanceDescsResource) {
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
            instanceDescs[i].InstanceContributionToHitGroupIndex = meshOffsets[i] * RayType::Count;
            instanceDescs[i].AccelerationStructure = bottomLevelASaddresses[i];

            XMMATRIX scale = XMMatrixScaling(transforms[i].scale.x, transforms[i].scale.y, transforms[i].scale.z);
            XMMATRIX rotation = XMMatrixRotationY(XMConvertToRadians(transforms[i].rotation));
            XMMATRIX translation = XMMatrixTranslationFromVector(XMLoadFloat3(&transforms[i].translation));
            XMStoreFloat3x4(reinterpret_cast<XMFLOAT3X4*>(instanceDescs[i].Transform), scale * rotation * translation);
        }

        UINT64 bufferSize = static_cast<UINT64>(instanceDescs.size() * sizeof(instanceDescs[0]));
        AllocateUploadBuffer(deviceResources->GetD3DDevice(), instanceDescs.data(), bufferSize, &(*instanceDescsResource),
            L"InstanceDescs");
    };
};