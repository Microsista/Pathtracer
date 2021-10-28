module;
#include "d3dx12.h"
#include <string>
#include <unordered_map>
#include "RayTracingHlslCompat.hlsli"
#include <d3d12.h>
#include <memory>
#include <vector>
#include <wrl/client.h>

#include "RaytracingSceneDefines.h"
export module ShaderTableComponent;

import DeviceResourcesInterface;
import ShaderTable;
import ShaderRecord;
import DXSampleHelper;
import Material;
import Mesh;
import D3DBuffer;
import D3DTexture;

using namespace std;
using namespace Microsoft::WRL;

export class ShaderTableComponent {
    shared_ptr<DeviceResourcesInterface>& deviceResources;
    vector<const wchar_t*>& c_hitGroupNames_TriangleGeometry;
    vector<const wchar_t*>& c_missShaderNames;
    const wchar_t*& c_closestHitShaderName;
    const wchar_t*& c_raygenShaderName;
    ComPtr<ID3D12Resource>& hitGroupShaderTable;
    ComPtr<ID3D12StateObject>& dxrStateObject;
    UINT& missShaderTableStrideInBytes;
    vector<int>& meshOffsets;
    vector<D3DBuffer>& indexBuffer;
    vector<D3DBuffer>& vertexBuffer;
    unordered_map<string, unique_ptr<MeshGeometry>>& geometries;
    vector<int>& meshSizes;
    vector<D3DTexture>& templeTextures;
    vector<D3DTexture>& templeNormalTextures;
    vector<D3DTexture>& templeSpecularTextures;
    vector<D3DTexture>& templeEmittanceTextures;
    const UINT& NUM_BLAS;
    UINT& hitGroupShaderTableStrideInBytes;
    vector<PrimitiveConstantBuffer>& triangleMaterialCB;
    ComPtr<ID3D12Resource>& rayGenShaderTable;
    ComPtr<ID3D12Resource>& missShaderTable;

public:
    ShaderTableComponent(
        shared_ptr<DeviceResourcesInterface>& deviceResources,
        vector<const wchar_t*>& c_hitGroupNames_TriangleGeometry,
        vector<const wchar_t*>& c_missShaderNames,
        const wchar_t*& c_closestHitShaderName,
        const wchar_t*& c_raygenShaderName,
        ComPtr<ID3D12Resource>& hitGroupShaderTable,
        ComPtr<ID3D12StateObject>& dxrStateObject,
        UINT& missShaderTableStrideInBytes,
        vector<int>& meshOffsets,
        vector<D3DBuffer>& indexBuffer,
        vector<D3DBuffer>& vertexBuffer,
        unordered_map<string, unique_ptr<MeshGeometry>>& geometries,
        vector<int>& meshSizes,
        vector<D3DTexture>& templeTextures,
        vector<D3DTexture>& templeNormalTextures,
        vector<D3DTexture>& templeSpecularTextures,
        vector<D3DTexture>& templeEmittanceTextures,
        const UINT& NUM_BLAS,
        UINT& hitGroupShaderTableStrideInBytes,
        vector<PrimitiveConstantBuffer>& triangleMaterialCB,
        ComPtr<ID3D12Resource>& rayGenShaderTable,
        ComPtr<ID3D12Resource>& missShaderTable
    ) :
        deviceResources{ deviceResources },
        c_hitGroupNames_TriangleGeometry{ c_hitGroupNames_TriangleGeometry },
        c_missShaderNames{ c_missShaderNames },
        c_closestHitShaderName{ c_closestHitShaderName },
        c_raygenShaderName{ c_raygenShaderName },
        hitGroupShaderTable{ hitGroupShaderTable },
        dxrStateObject{ dxrStateObject },
        missShaderTableStrideInBytes{ missShaderTableStrideInBytes },
        meshOffsets{ meshOffsets },
        indexBuffer{ indexBuffer },
        vertexBuffer{ vertexBuffer },
        geometries{ geometries },
        meshSizes{ meshSizes },
        templeTextures{ templeTextures },
        templeNormalTextures{ templeNormalTextures },
        templeSpecularTextures{ templeSpecularTextures },
        templeEmittanceTextures{ templeEmittanceTextures },
        NUM_BLAS{ NUM_BLAS },
        hitGroupShaderTableStrideInBytes{ hitGroupShaderTableStrideInBytes },
        triangleMaterialCB{ triangleMaterialCB },
        rayGenShaderTable{ rayGenShaderTable },
        missShaderTable{ missShaderTable }
    {}

    void BuildShaderTables()
    {
        auto device = deviceResources->GetD3DDevice();

        void* rayGenShaderID;
        void* missShaderIDs[RayType::Count];
        void* hitGroupShaderIDs_TriangleGeometry[RayType::Count];

        // A shader name look-up table for shader table debug print out.
        unordered_map<void*, wstring> shaderIdToStringMap;

        auto GetShaderIDs = [&](auto* stateObjectProperties)
        {
            rayGenShaderID = stateObjectProperties->GetShaderIdentifier((LPCWSTR)c_raygenShaderName);
            shaderIdToStringMap[rayGenShaderID] = wstring(c_raygenShaderName);

            for (UINT i = 0; i < RayType::Count; i++)
            {
                missShaderIDs[i] = stateObjectProperties->GetShaderIdentifier(c_missShaderNames[i]);
                shaderIdToStringMap[missShaderIDs[i]] = wstring(c_missShaderNames[i]);
            }
            for (UINT i = 0; i < RayType::Count; i++)
            {
                hitGroupShaderIDs_TriangleGeometry[i] = stateObjectProperties->GetShaderIdentifier(c_hitGroupNames_TriangleGeometry[i]);
                shaderIdToStringMap[hitGroupShaderIDs_TriangleGeometry[i]] = wstring(c_hitGroupNames_TriangleGeometry[i]);
            }
        };

        // Get shader identifiers.
        UINT shaderIDSize;
        {
            ComPtr<ID3D12StateObjectProperties> stateObjectProperties;
            ThrowIfFailed(dxrStateObject.As(&stateObjectProperties));
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
            this->rayGenShaderTable = rayGenShaderTable.GetResource();
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
            missShaderTableStrideInBytes = missShaderTable.GetShaderRecordSize();
            this->missShaderTable = missShaderTable.GetResource();
        }

        // Hit group shader table.
        {
            UINT numShaderRecords = NUM_BLAS * RayType::Count;
            UINT shaderRecordSize = shaderIDSize + LocalRootSignature::MaxRootArgumentsSize();
            ShaderTable hitGroupShaderTable(device, numShaderRecords, shaderRecordSize, L"HitGroupShaderTable");

            for (auto i = 0; i < 6; ++i)
            {
                LocalRootSignature::Triangle::RootArguments rootArgs;

                // Create a shader record for each primitive.
                for (UINT instanceIndex = 0; instanceIndex < (UINT)meshSizes[i]; instanceIndex++)
                {
                    rootArgs.materialCb = triangleMaterialCB[instanceIndex + meshOffsets[i]];
                    rootArgs.triangleCB.instanceIndex = instanceIndex + meshOffsets[i];
                    auto ib = indexBuffer[instanceIndex + meshOffsets[i]].gpuDescriptorHandle;
                    auto vb = vertexBuffer[instanceIndex + meshOffsets[i]].gpuDescriptorHandle;
                    string geoName = "geo" + to_string(instanceIndex + meshOffsets[i]);
                    string meshName = "mesh" + to_string(instanceIndex + meshOffsets[i]);
                    Material m = geometries[geoName]->DrawArgs[meshName].Material;
                    auto texture = templeTextures[m.id].gpuDescriptorHandle;
                    auto normalTexture = templeNormalTextures[m.id].gpuDescriptorHandle;
                    auto specularTexture = templeSpecularTextures[m.id].gpuDescriptorHandle;
                    auto emittanceTexture = templeEmittanceTextures[m.id].gpuDescriptorHandle;
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
            hitGroupShaderTableStrideInBytes = hitGroupShaderTable.GetShaderRecordSize();
            this->hitGroupShaderTable = hitGroupShaderTable.GetResource();
        }
    }

   
};