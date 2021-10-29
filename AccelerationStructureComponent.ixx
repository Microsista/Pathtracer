module;
#include <memory>
#include <d3d12.h>
#include <array>
#include <vector>
#include "d3dx12.h"
#include <unordered_map>
#include <string>
#include <wrl/client.h>
export module AccelerationStructureComponent;

import DeviceResourcesInterface;
import AccelerationStructureBuffers;
import RaytracingSceneDefines;
import CoreInterface;
import D3DTexture;
import DescriptorHeap;
import DXSampleHelper;
import Mesh;

using namespace std;
using namespace Microsoft::WRL;

export class AccelerationStructureComponent {
    shared_ptr<DeviceResourcesInterface>& m_deviceResources;
    CoreInterface* core;
    D3DTexture* m_stoneTexture;
    std::shared_ptr<DescriptorHeap>& m_descriptorHeap;
    std::unordered_map<std::string, std::unique_ptr<MeshGeometry>>& m_geometries;
    std::unordered_map<int, Material>& m_materials;
    D3DTexture* m_templeTextures;
    D3DTexture* m_templeNormalTextures;
    D3DTexture* m_templeSpecularTextures;
    D3DTexture* m_templeEmittanceTextures;
    Microsoft::WRL::ComPtr<ID3D12Resource>* m_bottomLevelAS;
    Microsoft::WRL::ComPtr<ID3D12Resource>& m_topLevelAS;

public:
    AccelerationStructureComponent(
        shared_ptr<DeviceResourcesInterface>& m_deviceResources,
        CoreInterface* core,
        D3DTexture* m_stoneTexture,
        std::shared_ptr<DescriptorHeap>& m_descriptorHeap,
        std::unordered_map<std::string, std::unique_ptr<MeshGeometry>>& m_geometries,
        std::unordered_map<int, Material>& m_materials,
        D3DTexture* m_templeTextures,
        D3DTexture* m_templeNormalTextures,
        D3DTexture* m_templeSpecularTextures,
        D3DTexture* m_templeEmittanceTextures,
        Microsoft::WRL::ComPtr<ID3D12Resource>* m_bottomLevelAS,
        Microsoft::WRL::ComPtr<ID3D12Resource>& m_topLevelAS
    ) :
        m_deviceResources{m_deviceResources},
        core{core},
        m_stoneTexture{m_stoneTexture},
        m_descriptorHeap{ m_descriptorHeap },
        m_geometries{m_geometries},
        m_materials{ m_materials },
        m_templeTextures{ m_templeTextures },
        m_templeNormalTextures{ m_templeNormalTextures },
        m_templeSpecularTextures{ m_templeSpecularTextures },
        m_templeEmittanceTextures{ m_templeEmittanceTextures },
        m_bottomLevelAS{ m_bottomLevelAS },
        m_topLevelAS{ m_topLevelAS }
    {}

    void BuildAccelerationStructures() {
        auto device = m_deviceResources->GetD3DDevice();
        auto commandList = m_deviceResources->GetCommandList();
        auto commandQueue = m_deviceResources->GetCommandQueue();
        auto commandAllocator = m_deviceResources->GetCommandAllocator();

        // Build bottom-level AS.
        AccelerationStructureBuffers bottomLevelAS[BottomLevelASType::Count];
        array<vector<D3D12_RAYTRACING_GEOMETRY_DESC>, BottomLevelASType::Count> geometryDescs;
        {
            core->BuildGeometryDescsForBottomLevelAS(geometryDescs);
            // Build all bottom-level AS.
            for (UINT i = 0; i < BottomLevelASType::Count; i++)
                bottomLevelAS[i] = core->BuildBottomLevelAS(geometryDescs[i]);
        }

        // Batch all resource barriers for bottom-level AS builds.
        D3D12_RESOURCE_BARRIER resourceBarriers[BottomLevelASType::Count];
        for (UINT i = 0; i < BottomLevelASType::Count; i++)
        {
            resourceBarriers[i] = CD3DX12_RESOURCE_BARRIER::UAV(bottomLevelAS[i].accelerationStructure.Get());
        }
        commandList->ResourceBarrier(BottomLevelASType::Count, resourceBarriers);

        // Build top-level AS.
        AccelerationStructureBuffers topLevelAS = core->BuildTopLevelAS(bottomLevelAS);

        TCHAR buffer[MAX_PATH] = { 0 };
        GetModuleFileName(NULL, buffer, MAX_PATH);
        std::wstring::size_type pos = std::wstring(buffer).find_last_of(L"\\/");
        wstring path = std::wstring(buffer).substr(0, pos);

        wstring s(path.begin(), path.end());
        s.append(L"\\..\\..\\Models\\SunTemple\\Textures\\M_Arch_Inst_0_BaseColor.dds");

        string s4(path.begin(), path.end());
        s4.append("\\..\\..\\Models\\SunTemple\\Textures\\");

        m_stoneTexture[0].heapIndex = 3000 + 1;
        LoadDDSTexture(device, commandList, s.c_str(), m_descriptorHeap.get(), &m_stoneTexture[0].resource, &m_stoneTexture[0].upload, &m_stoneTexture[0].heapIndex, &m_stoneTexture[0].cpuDescriptorHandle, &m_stoneTexture[0].gpuDescriptorHandle, D3D12_SRV_DIMENSION_TEXTURE2D);


        std::vector<int> included;
        for (UINT i = 0; i < 1056; i++)
        {
            string geoName = "geo" + to_string(i + 11);
            string meshName = "mesh" + to_string(i + 11);


            int key = m_geometries[geoName]->DrawArgs[meshName].Material.id;


            if (std::find(included.begin(), included.end(), key) != included.end()) {
            }
            else {
                m_materials[key] = m_geometries[geoName]->DrawArgs[meshName].Material;
                included.push_back(key);
            }
        }

        for (int i = 0; i < m_materials.size(); i++) {
            string base = "..\\..\\Models\\SunTemple\\Textures\\";
            base = s4;
            string add = m_materials[i].map_Kd;
            string normalMapPath = m_materials[i].map_Bump;
            string specularMapPath = m_materials[i].map_Ks;
            string emittanceMapPath = m_materials[i].map_Ke;

            std::size_t pos = add.find("\\");
            std::string str3 = add.substr(pos + 1);
            string path = base + str3;
            print(path);
            if (add != "")
                LoadDDSTexture(device, commandList, wstring(path.begin(), path.end()).c_str(), m_descriptorHeap.get(), &m_templeTextures[i]);

            std::size_t pos2 = normalMapPath.find("\\");
            std::string str4 = normalMapPath.substr(pos2 + 1);
            string path2 = base + str4;
            print(path2);
            if (normalMapPath != "")
                LoadDDSTexture(device, commandList, wstring(path2.begin(), path2.end()).c_str(), m_descriptorHeap.get(), &m_templeNormalTextures[i]);

            std::size_t pos3 = specularMapPath.find("\\");
            std::string str5 = specularMapPath.substr(pos3 + 1);
            string path3 = base + str5;
            print(path3);
            if (specularMapPath != "")
                LoadDDSTexture(device, commandList, wstring(path3.begin(), path3.end()).c_str(), m_descriptorHeap.get(), &m_templeSpecularTextures[i]);

            std::size_t pos4 = emittanceMapPath.find("\\");
            std::string str6 = emittanceMapPath.substr(pos4 + 1);
            string path4 = base + str6;
            print(path4);
            if (emittanceMapPath != "")
                LoadDDSTexture(device, commandList, wstring(path4.begin(), path4.end()).c_str(), m_descriptorHeap.get(), &m_templeEmittanceTextures[i]);
        }

        // Kick off acceleration structure construction.
        m_deviceResources->ExecuteCommandList();

        // Wait for GPU to finish as the locally created temporary GPU resources will get released once we go out of scope.
        m_deviceResources->WaitForGpu();

        // Store the AS buffers. The rest of the buffers will be released once we exit the function.
        for (UINT i = 0; i < BottomLevelASType::Count; i++)
        {
            m_bottomLevelAS[i] = bottomLevelAS[i].accelerationStructure;
        }
        m_topLevelAS = topLevelAS.accelerationStructure;
    }
};