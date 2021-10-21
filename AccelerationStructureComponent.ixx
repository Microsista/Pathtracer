module;
#include <vector>
#include <d3d12.h>
#include <string>
#include <array>
#include <DirectXMath.h>
#include "d3dx12.h"
#include <unordered_map>
#include <memory>
#include "RayTracingHlslCompat.hlsli"
#include <ranges>
#include <wrl/client.h>
export module AccelerationStructureComponent;

import AccelerationStructureBuffers;
import DeviceResources;
import DXSampleHelper;
import Helper;
import RaytracingSceneDefines;
import Transform;
import D3DTexture;
import Material;
import Mesh;
import D3DBuffer;
import DescriptorHeap;

using namespace std;
using namespace DirectX;
using namespace std::views;
using namespace Microsoft::WRL;

export class AccelerationStructureComponent {
    DeviceResources* deviceResources;
    ID3D12Device5* dxrDevice;
    UINT NUM_BLAS;
    ID3D12GraphicsCommandList5* dxrCommandList;
    ComPtr<ID3D12Resource>* bottomLevelAS;
    ComPtr<ID3D12Resource> topLevelAS;
    ComPtr<ID3D12Resource> m_bottomLevelAS[BottomLevelASType::Count];
    D3DTexture** stoneTexture;
    unordered_map<int, Material>* materials;
    UINT* meshOffsets;
    D3DBuffer* indexBuffer;
    D3DBuffer* vertexBuffer;
    unordered_map<string, unique_ptr<MeshGeometry>>* geometries;
    DescriptorHeap* descriptorHeap;
    vector<int>* meshSizes;
    D3DTexture** templeTextures;
    D3DTexture** templeNormalTextures;
    D3DTexture** templeSpecularTextures;
    D3DTexture** templeEmittanceTextures;
    
public:
    AccelerationStructureComponent() {}

    AccelerationStructureBuffers BuildBottomLevelAS(const std::vector<D3D12_RAYTRACING_GEOMETRY_DESC>& geometryDescs, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE) {
        auto device = deviceResources->GetD3DDevice();
        auto commandList = deviceResources->GetCommandList();
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
        dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&bottomLevelInputs, &bottomLevelPrebuildInfo);
        ThrowIfFalse(bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

        // Create a scratch buffer.
        AllocateUAVBuffer(device, bottomLevelPrebuildInfo.ScratchDataSizeInBytes, &scratch, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");

        D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
        AllocateUAVBuffer(device, bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes, &bottomLevelAS, initialResourceState, L"BottomLevelAccelerationStructure");

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

    AccelerationStructureBuffers BuildTopLevelAS(AccelerationStructureBuffers bottomLevelAS[BottomLevelASType::Count], D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE)
    {
        auto device = deviceResources->GetD3DDevice();
        auto commandList = deviceResources->GetCommandList();
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
        dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&topLevelInputs, &topLevelPrebuildInfo);
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
        dxrCommandList->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);

        AccelerationStructureBuffers topLevelASBuffers;
        topLevelASBuffers.accelerationStructure = topLevelAS;
        topLevelASBuffers.instanceDesc = instanceDescsResource;
        topLevelASBuffers.scratch = scratch;
        topLevelASBuffers.ResultDataMaxSizeInBytes = topLevelPrebuildInfo.ResultDataMaxSizeInBytes;
        return topLevelASBuffers;
    }

    void BuildAccelerationStructures() {
        auto device = deviceResources->GetD3DDevice();
        auto commandList = deviceResources->GetCommandList();
        auto commandQueue = deviceResources->GetCommandQueue();
        auto commandAllocator = deviceResources->GetCommandAllocator();

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
        s.append(L"\\..\\..\\Models\\SunTemple\\Textures\\Arch_Inst_0_BaseColor.dds");

        string s4(path.begin(), path.end());
        s4.append("\\..\\..\\Models\\SunTemple\\Textures\\");

        stoneTexture[0]->heapIndex = 3000 + 1;
        LoadDDSTexture(device, commandList, s.c_str(), descriptorHeap, &(*stoneTexture)[0].resource, &(*stoneTexture)[0].upload, &(*stoneTexture)[0].heapIndex, &(*stoneTexture)[0].cpuDescriptorHandle, &(*stoneTexture)[0].gpuDescriptorHandle, D3D12_SRV_DIMENSION_TEXTURE2D);


        std::vector<int> included;
        for (UINT i = 0; i < 1056; i++)
        {
            string geoName = "geo" + to_string(i + 11);
            string meshName = "mesh" + to_string(i + 11);


            int key = (*geometries)[geoName]->DrawArgs[meshName].Material.id;


            if (std::find(included.begin(), included.end(), key) != included.end()) {
            }
            else {
                (*materials)[key] = (*geometries)[geoName]->DrawArgs[meshName].Material;
                included.push_back(key);
            }
        }

        for (int i = 0; i < (*materials).size(); i++) {
            string base = "..\\..\\Models\\SunTemple\\Textures\\";
            base = s4;
            string add = (*materials)[i].map_Kd;
            string normalMapPath = (*materials)[i].map_Bump;
            string specularMapPath = (*materials)[i].map_Ks;
            string emittanceMapPath = (*materials)[i].map_Ke;

            std::size_t pos = add.find("\\");
            std::string str3 = add.substr(pos + 1);
            string path = base + str3;
            print(path);
            if (add != "")
                LoadDDSTexture(device, commandList, wstring(path.begin(), path.end()).c_str(), descriptorHeap, &(*templeTextures)[i]);

            std::size_t pos2 = normalMapPath.find("\\");
            std::string str4 = normalMapPath.substr(pos2 + 1);
            string path2 = base + str4;
            print(path2);
            if (normalMapPath != "")
                LoadDDSTexture(device, commandList, wstring(path2.begin(), path2.end()).c_str(), descriptorHeap, &(*templeNormalTextures)[i]);

            std::size_t pos3 = specularMapPath.find("\\");
            std::string str5 = specularMapPath.substr(pos3 + 1);
            string path3 = base + str5;
            print(path3);
            if (specularMapPath != "")
                LoadDDSTexture(device, commandList, wstring(path3.begin(), path3.end()).c_str(), descriptorHeap, &(*templeSpecularTextures)[i]);

            std::size_t pos4 = emittanceMapPath.find("\\");
            std::string str6 = emittanceMapPath.substr(pos4 + 1);
            string path4 = base + str6;
            print(path4);
            if (emittanceMapPath != "")
                LoadDDSTexture(device, commandList, wstring(path4.begin(), path4.end()).c_str(), descriptorHeap, &(*templeEmittanceTextures)[i]);
        }

        // Kick off acceleration structure construction.
        deviceResources->ExecuteCommandList();

        // Wait for GPU to finish as the locally created temporary GPU resources will get released once we go out of scope.
        deviceResources->WaitForGpu();

        // Store the AS buffers. The rest of the buffers will be released once we exit the function.
        for (UINT i = 0; i < BottomLevelASType::Count; i++)
        {
            this->bottomLevelAS[i] = bottomLevelAS[i].accelerationStructure;
        }
        this->topLevelAS = topLevelAS.accelerationStructure;
    }

    void BuildGeometryDescsForBottomLevelAS(std::array<std::vector<D3D12_RAYTRACING_GEOMETRY_DESC>, BottomLevelASType::Count>& geometryDescs) {
        D3D12_RAYTRACING_GEOMETRY_DESC triangleDescTemplate{};
        triangleDescTemplate.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        triangleDescTemplate.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
        triangleDescTemplate.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
        triangleDescTemplate.Triangles.VertexBuffer.StrideInBytes = sizeof(VertexPositionNormalTextureTangent);
        triangleDescTemplate.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

        for (auto i : iota(2, 6)) {
            geometryDescs[i].resize((*meshSizes)[i], triangleDescTemplate);
            for (auto j : iota(0, (*meshSizes)[i])) {
                string geoName = "geo" + to_string(meshOffsets[i] + j);
                string meshName = "mesh" + to_string(meshOffsets[i] + j);
                geometryDescs[i][j].Triangles.IndexBuffer = indexBuffer[meshOffsets[i] + j].resource->GetGPUVirtualAddress();
                geometryDescs[i][j].Triangles.VertexBuffer.StartAddress = vertexBuffer[meshOffsets[i] + j].resource->GetGPUVirtualAddress();
                geometryDescs[i][j].Triangles.IndexCount = (*geometries)[geoName]->DrawArgs[meshName].IndexCount;
                geometryDescs[i][j].Triangles.VertexCount = (*geometries)[geoName]->DrawArgs[meshName].VertexCount;
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
            instanceDescs[i].InstanceContributionToHitGroupIndex = meshOffsets[i] * RayType::Count;
            instanceDescs[i].AccelerationStructure = bottomLevelASaddresses[i];

            XMMATRIX scale = XMMatrixScaling(transforms[i].scale.x, transforms[i].scale.y, transforms[i].scale.z);
            XMMATRIX rotation = XMMatrixRotationY(XMConvertToRadians(transforms[i].rotation));
            XMMATRIX translation = XMMatrixTranslationFromVector(XMLoadFloat3(&transforms[i].translation));
            XMStoreFloat3x4(reinterpret_cast<XMFLOAT3X4*>(instanceDescs[i].Transform), scale * rotation * translation);
        }

        UINT64 bufferSize = static_cast<UINT64>(instanceDescs.size() * sizeof(instanceDescs[0]));
        AllocateUploadBuffer(deviceResources->GetD3DDevice(), instanceDescs.data(), bufferSize, &(*instanceDescsResource), L"InstanceDescs");
    };
};