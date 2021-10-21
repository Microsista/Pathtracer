module;
#include "d3dx12.h"
#include <string>
#include <unordered_map>
#include "RayTracingHlslCompat.hlsli"
#include <d3d12.h>
#include <memory>
#include <vector>
#include <ranges>
#include <wrl/client.h>
export module ShaderTableComponent;

import DeviceResources;
import ShaderTable;
import ShaderRecord;
import DXSampleHelper;
import Material;
import RaytracingSceneDefines;
import Mesh;
import D3DBuffer;
import D3DTexture;
import ConstantBuffer;

using namespace std;
using namespace std::views;
using namespace Microsoft::WRL;

export class ShaderTableComponent {
    DeviceResources* deviceResources;
    const wchar_t** c_hitGroupNames_TriangleGeometry;
    const wchar_t** c_missShaderNames;
    const wchar_t* c_closestHitShaderName;
    const wchar_t* c_raygenShaderName;
    ShaderTable* hitGroupShaderTable;
    ComPtr<ID3D12StateObject> dxrStateObject;
    UINT missShaderTableStrideInBytes;
    UINT* meshOffsets;
    D3DBuffer* indexBuffer;
    D3DBuffer* vertexBuffer;
    unordered_map<string, unique_ptr<MeshGeometry>>* geometries;
    vector<int>* meshSizes;
    D3DTexture** templeTextures;
    D3DTexture** templeNormalTextures;
    D3DTexture** templeSpecularTextures;
    D3DTexture** templeEmittanceTextures;
    UINT NUM_BLAS;
    UINT hitGroupShaderTableStrideInBytes;
    ConstantBuffer<PrimitiveConstantBuffer>* triangleMaterialCB;


public:
    ShaderTableComponent() {}

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
            Microsoft::WRL::ComPtr<ID3D12StateObjectProperties> stateObjectProperties;
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
            rayGenShaderTable = rayGenShaderTable.GetResource();
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
            missShaderTable = missShaderTable.GetResource();
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
                for (UINT instanceIndex = 0; instanceIndex < (UINT)(*meshSizes)[i]; instanceIndex++)
                {
                    rootArgs.materialCb = triangleMaterialCB[instanceIndex + meshOffsets[i]];
                    rootArgs.triangleCB.instanceIndex = instanceIndex + meshOffsets[i];
                    auto ib = indexBuffer[instanceIndex + meshOffsets[i]].gpuDescriptorHandle;
                    auto vb = vertexBuffer[instanceIndex + meshOffsets[i]].gpuDescriptorHandle;
                    string geoName = "geo" + to_string(instanceIndex + meshOffsets[i]);
                    string meshName = "mesh" + to_string(instanceIndex + meshOffsets[i]);
                    Material m = (*geometries)[geoName]->DrawArgs[meshName].Material;
                    auto texture = (*templeTextures)[m.id].gpuDescriptorHandle;
                    auto normalTexture = (*templeNormalTextures)[m.id].gpuDescriptorHandle;
                    auto specularTexture = (*templeSpecularTextures)[m.id].gpuDescriptorHandle;
                    auto emittanceTexture = (*templeEmittanceTextures)[m.id].gpuDescriptorHandle;
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
            hitGroupShaderTable = hitGroupShaderTable.GetResource();
        }
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
};