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
#include <wrl/client.h>

#include "RaytracingSceneDefines.h"
export module AccelerationStructureComponent;

//import Helper;
//import D3DBuffer;
//import Transform;

import DeviceResources;
import D3DTexture;
import Material;
import Mesh;
import DescriptorHeap;
import AccelerationStructureBuffers;
import DXSampleHelper;
import BottomLevelASComponent;
import TopLevelASComponent;

using namespace std;
using namespace DirectX;
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
    unordered_map<string, unique_ptr<MeshGeometry>>* geometries;
    DescriptorHeap* descriptorHeap;
    vector<int>* meshSizes;
    D3DTexture** templeTextures;
    D3DTexture** templeNormalTextures;
    D3DTexture** templeSpecularTextures;
    D3DTexture** templeEmittanceTextures;

    BottomLevelASComponent* bottomLevelAsComponent;
    TopLevelASComponent* topLevelAsComponent;
    
public:
    AccelerationStructureComponent() {}

    void BuildAccelerationStructures() {
        auto device = deviceResources->GetD3DDevice();
        auto commandList = deviceResources->GetCommandList();
        auto commandQueue = deviceResources->GetCommandQueue();
        auto commandAllocator = deviceResources->GetCommandAllocator();

        // Build bottom-level AS.
        AccelerationStructureBuffers bottomLevelAS[BottomLevelASType::Count];
        array<vector<D3D12_RAYTRACING_GEOMETRY_DESC>, BottomLevelASType::Count> geometryDescs;
        {
            bottomLevelAsComponent->BuildGeometryDescsForBottomLevelAS(geometryDescs);
            // Build all bottom-level AS.
            for (UINT i = 0; i < BottomLevelASType::Count; i++)
                bottomLevelAS[i] = bottomLevelAsComponent->BuildBottomLevelAS(geometryDescs[i]);
        }

        // Batch all resource barriers for bottom-level AS builds.
        D3D12_RESOURCE_BARRIER resourceBarriers[BottomLevelASType::Count];
        for (UINT i = 0; i < BottomLevelASType::Count; i++)
        {
            resourceBarriers[i] = CD3DX12_RESOURCE_BARRIER::UAV(bottomLevelAS[i].accelerationStructure.Get());
        }
        commandList->ResourceBarrier(BottomLevelASType::Count, resourceBarriers);

        // Build top-level AS.
        AccelerationStructureBuffers topLevelAS = topLevelAsComponent->BuildTopLevelAS(bottomLevelAS);

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
};