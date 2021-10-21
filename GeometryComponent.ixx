module;
#include <string>
#include <Windows.h>
#include <vector>
#include <DirectXMath.h>
#include <iomanip>
#include <ranges>
#include <memory>
#include <unordered_map>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "RayTracingHlslCompat.hlsli"
export module GeometryComponent;

export extern "C" {
#include "Lua542/include/lua.h"
#include "Lua542/include/lauxlib.h"
#include "Lua542/include/lualib.h"
}

import Geometry;
import DeviceResources;
import MeshData;
import Mesh;
import SrvComponent;
import Helper;
import D3DBuffer;
import SrvComponent;
import Globals;

using namespace std;
using namespace DirectX;
using namespace std::views;

export class GeometryComponent {
    DeviceResources* deviceResources;
    vector<int>* meshOffsets;
    vector<int>* meshSizes;
    UINT geoOffset;
    unordered_map<string, unique_ptr<MeshGeometry>>* geometries;
    D3DBuffer* indexBuffer;
    D3DBuffer* vertexBuffer;
    SrvComponent* srvComponent;


public:
    GeometryComponent() {}

    void BuildModel(std::string path, UINT flags, bool usesTextures = false) {
        auto device = deviceResources->GetD3DDevice();

        GeometryGenerator geoGen;
        vector<MeshData> meshes = geoGen.LoadModel(path, flags);

        (*meshOffsets).push_back((*meshOffsets).back() + (*meshSizes).back());
        (*meshSizes).push_back((int)meshes.size());

        for (auto j = 0; j < meshes.size(); j++) {
            MeshGeometry::Submesh submesh{};
            submesh.IndexCount = (UINT)meshes[j].Indices32.size();
            submesh.VertexCount = (UINT)meshes[j].Vertices.size();
            submesh.Material = meshes[j].Material;

            vector<VertexPositionNormalTextureTangent> vertices(meshes[j].Vertices.size());
            for (size_t i = 0; i < meshes[j].Vertices.size(); ++i) {
                vertices[i].position = meshes[j].Vertices[i].Position;
                vertices[i].normal = meshes[j].Vertices[i].Normal;
                vertices[i].textureCoordinate = usesTextures ? meshes[j].Vertices[i].TexC : XMFLOAT2{ 0.0f, 0.0f };
                vertices[i].tangent = usesTextures ? meshes[j].Vertices[i].TangentU : XMFLOAT3{ 1.0f, 0.0f, 0.0f };
            }

            vector<Index> indices;
            indices.insert(indices.end(), begin(meshes[j].Indices32), end(meshes[j].Indices32));

            AllocateUploadBuffer(device, &indices[0], indices.size() * sizeof(Index), &indexBuffer[j + geoOffset].resource);
            AllocateUploadBuffer(device, &vertices[0], vertices.size() * sizeof(VertexPositionNormalTextureTangent), &vertexBuffer[j + geoOffset].resource);

            srvComponent->CreateBufferSRV(&indexBuffer[j + geoOffset], (UINT)indices.size(), (UINT)sizeof(Index));
            srvComponent->CreateBufferSRV(&vertexBuffer[j + geoOffset], (UINT)vertices.size(), (UINT)sizeof(VertexPositionNormalTextureTangent));

            unique_ptr<MeshGeometry> geo = make_unique<MeshGeometry>();
            string tmp = "geo" + to_string(j + geoOffset);
            geo->Name = tmp;
            geo->VertexBufferGPU = vertexBuffer[j + geoOffset].resource;
            geo->IndexBufferGPU = indexBuffer[j + geoOffset].resource;
            geo->VertexByteStride = sizeof(VertexPositionNormalTextureTangent);
            geo->IndexFormat = DXGI_FORMAT_R32_UINT;
            geo->VertexBufferByteSize = (UINT)vertices.size() * sizeof(VertexPositionNormalTextureTangent);
            geo->IndexBufferByteSize = (UINT)indices.size() * sizeof(Index);
            tmp = "mesh" + to_string(j + geoOffset);
            geo->DrawArgs[tmp] = submesh;
            (*geometries)[geo->Name] = move(geo);
        }

        geoOffset += (UINT)meshes.size();
    }

    void BuildGeometry()
    {
        auto device = deviceResources->GetD3DDevice();

        GeometryGenerator geoGen;
        Material nullMaterial;
        nullMaterial.id = 0;

        XMFLOAT3 sizes[] = {
            XMFLOAT3(30.0f, 15.0f, 30.0f),
            XMFLOAT3(20.0f, 0.01f, 0.01f),
            XMFLOAT3(0.0f, 0.0f, 0.0f)
        };

        lua_State* L = luaL_newstate();
        luaL_openlibs(L);

        //lua_register(L, "HostFunction", lua_HostFunction);

        /*if (CheckLua(L, luaL_dofile(L, "LuaScripts/MyScript.lua"))) {
            lua_getglobal(L, "coordinatesSize");
            if (lua_istable(L, -1)) {
                lua_pushstring(L, "x");
                lua_gettable(L, -2);
                float x = (float)lua_tonumber(L, -1);
                lua_pop(L, 1);

                lua_pushstring(L, "y");
                lua_gettable(L, -2);
                float y = (float)lua_tonumber(L, -1);
                lua_pop(L, 1);

                lua_pushstring(L, "z");
                lua_gettable(L, -2);
                float z = (float)lua_tonumber(L, -1);
                lua_pop(L, 1);

                sizes[1] = XMFLOAT3(x, y, z);
            }
        }*/
        lua_close(L);

        MeshData(GeometryGenerator:: * createGeo[3])(float width, float height, float depth) = {
            &GeometryGenerator::CreateRoom, &GeometryGenerator::CreateCoordinates, &GeometryGenerator::CreateSkull
        };

        for (auto i : iota(0, 3)) {
            MeshData geo = (geoGen.*createGeo[i])(sizes[i].x, sizes[i].y, sizes[i].z);
            UINT roomVertexOffset = 0;
            UINT roomIndexOffset = 0;
            MeshGeometry::Submesh roomSubmesh;
            roomSubmesh.IndexCount = (UINT)geo.Indices32.size();
            roomSubmesh.StartIndexLocation = roomIndexOffset;
            roomSubmesh.VertexCount = (UINT)geo.Vertices.size();
            roomSubmesh.BaseVertexLocation = roomVertexOffset;
            roomSubmesh.Material = nullMaterial;
            std::vector<VertexPositionNormalTextureTangent> roomVertices(geo.Vertices.size());
            UINT k = 0;
            for (size_t i = 0; i < geo.Vertices.size(); ++i, ++k)
            {
                roomVertices[k].position = geo.Vertices[i].Position;
                roomVertices[k].normal = geo.Vertices[i].Normal;
                roomVertices[k].textureCoordinate = geo.Vertices[i].TexC;
                roomVertices[k].tangent = geo.Vertices[i].TangentU;
            }
            std::vector<std::uint32_t> roomIndices;
            roomIndices.insert(roomIndices.end(), begin(geo.Indices32), end(geo.Indices32));
            const UINT roomibByteSize = (UINT)roomIndices.size() * sizeof(std::uint32_t);
            const UINT roomvbByteSize = (UINT)roomVertices.size() * sizeof(VertexPositionNormalTextureTangent);
            AllocateUploadBuffer(device, &roomIndices[0], roomibByteSize, &indexBuffer[i].resource);
            AllocateUploadBuffer(device, &roomVertices[0], roomvbByteSize, &vertexBuffer[i].resource);
            srvComponent->CreateBufferSRV(&indexBuffer[i], (UINT)roomIndices.size(), (UINT)sizeof(uint32_t));
            srvComponent->CreateBufferSRV(&vertexBuffer[i], (UINT)roomVertices.size(), (UINT)sizeof(roomVertices[0]));
            auto roomGeo = std::make_unique<MeshGeometry>();

            string geoName = "geo" + to_string(i);
            string meshName = "mesh" + to_string(i);
            roomGeo->Name = geoName;
            roomGeo->VertexBufferGPU = vertexBuffer[i].resource;
            roomGeo->IndexBufferGPU = indexBuffer[i].resource;
            roomGeo->VertexByteStride = sizeof(VertexPositionNormalTextureTangent);
            roomGeo->IndexFormat = DXGI_FORMAT_R32_UINT;
            roomGeo->VertexBufferByteSize = roomvbByteSize;
            roomGeo->IndexBufferByteSize = roomibByteSize;
            roomGeo->DrawArgs[meshName] = roomSubmesh;
            (*geometries)[roomGeo->Name] = std::move(roomGeo);
            geoOffset++;
        }

        (*meshOffsets).push_back(0);
        (*meshSizes).push_back(1);
        (*meshOffsets).push_back((*meshOffsets).back() + (*meshSizes).back());
        (*meshSizes).push_back(1);
        (*meshOffsets).push_back((*meshOffsets).back() + (*meshSizes).back());
        (*meshSizes).push_back(1);

        char dir[200];
        GetCurrentDirectoryA(sizeof(dir), dir);

        TCHAR buffer[MAX_PATH] = { 0 };
        GetModuleFileName(NULL, buffer, MAX_PATH);
        std::wstring::size_type pos = std::wstring(buffer).find_last_of(L"\\/");
        wstring path = std::wstring(buffer).substr(0, pos);

        string s;
        string s2;
        string s3;
        std::transform(path.begin(), path.end(), std::back_inserter(s), [](wchar_t c) { return (char)c; });
        s.append("\\..\\..\\Models\\table.obj");
        std::transform(path.begin(), path.end(), std::back_inserter(s2), [](wchar_t c) { return (char)c; });
        s2.append("\\..\\..\\Models\\lamp.obj");
        std::transform(path.begin(), path.end(), std::back_inserter(s3), [](wchar_t c) { return (char)c; });
        s3.append("\\..\\..\\Models\\SunTemple\\SunTemple.fbx");

        BuildModel(s, aiProcess_Triangulate | aiProcess_FlipUVs, false);
        BuildModel(s2, aiProcess_Triangulate | aiProcess_FlipUVs, false);
        BuildModel(s3, aiProcess_Triangulate | aiProcess_FlipUVs, true);
    }
};