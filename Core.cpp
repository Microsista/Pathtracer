#include "stdafx.h"
#include "Core.h"
#include "Obj/Debug/CompiledShaders/ViewRG.hlsl.h"
#include "Obj/Debug/CompiledShaders/RadianceCH.hlsl.h"
#include "Obj/Debug/CompiledShaders/RadianceMS.hlsl.h"
#include "Obj/Debug/CompiledShaders/ShadowMS.hlsl.h"
#include "Obj/Debug/CompiledShaders/CompositionCS.hlsl.h"

#include "Geometry.h"
#include "Texture.h"

using namespace std;
using namespace DX;
using namespace util::lang;

const wchar_t* Core::c_raygenShaderName = L"MyRaygenShader";
const wchar_t* Core::c_closestHitShaderName = L"MyClosestHitShader_Triangle";
const wchar_t* Core::c_missShaderNames[] = { L"MyMissShader", L"MyMissShader_ShadowRay" };
const wchar_t* Core::c_hitGroupNames_TriangleGeometry[] = { L"MyHitGroup_Triangle", L"MyHitGroup_Triangle_ShadowRay" };

void Core::CreateRootSignatures() {
    auto device = m_deviceResources->GetD3DDevice();

    // Global Root Signature
    {
        CD3DX12_DESCRIPTOR_RANGE ranges[3] = {};
        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
        ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1);
        ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 2);

        CD3DX12_ROOT_PARAMETER rootParameters[GlobalRootSignature::Slot::Count];
        rootParameters[GlobalRootSignature::Slot::OutputView].InitAsDescriptorTable(1, &ranges[0]);
        rootParameters[GlobalRootSignature::Slot::AccelerationStructure].InitAsShaderResourceView(0);
        rootParameters[GlobalRootSignature::Slot::SceneConstant].InitAsConstantBufferView(0);
        rootParameters[GlobalRootSignature::Slot::TriangleAttributeBuffer].InitAsShaderResourceView(4);
        rootParameters[GlobalRootSignature::Slot::ReflectionBuffer].InitAsDescriptorTable(1, &ranges[1]);
        rootParameters[GlobalRootSignature::Slot::ShadowBuffer].InitAsDescriptorTable(1, &ranges[2]);

        CD3DX12_STATIC_SAMPLER_DESC staticSamplers[] = { CD3DX12_STATIC_SAMPLER_DESC(0, SAMPLER_FILTER) }; // LinearWrapSampler

        CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters, ARRAYSIZE(staticSamplers), staticSamplers);
        SerializeAndCreateRaytracingRootSignature(device, globalRootSignatureDesc, &m_raytracingGlobalRootSignature, L"Global root signature");
    }

    // Local Root Signature
    using namespace LocalRootSignature::Triangle;
    CD3DX12_DESCRIPTOR_RANGE ranges[Slot::Count] = {};
    ranges[Slot::IndexBuffer].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
    ranges[Slot::VertexBuffer].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
    ranges[Slot::DiffuseTexture].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3);

    namespace RootSignatureSlots = LocalRootSignature::Triangle::Slot;
    CD3DX12_ROOT_PARAMETER rootParameters[RootSignatureSlots::Count] = {};

    rootParameters[RootSignatureSlots::MaterialConstant].InitAsConstants(SizeOfInUint32(PrimitiveConstantBuffer), 1);
    rootParameters[RootSignatureSlots::GeometryIndex].InitAsConstants(SizeOfInUint32(PrimitiveInstanceConstantBuffer), 3);
    rootParameters[RootSignatureSlots::IndexBuffer].InitAsDescriptorTable(1, &ranges[Slot::IndexBuffer]);
    rootParameters[RootSignatureSlots::VertexBuffer].InitAsDescriptorTable(1, &ranges[Slot::VertexBuffer]);
    rootParameters[RootSignatureSlots::DiffuseTexture].InitAsDescriptorTable(1, &ranges[Slot::DiffuseTexture]);
    CD3DX12_ROOT_SIGNATURE_DESC localRootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters);
    localRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
    SerializeAndCreateRaytracingRootSignature(device, localRootSignatureDesc, &m_raytracingLocalRootSignature[LocalRootSignature::Type::Triangle], L"Local root signature");
}

void Core::CreateLocalRootSignatureSubobjects(CD3DX12_STATE_OBJECT_DESC* raytracingPipeline) {
    // Hit groups
    auto localRootSignature = raytracingPipeline->CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
    localRootSignature->SetRootSignature(m_raytracingLocalRootSignature[LocalRootSignature::Type::Triangle].Get());
    // Shader association
    auto rootSignatureAssociation = raytracingPipeline->CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
    rootSignatureAssociation->SetSubobjectToAssociate(*localRootSignature);
    rootSignatureAssociation->AddExports(c_hitGroupNames_TriangleGeometry);
}

void Core::CreateRaytracingPipelineStateObject() {
    CD3DX12_STATE_OBJECT_DESC raytracingPipeline{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };
    CreateDxilLibrarySubobjects(&raytracingPipeline);
    CreateHitGroupSubobjects(&raytracingPipeline);
    auto shaderConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
    UINT payloadSize = max(sizeof(RayPayload), sizeof(ShadowRayPayload));
    UINT attributeSize = sizeof(struct ProceduralPrimitiveAttributes);
    shaderConfig->Config(payloadSize, attributeSize);
    CreateLocalRootSignatureSubobjects(&raytracingPipeline);
    auto globalRootSignature = raytracingPipeline.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
    globalRootSignature->SetRootSignature(m_raytracingGlobalRootSignature.Get());
    auto pipelineConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
    UINT maxRecursionDepth = MAX_RAY_RECURSION_DEPTH;
    pipelineConfig->Config(maxRecursionDepth);
    PrintStateObjectDesc(raytracingPipeline);
    ThrowIfFailed(m_dxrDevice->CreateStateObject(raytracingPipeline, IID_PPV_ARGS(&m_dxrStateObject)), L"Couldn't create DirectX Raytracing state object.\n");
}

void Core::CreateDescriptorHeap() {
    auto device = m_deviceResources->GetD3DDevice();

    m_descriptorHeap = std::make_shared<DX::DescriptorHeap>(device, 10000, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_descriptorSize = m_descriptorHeap->DescriptorSize();
}

void Core::BuildModel(string path, UINT flags, bool usesTextures = false) {
    auto device = m_deviceResources->GetD3DDevice();

    GeometryGenerator geoGen;
    vector<GeometryGenerator::MeshData> meshes = geoGen.LoadModel(path, flags);
 
    for (auto j = 0; j < meshes.size(); j++) {
        MeshGeometry::Submesh submesh{};
        submesh.IndexCount = meshes[j].Indices32.size();
        submesh.VertexCount = meshes[j].Vertices.size();
        submesh.Material = meshes[j].Material;
    
        vector<VertexPositionNormalTextureTangent> vertices(meshes[j].Vertices.size());
        for (size_t i = 0; i < meshes[j].Vertices.size(); ++i) {
            vertices[i].position = meshes[j].Vertices[i].Position;
            vertices[i].normal = meshes[j].Vertices[i].Normal;
            vertices[i].textureCoordinate = usesTextures ? meshes[j].Vertices[i].TexC : XMFLOAT2{0.0f, 0.0f};
            vertices[i].tangent = usesTextures ? meshes[j].Vertices[i].TangentU : XMFLOAT3{1.0f, 0.0f, 0.0f};
        }
    
        vector<Index> indices;
        indices.insert(indices.end(), begin(meshes[j].Indices32), end(meshes[j].Indices32));

        AllocateUploadBuffer(device, &indices[0], indices.size() * sizeof(Index), &m_indexBuffer[j + m_geoOffset].resource);
        AllocateUploadBuffer(device, &vertices[0], vertices.size() * sizeof(VertexPositionNormalTextureTangent), &m_vertexBuffer[j + m_geoOffset].resource);
  
        CreateBufferSRV(&m_indexBuffer[j + m_geoOffset], indices.size(), sizeof(Index));
        CreateBufferSRV(&m_vertexBuffer[j + m_geoOffset], vertices.size(), sizeof(VertexPositionNormalTextureTangent));

        unique_ptr<MeshGeometry> geo = make_unique<MeshGeometry>();
        string tmp = "geo" + to_string(j + m_geoOffset);
        geo->Name = tmp;
        geo->VertexBufferGPU = m_vertexBuffer[j + m_geoOffset].resource;
        geo->IndexBufferGPU = m_indexBuffer[j + m_geoOffset].resource;
        geo->VertexByteStride = sizeof(VertexPositionNormalTextureTangent);
        geo->IndexFormat = DXGI_FORMAT_R32_UINT;
        geo->VertexBufferByteSize = vertices.size() * sizeof(VertexPositionNormalTextureTangent);
        geo->IndexBufferByteSize = indices.size() * sizeof(Index);
        tmp = "mesh" + to_string(j + m_geoOffset);
        geo->DrawArgs[tmp] = submesh;
        m_geometries[geo->Name] = move(geo);
    }

    m_geoOffset += meshes.size();
}

void Core::BuildGeometry()
{
    auto device = m_deviceResources->GetD3DDevice();
    
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData room = geoGen.CreateRoom(30.0f, 15.0f, 30.0f);
    GeometryGenerator::MeshData coordinateSystem = geoGen.CreateCoordinates(20.0f, 0.01f, 0.01f);
    GeometryGenerator::MeshData skull = geoGen.CreateSkull(0.0f, 0.0f, 0.0f);
 
    UINT roomVertexOffset = 0;
    UINT coordinateSystemVertexOffset = 0;
    UINT skullVertexOffset = 0;
    vector<UINT> modelMeshesVertexOffsets;

    UINT roomIndexOffset = 0;
    UINT coordinateSystemIndexOffset = 0;
    UINT skullIndexOffset = 0;
    vector<UINT> modelMeshesIndexOffsets;
  
    MeshGeometry::Submesh roomSubmesh;
    roomSubmesh.IndexCount = (UINT)room.Indices32.size();
    roomSubmesh.StartIndexLocation = roomIndexOffset;
    roomSubmesh.VertexCount = (UINT)room.Vertices.size();
    roomSubmesh.BaseVertexLocation = roomVertexOffset;

    MeshGeometry::Submesh coordinateSystemSubmesh;
    coordinateSystemSubmesh.IndexCount = (UINT)coordinateSystem.Indices32.size();
    coordinateSystemSubmesh.StartIndexLocation = coordinateSystemIndexOffset;
    coordinateSystemSubmesh.VertexCount = (UINT)coordinateSystem.Vertices.size();
    coordinateSystemSubmesh.BaseVertexLocation = coordinateSystemVertexOffset;

    MeshGeometry::Submesh skullSubmesh;
    skullSubmesh.IndexCount = (UINT)skull.Indices32.size();
    skullSubmesh.StartIndexLocation = skullIndexOffset;
    skullSubmesh.VertexCount = (UINT)skull.Vertices.size();
    skullSubmesh.BaseVertexLocation = skullVertexOffset;

    std::vector<VertexPositionNormalTextureTangent> roomVertices(room.Vertices.size());
    UINT k = 0;
    for (size_t i = 0; i < room.Vertices.size(); ++i, ++k)
    {
        roomVertices[k].position = room.Vertices[i].Position;
        roomVertices[k].normal = room.Vertices[i].Normal;
        roomVertices[k].textureCoordinate = room.Vertices[i].TexC;
        roomVertices[k].tangent = room.Vertices[i].TangentU;
    }

    std::vector<VertexPositionNormalTextureTangent> coordinatesVertices(coordinateSystem.Vertices.size());
    k = 0;
    for (size_t i = 0; i < coordinateSystem.Vertices.size(); ++i, ++k)
    {
        coordinatesVertices[k].position = coordinateSystem.Vertices[i].Position;
        coordinatesVertices[k].normal = coordinateSystem.Vertices[i].Normal;
        coordinatesVertices[k].textureCoordinate = coordinateSystem.Vertices[i].TexC;
        coordinatesVertices[k].tangent = coordinateSystem.Vertices[i].TangentU;
    }

    std::vector<VertexPositionNormalTextureTangent> skullVertices(skull.Vertices.size());
    k = 0;
    for (size_t i = 0; i < skull.Vertices.size(); ++i, ++k)
    {
        skullVertices[k].position = skull.Vertices[i].Position;
        skullVertices[k].normal = skull.Vertices[i].Normal;
        skullVertices[k].textureCoordinate = { 0.0f, 0.0f };// skull.Vertices[i].TexC;
        skullVertices[k].tangent = { 1.0f, 0.0f, 0.0f };//skull.Vertices[i].TangentU;
    }
    
    std::vector<std::uint32_t> roomIndices;
    roomIndices.insert(roomIndices.end(), begin(room.Indices32), end(room.Indices32));

    std::vector<std::uint32_t> coordinateSystemIndices;
    coordinateSystemIndices.insert(coordinateSystemIndices.end(), begin(coordinateSystem.Indices32), end(coordinateSystem.Indices32));

    std::vector<std::uint32_t> skullIndices;
    skullIndices.insert(skullIndices.end(), begin(skull.Indices32), end(skull.Indices32));

    const UINT roomibByteSize = (UINT)roomIndices.size() * sizeof(std::uint32_t);
    const UINT roomvbByteSize = (UINT)roomVertices.size() * sizeof(VertexPositionNormalTextureTangent);

    const UINT csibByteSize = (UINT)coordinateSystemIndices.size() * sizeof(std::uint32_t);
    const UINT csvbByteSize = (UINT)coordinatesVertices.size() * sizeof(VertexPositionNormalTextureTangent);

    const UINT skullibByteSize = (UINT)skullIndices.size() * sizeof(std::uint32_t);
    const UINT skullvbByteSize = (UINT)skullVertices.size() * sizeof(VertexPositionNormalTextureTangent);

    AllocateUploadBuffer(device, &roomIndices[0], roomibByteSize, &m_indexBuffer[TriangleGeometry::Room].resource);
    AllocateUploadBuffer(device, &roomVertices[0], roomvbByteSize, &m_vertexBuffer[TriangleGeometry::Room].resource);

    AllocateUploadBuffer(device, &coordinateSystemIndices[0], csibByteSize, &m_indexBuffer[CoordinateGeometry::Coordinates + TriangleGeometry::Count].resource);
    AllocateUploadBuffer(device, &coordinatesVertices[0], csvbByteSize, &m_vertexBuffer[CoordinateGeometry::Coordinates + TriangleGeometry::Count].resource);

    AllocateUploadBuffer(device, &skullIndices[0], skullibByteSize, &m_indexBuffer[SkullGeometry::Skull + 2].resource);
    AllocateUploadBuffer(device, &skullVertices[0], skullvbByteSize, &m_vertexBuffer[SkullGeometry::Skull + 2].resource);

    CreateBufferSRV(&m_indexBuffer[TriangleGeometry::Room], roomIndices.size(), sizeof(uint32_t));
    CreateBufferSRV(&m_vertexBuffer[TriangleGeometry::Room], roomVertices.size(), sizeof(roomVertices[0]));

    CreateBufferSRV(&m_indexBuffer[CoordinateGeometry::Coordinates + 1], coordinateSystemIndices.size(), sizeof(uint32_t));
    CreateBufferSRV(&m_vertexBuffer[CoordinateGeometry::Coordinates + 1], coordinatesVertices.size(), sizeof(coordinatesVertices[0]));

    CreateBufferSRV(&m_indexBuffer[SkullGeometry::Skull + 2], skullIndices.size(), sizeof(uint32_t));
    CreateBufferSRV(&m_vertexBuffer[SkullGeometry::Skull + 2], skullVertices.size(), sizeof(roomVertices[0]));

    auto roomGeo = std::make_unique<MeshGeometry>();
    auto csGeo = std::make_unique<MeshGeometry>();
    auto skullGeo = std::make_unique<MeshGeometry>();

    roomGeo->Name = "roomGeo";
    csGeo->Name = "csGeo";
    skullGeo->Name = "skullGeo";

    roomGeo->VertexBufferGPU = m_vertexBuffer[TriangleGeometry::Room].resource;
    roomGeo->IndexBufferGPU = m_indexBuffer[TriangleGeometry::Room].resource;

    csGeo->VertexBufferGPU = m_vertexBuffer[CoordinateGeometry::Coordinates].resource;
    csGeo->IndexBufferGPU = m_indexBuffer[CoordinateGeometry::Coordinates].resource;

    skullGeo->VertexBufferGPU = m_vertexBuffer[SkullGeometry::Skull].resource;
    skullGeo->IndexBufferGPU = m_indexBuffer[SkullGeometry::Skull].resource;

    roomGeo->VertexByteStride = sizeof(VertexPositionNormalTextureTangent);
    csGeo->VertexByteStride = sizeof(VertexPositionNormalTextureTangent);
    skullGeo->VertexByteStride = sizeof(VertexPositionNormalTextureTangent);

    roomGeo->IndexFormat = DXGI_FORMAT_R32_UINT;
    csGeo->IndexFormat = DXGI_FORMAT_R32_UINT;
    skullGeo->IndexFormat = DXGI_FORMAT_R32_UINT;

    roomGeo->VertexBufferByteSize = roomvbByteSize;
    csGeo->VertexBufferByteSize = csvbByteSize;
    skullGeo->VertexBufferByteSize = skullvbByteSize;

    roomGeo->IndexBufferByteSize = roomibByteSize;
    csGeo->IndexBufferByteSize = csibByteSize;
    skullGeo->IndexBufferByteSize = skullibByteSize;

    roomGeo->DrawArgs["room"] = roomSubmesh;
    csGeo->DrawArgs["coordinateSystem"] = coordinateSystemSubmesh;
    skullGeo->DrawArgs["skull"] = skullSubmesh;

    m_geometries[roomGeo->Name] = std::move(roomGeo);
    m_geometries[csGeo->Name] = std::move(csGeo);
    m_geometries[skullGeo->Name] = std::move(skullGeo);

    m_geoOffset += 3;

    char dir[200];
    GetCurrentDirectoryA(sizeof(dir), dir);

    TCHAR buffer[MAX_PATH] = { 0 };
    GetModuleFileName(NULL, buffer, MAX_PATH);
    std::wstring::size_type pos = std::wstring(buffer).find_last_of(L"\\/");
    wstring path = std::wstring(buffer).substr(0, pos);

    string s(path.begin(), path.end());
    s.append("\\..\\..\\Models\\table.obj");
    string s2(path.begin(), path.end());
    s2.append("\\..\\..\\Models\\lamp.obj");
    string s3(path.begin(), path.end());
    s3.append("\\..\\..\\Models\\SunTemple\\SunTemple.fbx");

    BuildModel(s, aiProcess_Triangulate | aiProcess_FlipUVs, false);
    BuildModel(s2, aiProcess_Triangulate | aiProcess_FlipUVs, false);
    BuildModel(s3, aiProcess_Triangulate | aiProcess_FlipUVs, true);
}

void Core::BuildGeometryDescsForBottomLevelAS(array<vector<D3D12_RAYTRACING_GEOMETRY_DESC>, BottomLevelASType::Count>& geometryDescs)
{
    D3D12_RAYTRACING_GEOMETRY_FLAGS geometryFlags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

    D3D12_RAYTRACING_GEOMETRY_DESC triangleDescTemplate{};
    triangleDescTemplate.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    triangleDescTemplate.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
    triangleDescTemplate.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
    triangleDescTemplate.Triangles.VertexBuffer.StrideInBytes = sizeof(VertexPositionNormalTextureTangent);
    triangleDescTemplate.Flags = geometryFlags;

    {
        geometryDescs[BottomLevelASType::Triangle].resize(TriangleGeometry::Count, triangleDescTemplate);
        auto& geometryDesc = geometryDescs[BottomLevelASType::Triangle][0];
        geometryDesc.Triangles.IndexBuffer = m_indexBuffer[AllGeometry::Room].resource->GetGPUVirtualAddress();
        geometryDesc.Triangles.VertexBuffer.StartAddress = m_vertexBuffer[AllGeometry::Room].resource->GetGPUVirtualAddress();
        geometryDesc.Triangles.IndexCount = m_geometries["roomGeo"]->DrawArgs["room"].IndexCount;
        geometryDesc.Triangles.VertexCount = m_geometries["roomGeo"]->DrawArgs["room"].VertexCount;
    }
    {
        geometryDescs[BottomLevelASType::Coordinates].resize(CoordinateGeometry::Count, triangleDescTemplate);
        auto& geometryDesc = geometryDescs[BottomLevelASType::Coordinates][0];
        geometryDesc.Triangles.IndexBuffer = m_indexBuffer[AllGeometry::Coordinates].resource->GetGPUVirtualAddress();
        geometryDesc.Triangles.VertexBuffer.StartAddress = m_vertexBuffer[AllGeometry::Coordinates].resource->GetGPUVirtualAddress();
        geometryDesc.Triangles.IndexCount = m_geometries["csGeo"]->DrawArgs["coordinateSystem"].IndexCount;
        geometryDesc.Triangles.VertexCount = m_geometries["csGeo"]->DrawArgs["coordinateSystem"].VertexCount; }
    
    {
        geometryDescs[BottomLevelASType::Skull].resize(SkullGeometry::Count, triangleDescTemplate);
        auto& geometryDesc = geometryDescs[BottomLevelASType::Skull][0];
        geometryDesc.Triangles.IndexBuffer = m_indexBuffer[AllGeometry::Skull].resource->GetGPUVirtualAddress();
        geometryDesc.Triangles.VertexBuffer.StartAddress = m_vertexBuffer[AllGeometry::Skull].resource->GetGPUVirtualAddress();
        geometryDesc.Triangles.IndexCount = m_geometries["skullGeo"]->DrawArgs["skull"].IndexCount;
        geometryDesc.Triangles.VertexCount = m_geometries["skullGeo"]->DrawArgs["skull"].VertexCount;
    }
   

    geometryDescs[BottomLevelASType::Table].resize(TableGeometry::Count, triangleDescTemplate);
    for (UINT i = 0; i < TableGeometry::Count; i++)
    {
        auto& geometryDesc = geometryDescs[BottomLevelASType::Table][i];

        string geoName = "geo" + to_string(i + 3);
        string meshName = "mesh" + to_string(i + 3);
        geometryDesc.Triangles.IndexBuffer = m_indexBuffer[3 + i].resource->GetGPUVirtualAddress();
        geometryDesc.Triangles.VertexBuffer.StartAddress = m_vertexBuffer[3 + i].resource->GetGPUVirtualAddress();
        geometryDesc.Triangles.IndexCount = m_geometries[geoName]->DrawArgs[meshName].IndexCount;
        geometryDesc.Triangles.VertexCount = m_geometries[geoName]->DrawArgs[meshName].VertexCount;
    }

    geometryDescs[BottomLevelASType::Lamps].resize(LampsGeometry::Count, triangleDescTemplate);
    for (UINT i = 0; i < 4; i++) {
        auto& geometryDesc = geometryDescs[BottomLevelASType::Lamps][i];

        string geoName = "geo" + to_string(i + 7);
        string meshName = "mesh" + to_string(i + 7);
        geometryDesc.Triangles.IndexBuffer = m_indexBuffer[7 + i].resource->GetGPUVirtualAddress();
        geometryDesc.Triangles.VertexBuffer.StartAddress = m_vertexBuffer[7 + i].resource->GetGPUVirtualAddress();
        geometryDesc.Triangles.IndexCount = m_geometries[geoName]->DrawArgs[meshName].IndexCount;
        geometryDesc.Triangles.VertexCount = m_geometries[geoName]->DrawArgs[meshName].VertexCount;
    }

    geometryDescs[BottomLevelASType::SunTemple].resize(1056, triangleDescTemplate);
    for (UINT i = 0; i < 1056; i++) {
        auto& geometryDesc = geometryDescs[BottomLevelASType::SunTemple][i];

        string geoName = "geo" + to_string(i +11);
        string meshName = "mesh" + to_string(i + 11);
        geometryDesc.Triangles.IndexBuffer = m_indexBuffer[11 + i].resource->GetGPUVirtualAddress();
        geometryDesc.Triangles.VertexBuffer.StartAddress = m_vertexBuffer[11 + i].resource->GetGPUVirtualAddress();
        geometryDesc.Triangles.IndexCount = m_geometries[geoName]->DrawArgs[meshName].IndexCount;
        geometryDesc.Triangles.VertexCount = m_geometries[geoName]->DrawArgs[meshName].VertexCount;
    }
}

void Core::BuildShaderTables()
{
    auto device = m_deviceResources->GetD3DDevice();

    void* rayGenShaderID;
    void* missShaderIDs[RayType::Count];
    void* hitGroupShaderIDs_TriangleGeometry[RayType::Count];

    // A shader name look-up table for shader table debug print out.
    unordered_map<void*, wstring> shaderIdToStringMap;

    auto GetShaderIDs = [&](auto* stateObjectProperties)
    {
        rayGenShaderID = stateObjectProperties->GetShaderIdentifier(c_raygenShaderName);
        shaderIdToStringMap[rayGenShaderID] = c_raygenShaderName;

        for (UINT i = 0; i < RayType::Count; i++)
        {
            missShaderIDs[i] = stateObjectProperties->GetShaderIdentifier(c_missShaderNames[i]);
            shaderIdToStringMap[missShaderIDs[i]] = c_missShaderNames[i];
        }
        for (UINT i = 0; i < RayType::Count; i++)
        {
            hitGroupShaderIDs_TriangleGeometry[i] = stateObjectProperties->GetShaderIdentifier(c_hitGroupNames_TriangleGeometry[i]);
            shaderIdToStringMap[hitGroupShaderIDs_TriangleGeometry[i]] = c_hitGroupNames_TriangleGeometry[i];
        }
    };

    // Get shader identifiers.
    UINT shaderIDSize;
    {
        ComPtr<ID3D12StateObjectProperties> stateObjectProperties;
        ThrowIfFailed(m_dxrStateObject.As(&stateObjectProperties));
        GetShaderIDs(stateObjectProperties.Get());
        shaderIDSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    }

     // RayGen shader table.
    {
        UINT numShaderRecords = 1;
        UINT shaderRecordSize = shaderIDSize; // No root arguments
        
        ShaderTable rayGenShaderTable(device, numShaderRecords, shaderRecordSize, L"RayGenShaderTable" );
        rayGenShaderTable.push_back(ShaderRecord(rayGenShaderID, shaderRecordSize, nullptr, 0));
        rayGenShaderTable.DebugPrint(shaderIdToStringMap);
        m_rayGenShaderTable = rayGenShaderTable.GetResource();
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
        m_missShaderTableStrideInBytes = missShaderTable.GetShaderRecordSize();
        m_missShaderTable = missShaderTable.GetResource();
    }

    // Hit group shader table.
    {
        UINT numShaderRecords = NUM_BLAS * RayType::Count;
        UINT shaderRecordSize = shaderIDSize + LocalRootSignature::MaxRootArgumentsSize();
        ShaderTable hitGroupShaderTable(device, numShaderRecords, shaderRecordSize, L"HitGroupShaderTable");

        // Triangle geometry hit groups.
        {
            LocalRootSignature::Triangle::RootArguments rootArgs;

            // Create a shader record for each primitive.
            for (UINT instanceIndex = 0; instanceIndex < TriangleGeometry::Count; instanceIndex++)
            {
                rootArgs.materialCb = m_triangleMaterialCB[instanceIndex];
                rootArgs.triangleCB.instanceIndex = instanceIndex;
                auto ib = m_indexBuffer[instanceIndex].gpuDescriptorHandle;
                auto vb = m_vertexBuffer[instanceIndex].gpuDescriptorHandle;
                auto texture = m_templeTextures[0].gpuDescriptorHandle;
                memcpy(&rootArgs.indexBufferGPUHandle, &ib, sizeof(ib));
                memcpy(&rootArgs.vertexBufferGPUHandle, &vb, sizeof(ib));
                memcpy(&rootArgs.diffuseTextureGPUHandle, &texture, sizeof(ib));

                // Ray types
                for (UINT r = 0; r < RayType::Count; r++)
                {
                    auto& hitGroupShaderID = hitGroupShaderIDs_TriangleGeometry[r];
                    hitGroupShaderTable.push_back(ShaderRecord(hitGroupShaderID, shaderIDSize, &rootArgs, sizeof(rootArgs)));
                }
            }
        }

        // Coordinate
        {
            LocalRootSignature::Triangle::RootArguments rootArgs;

            // Create a shader record for each primitive.
            for (UINT instanceIndex = 0; instanceIndex < CoordinateGeometry::Count; instanceIndex++)
            {
                rootArgs.materialCb = m_triangleMaterialCB[instanceIndex+1];
                rootArgs.triangleCB.instanceIndex = instanceIndex+1;
                auto ib = m_indexBuffer[instanceIndex+1].gpuDescriptorHandle;
                auto vb = m_vertexBuffer[instanceIndex+1].gpuDescriptorHandle;
                auto texture = m_templeTextures[0].gpuDescriptorHandle;
                memcpy(&rootArgs.indexBufferGPUHandle, &ib, sizeof(ib));
                memcpy(&rootArgs.vertexBufferGPUHandle, &vb, sizeof(ib));
                memcpy(&rootArgs.diffuseTextureGPUHandle, &texture, sizeof(ib));

                // Ray types
                for (UINT r = 0; r < RayType::Count; r++)
                {
                    auto& hitGroupShaderID = hitGroupShaderIDs_TriangleGeometry[r];
                    hitGroupShaderTable.push_back(ShaderRecord(hitGroupShaderID, shaderIDSize, &rootArgs, sizeof(rootArgs)));
                }
            }
        }

        // Skull
        {
            LocalRootSignature::Triangle::RootArguments rootArgs;

            // Create a shader record for each primitive.
            for (UINT instanceIndex = 0; instanceIndex < SkullGeometry::Count; instanceIndex++)
            {
                rootArgs.materialCb = m_triangleMaterialCB[instanceIndex+2];
                rootArgs.triangleCB.instanceIndex = instanceIndex+2;
                auto ib = m_indexBuffer[instanceIndex+2].gpuDescriptorHandle;
                auto vb = m_vertexBuffer[instanceIndex+2].gpuDescriptorHandle;
                auto texture = m_templeTextures[0].gpuDescriptorHandle;
                memcpy(&rootArgs.indexBufferGPUHandle, &ib, sizeof(ib));
                memcpy(&rootArgs.vertexBufferGPUHandle, &vb, sizeof(ib));
                memcpy(&rootArgs.diffuseTextureGPUHandle, &texture, sizeof(ib));

                // Ray types
                for (UINT r = 0; r < RayType::Count; r++)
                {
                    auto& hitGroupShaderID = hitGroupShaderIDs_TriangleGeometry[r];
                    hitGroupShaderTable.push_back(ShaderRecord(hitGroupShaderID, shaderIDSize, &rootArgs, sizeof(rootArgs)));
                }
            }
        }

        // Table
        {
            LocalRootSignature::Triangle::RootArguments rootArgs;

            // Create a shader record for each primitive.
            for (UINT instanceIndex = 0; instanceIndex < TableGeometry::Count; instanceIndex++)
            {
                rootArgs.materialCb = m_triangleMaterialCB[instanceIndex+3];
                rootArgs.triangleCB.instanceIndex = instanceIndex+3;
                auto ib = m_indexBuffer[instanceIndex+3].gpuDescriptorHandle;
                auto vb = m_vertexBuffer[instanceIndex+3].gpuDescriptorHandle;
                auto texture = m_templeTextures[0].gpuDescriptorHandle;
                memcpy(&rootArgs.indexBufferGPUHandle, &ib, sizeof(ib));
                memcpy(&rootArgs.vertexBufferGPUHandle, &vb, sizeof(ib));
                memcpy(&rootArgs.diffuseTextureGPUHandle, &texture, sizeof(ib));

                // Ray types
                for (UINT r = 0; r < RayType::Count; r++)
                {
                    auto& hitGroupShaderID = hitGroupShaderIDs_TriangleGeometry[r];
                    hitGroupShaderTable.push_back(ShaderRecord(hitGroupShaderID, shaderIDSize, &rootArgs, sizeof(rootArgs)));
                }
            }
        }

        // Lamp
        {
            LocalRootSignature::Triangle::RootArguments rootArgs;

            // Create a shader record for each primitive.
            for (UINT instanceIndex = 0; instanceIndex < 4; instanceIndex++)
            {
                rootArgs.materialCb = m_triangleMaterialCB[instanceIndex+7];
                rootArgs.triangleCB.instanceIndex = instanceIndex+7;
                auto ib = m_indexBuffer[instanceIndex+7].gpuDescriptorHandle;
                auto vb = m_vertexBuffer[instanceIndex+7].gpuDescriptorHandle;
                auto texture = m_templeTextures[0].gpuDescriptorHandle;
                memcpy(&rootArgs.indexBufferGPUHandle, &ib, sizeof(ib));
                memcpy(&rootArgs.vertexBufferGPUHandle, &vb, sizeof(ib));
                memcpy(&rootArgs.diffuseTextureGPUHandle, &texture, sizeof(ib));

                // Ray types
                for (UINT r = 0; r < RayType::Count; r++)
                {
                    auto& hitGroupShaderID = hitGroupShaderIDs_TriangleGeometry[r];
                    hitGroupShaderTable.push_back(ShaderRecord(hitGroupShaderID, shaderIDSize, &rootArgs, sizeof(rootArgs)));
                }
            }
        }

        // SunTemple
        {
            LocalRootSignature::Triangle::RootArguments rootArgs;

            // Create a shader record for each primitive.
            for (UINT instanceIndex = 0; instanceIndex < 1056; instanceIndex++)
            {
                rootArgs.materialCb = m_triangleMaterialCB[instanceIndex + 11];
                rootArgs.triangleCB.instanceIndex = instanceIndex + 11;
                auto ib = m_indexBuffer[instanceIndex + 11].gpuDescriptorHandle;
                auto vb = m_vertexBuffer[instanceIndex + 11].gpuDescriptorHandle;

                string geoName = "geo" + to_string(instanceIndex + 11);
                string meshName = "mesh" + to_string(instanceIndex + 11);

                Material m = m_geometries[geoName]->DrawArgs[meshName].Material;
                auto texture = m_templeTextures[m.id].gpuDescriptorHandle;



                memcpy(&rootArgs.indexBufferGPUHandle, &ib, sizeof(ib));
                memcpy(&rootArgs.vertexBufferGPUHandle, &vb, sizeof(ib));
                memcpy(&rootArgs.diffuseTextureGPUHandle, &texture, sizeof(ib));

                // Ray types
                for (UINT r = 0; r < RayType::Count; r++)
                {
                    auto& hitGroupShaderID = hitGroupShaderIDs_TriangleGeometry[r];
                    hitGroupShaderTable.push_back(ShaderRecord(hitGroupShaderID, shaderIDSize, &rootArgs, sizeof(rootArgs)));
                }
            }
        }
      
        hitGroupShaderTable.DebugPrint(shaderIdToStringMap);
        m_hitGroupShaderTableStrideInBytes = hitGroupShaderTable.GetShaderRecordSize();
        m_hitGroupShaderTable = hitGroupShaderTable.GetResource();
    }
}

void Core::DoRaytracing()
{
    auto commandList = m_deviceResources->GetCommandList();
    auto frameIndex = m_deviceResources->GetCurrentFrameIndex();

    auto DispatchRays = [&](auto* raytracingCommandList, auto* stateObject, auto* dispatchDesc)
    {
        dispatchDesc->HitGroupTable.StartAddress = m_hitGroupShaderTable->GetGPUVirtualAddress();
        dispatchDesc->HitGroupTable.SizeInBytes = m_hitGroupShaderTable->GetDesc().Width;
        dispatchDesc->HitGroupTable.StrideInBytes = m_hitGroupShaderTableStrideInBytes;
        dispatchDesc->MissShaderTable.StartAddress = m_missShaderTable->GetGPUVirtualAddress();
        dispatchDesc->MissShaderTable.SizeInBytes = m_missShaderTable->GetDesc().Width;
        dispatchDesc->MissShaderTable.StrideInBytes = m_missShaderTableStrideInBytes;
        dispatchDesc->RayGenerationShaderRecord.StartAddress = m_rayGenShaderTable->GetGPUVirtualAddress();
        dispatchDesc->RayGenerationShaderRecord.SizeInBytes = m_rayGenShaderTable->GetDesc().Width;
        dispatchDesc->Width = m_width;
        dispatchDesc->Height = m_height;
        dispatchDesc->Depth = 1;
        raytracingCommandList->SetPipelineState1(stateObject);

        m_gpuTimers[GpuTimers::Raytracing].Start(commandList);
        raytracingCommandList->DispatchRays(dispatchDesc);
        m_gpuTimers[GpuTimers::Raytracing].Stop(commandList);
    };

    auto SetCommonPipelineState = [&](auto* descriptorSetCommandList)
    {
        descriptorSetCommandList->SetDescriptorHeaps(1, m_descriptorHeap->GetAddressOf());

        // Set index and successive vertex buffer decriptor tables.
        commandList->SetComputeRootDescriptorTable(GlobalRootSignature::Slot::OutputView, m_raytracingOutputResourceUAVGpuDescriptor);
        commandList->SetComputeRootDescriptorTable(GlobalRootSignature::Slot::ReflectionBuffer, m_reflectionBufferResourceUAVGpuDescriptor);
        commandList->SetComputeRootDescriptorTable(GlobalRootSignature::Slot::ShadowBuffer, m_shadowBufferResourceUAVGpuDescriptor);
    };

    commandList->SetComputeRootSignature(m_raytracingGlobalRootSignature.Get());

    // Copy dynamic buffers to GPU.
    {
        m_sceneCB.CopyStagingToGpu(frameIndex);
        commandList->SetComputeRootConstantBufferView(GlobalRootSignature::Slot::SceneConstant, m_sceneCB.GpuVirtualAddress(frameIndex));

        m_trianglePrimitiveAttributeBuffer.CopyStagingToGpu(frameIndex);
        commandList->SetComputeRootShaderResourceView(GlobalRootSignature::Slot::TriangleAttributeBuffer, m_trianglePrimitiveAttributeBuffer.GpuVirtualAddress(frameIndex));
    }

    // Bind the heaps, acceleration structure and dispatch rays.  
    D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
    SetCommonPipelineState(commandList);
    commandList->SetComputeRootShaderResourceView(GlobalRootSignature::Slot::AccelerationStructure, m_topLevelAS->GetGPUVirtualAddress());
    DispatchRays(m_dxrCommandList.Get(), m_dxrStateObject.Get(), &dispatchDesc);
}

Core::Core(UINT width, UINT height, std::wstring name) :
    DXCore(width, height, name),
    m_raytracingOutputResourceUAVDescriptorHeapIndex(UINT_MAX),
    m_reflectionBufferResourceUAVDescriptorHeapIndex(UINT_MAX),
    m_shadowBufferResourceUAVDescriptorHeapIndex(UINT_MAX),
    m_animateGeometryTime(0.0f),
    m_animateCamera(false),
    m_animateGeometry(true),
    m_animateLight(false),
    m_descriptorsAllocated(0),
    m_descriptorSize(0),
    m_missShaderTableStrideInBytes(UINT_MAX),
    m_hitGroupShaderTableStrideInBytes(UINT_MAX)
{
    UpdateForSizeChange(width, height);
}

void Core::OnKeyDown(UINT8 key)
{
    // rotation
    float elapsedTime = static_cast<float>(m_timer.GetElapsedSeconds());
    float secondsToRotateAround = 0.1f;
    float angleToRotateBy = -360.0f * (elapsedTime / secondsToRotateAround);
    const XMVECTOR& prevLightPosition = m_sceneCB->lightPosition;
    XMMATRIX rotate = XMMatrixRotationY(XMConvertToRadians(angleToRotateBy));
    XMMATRIX rotateClockwise = XMMatrixRotationY(XMConvertToRadians(-angleToRotateBy));


    auto speed = 100.0f;
    if (GetKeyState(VK_SHIFT))
        speed *= 5;
    switch (key)
    {
    case 'W':
        m_camera.Walk(speed * elapsedTime);
        break;
    case 'S':
        m_camera.Walk(-speed * elapsedTime);
        break;
    case 'A':
        m_camera.Strafe(-speed * elapsedTime);
        break;
    case 'D':
        m_camera.Strafe(speed * elapsedTime);
        break;
    case 'Q':
        m_sceneCB->lightPosition = XMVector3Transform(prevLightPosition, rotate);
        break;
    case 'E':
        m_sceneCB->lightPosition = XMVector3Transform(prevLightPosition, rotateClockwise);
        break;
    case '1':
        XMFLOAT4 equal;
        XMStoreFloat4(&equal, XMVectorEqual(m_sceneCB->lightPosition, XMVECTOR{ 0.0f, 0.0f, 0.0f }));
        equal.x ? m_sceneCB->lightPosition = XMVECTOR{ 0.0f, 18.0f, -20.0f, 0.0f } : m_sceneCB->lightPosition = XMVECTOR{ 0.0f, 0.0f, 0.0f, 0.0f };
        break;
    case 'I':
        m_sceneCB->lightPosition += XMVECTOR{ 0.0f, 0.0f, 100.0f } *elapsedTime;
        break;
    case 'J':
        m_sceneCB->lightPosition += XMVECTOR{ -100.0f, 0.0f, 0.0f } *elapsedTime;
        break;
    case 'K':
        m_sceneCB->lightPosition += XMVECTOR{ 0.0f, 0.0f, -100.0f } *elapsedTime;
        break;
    case 'L':
        m_sceneCB->lightPosition += XMVECTOR{ 100.0f, 0.0f, 0.0f } *elapsedTime;
        break;
    case 'U':
        m_sceneCB->lightPosition += XMVECTOR{ 0.0f, -100.0f, 0.0f } *elapsedTime;
        break;
    case 'O':
        m_sceneCB->lightPosition += XMVECTOR{ 0.0f, 100.0f, 0.0f } *elapsedTime;
        break;
    case '2':
        m_orbitalCamera = !m_orbitalCamera;
        break;
    }
    m_camera.UpdateViewMatrix();
    UpdateCameraMatrices();
}

void Core::OnMouseMove(int x, int y)
{
    float dx = XMConvertToRadians(0.25f * static_cast<float>(x - m_lastMousePosition.x));
    float dy = XMConvertToRadians(0.25f * static_cast<float>(y - m_lastMousePosition.y));

    m_camera.RotateY(dx);
    m_camera.Pitch(dy);


    m_camera.UpdateViewMatrix();
    UpdateCameraMatrices();

    m_lastMousePosition.x = x;
    m_lastMousePosition.y = y;
}

void Core::OnLeftButtonDown(UINT x, UINT y)
{
    m_lastMousePosition = { static_cast<int>(x), static_cast<int>(y) };
}

void Core::OnInit()
{
    m_deviceResources = std::make_unique<DeviceResources>(
        DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_FORMAT_UNKNOWN,
        FrameCount,
        D3D_FEATURE_LEVEL_11_0,
        // Sample shows handling of use cases with tearing support, which is OS dependent and has been supported since TH2.
        // Since the sample requires build 1809 (RS5) or higher, we don't need to handle non-tearing cases.
        DeviceResources::c_RequireTearingSupport,
        m_adapterIDoverride
        );
    m_deviceResources->RegisterDeviceNotify(this);
    m_deviceResources->SetWindow(Win32Core::GetHwnd(), m_width, m_height);
    m_deviceResources->InitializeDXGIAdapter();

    ThrowIfFalse(IsDirectXRaytracingSupported(m_deviceResources->GetAdapter()),
        L"ERROR: DirectX Raytracing is not supported by your OS, GPU and/or driver.\n\n");

    m_deviceResources->CreateDeviceResources();
    m_deviceResources->CreateWindowSizeDependentResources();

    InitializeScene();
    CreateDeviceDependentResources();
    
    CreateWindowSizeDependentResources();
}

void Core::UpdateCameraMatrices()
{
    auto frameIndex = m_deviceResources->GetCurrentFrameIndex();

    if (m_orbitalCamera) {
        m_sceneCB->cameraPosition = m_eye;

        float fovAngleY = 45.0f;

        XMMATRIX view = XMMatrixLookAtLH(m_eye, m_at, m_up);
        XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(fovAngleY), m_aspectRatio, 0.01f, 125.0f);
        XMMATRIX viewProj = view * proj;
        m_sceneCB->projectionToWorld = XMMatrixInverse(nullptr, viewProj);
    } else {
        m_sceneCB->cameraPosition = m_camera.GetPosition();
        float fovAngleY = 45.0f;
        XMMATRIX view = m_camera.GetView();
        XMMATRIX proj = m_camera.GetProj();
        XMMATRIX viewProj = view * proj;
        m_sceneCB->projectionToWorld = XMMatrixInverse(nullptr, XMMatrixMultiply(m_camera.GetView(), m_camera.GetProj()));
    }

}

void Core::InitializeScene()
{
    auto frameIndex = m_deviceResources->GetCurrentFrameIndex();

    // Setup materials.
    {
        auto SetAttributes2 = [&](
            UINT primitiveIndex,
            const XMFLOAT4& albedo,
            float metalness = 0.7f,
            float roughness = 0.0f,
            float stepScale = 1.0f,
            float shaded = 1.0f)
        {
            auto& attributes = m_triangleMaterialCB[primitiveIndex];
            attributes.albedo = albedo;
            attributes.reflectanceCoef = metalness;
            attributes.diffuseCoef = 1-metalness;
            attributes.metalness = metalness;
            attributes.roughness = roughness;
            attributes.stepScale = stepScale;
            attributes.shaded = shaded;
        };

        // Albedos
        XMFLOAT4 green = XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f);
        XMFLOAT4 red = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);
        XMFLOAT4 yellow = XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f);
        XMFLOAT4 white = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);


        SetAttributes2(TriangleGeometry::Room, XMFLOAT4(1.000f, 0.766f, 0.336f, 1.000f), 0.3f, 0.1f, 1.0f, 1.0f);
        SetAttributes2(CoordinateGeometry::Coordinates+1, red, 0.7, 1.0, 1.0f, 0.0f);
        SetAttributes2(SkullGeometry::Skull+2, XMFLOAT4(1.000f, 0.8f, 0.836f, 1.000f), 0.3f, 0.5f, 1.0f, 1.0f);

        // Table
        for (int i = 0; i < 4; i++)
        {
            SetAttributes2(i+ 3, XMFLOAT4(1.000f, 0.0f, 0.0f, 1.000f), 0.8f, 0.01f, 1.0f, 1.0f);
        }

        // Lamp
        for (int i = 0; i < 4; i++)
        {
            SetAttributes2(i + 7, XMFLOAT4(0.000f, 0.5f, 0.836f, 1.000f), 0.8f, 0.5f, 1.0f, 1.0f);
        }
    }

    // Setup camera.
    {
        m_camera.SetPosition(0.0f, 2.0f, -15.0f);
        m_eye = { 0.0f, 1.6f, -10.0f, 1.0f };
        m_at = { 0.0f, 0.0f, 0.0f, 1.0f };  
        XMVECTOR right = { 1.0f, 0.0f, 0.0f, 0.0f };

        XMVECTOR direction = XMVector4Normalize(m_at - m_eye);
        m_up = XMVector3Normalize(XMVector3Cross(direction, right));

        XMMATRIX rotate = XMMatrixRotationY(XMConvertToRadians(45.0f));
        m_eye = XMVector3Transform(m_eye, rotate);
        m_up = XMVector3Transform(m_up, rotate);
        m_camera.UpdateViewMatrix();
        UpdateCameraMatrices();
    }

    // Setup lights.
    {
        XMFLOAT4 lightPosition;
        XMFLOAT4 lightAmbientColor;
        XMFLOAT4 lightDiffuseColor;

        lightPosition = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
        m_sceneCB->lightPosition = XMLoadFloat4(&lightPosition);

        lightAmbientColor = XMFLOAT4(0.25f, 0.25f, 0.25f, 1.0f);
        m_sceneCB->lightAmbientColor = XMLoadFloat4(&lightAmbientColor);

        float d = 0.6f;
        lightDiffuseColor = XMFLOAT4(d, d, d, 1.0f);
        m_sceneCB->lightDiffuseColor = XMLoadFloat4(&lightDiffuseColor);
    }
}

void Core::CreateConstantBuffers()
{
    auto device = m_deviceResources->GetD3DDevice();
    auto frameCount = m_deviceResources->GetBackBufferCount();

    m_sceneCB.Create(device, frameCount, L"Scene Constant Buffer");
}

void Core::CreateTrianglePrimitiveAttributesBuffers()
{
    auto device = m_deviceResources->GetD3DDevice();
    auto frameCount = m_deviceResources->GetBackBufferCount();
    m_trianglePrimitiveAttributeBuffer.Create(device, NUM_BLAS, frameCount, L"Triangle primitive attributes");
}

void Core::CreateDeviceDependentResources() {
    CreateAuxilaryDeviceResources();
    CreateRaytracingInterfaces();
    CreateRootSignatures();
    CreateRaytracingPipelineStateObject();
    CreateDescriptorHeap();
    BuildGeometry();

    auto SetAttributes2 = [&](
        UINT primitiveIndex,
        const XMFLOAT4& albedo,
        float metalness = 0.7f,
        float roughness = 0.0f,
        float stepScale = 1.0f,
        float shaded = 1.0f)
    {
        auto& attributes = m_triangleMaterialCB[primitiveIndex];
        attributes.albedo = albedo;
        attributes.reflectanceCoef = metalness;
        attributes.diffuseCoef = 1 - metalness;
        attributes.metalness = metalness;
        attributes.roughness = roughness;
        attributes.stepScale = stepScale;
        attributes.shaded = shaded;
    };

    for (int i = 0; i < 1056; i++) {
         string geoName = "geo" + to_string(i + 11);
         string meshName = "mesh" + to_string(i + 11);
         Material m = m_geometries[geoName]->DrawArgs[meshName].Material;
         SetAttributes2(i + 11, XMFLOAT4(m.Kd.x, m.Kd.y, m.Kd.z, 1.0f), m.Ks.x, m.Ns, 1.0f, 1.0f);
    }

    BuildAccelerationStructures();
    CreateConstantBuffers();
    CreateTrianglePrimitiveAttributesBuffers();
    BuildShaderTables();
    CreateRaytracingOutputResource();
}

void Core::SerializeAndCreateRaytracingRootSignature(ID3D12Device5* device, D3D12_ROOT_SIGNATURE_DESC& desc, ComPtr<ID3D12RootSignature>* rootSig, LPCWSTR resourceName = nullptr) {
    ComPtr<ID3DBlob> blob;
    ComPtr<ID3DBlob> error;

    ThrowIfFailed(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error), error ? static_cast<wchar_t*>(error->GetBufferPointer()) : nullptr);
    ThrowIfFailed(device->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&(*rootSig))));

    if (resourceName)
    {
        (*rootSig)->SetName(resourceName);
    }
}

void Core::CreateRaytracingInterfaces()
{
    auto device = m_deviceResources->GetD3DDevice();
    auto commandList = m_deviceResources->GetCommandList();

    ThrowIfFailed(device->QueryInterface(IID_PPV_ARGS(&m_dxrDevice)), L"Couldn't get DirectX Raytracing interface for the device.\n");
    ThrowIfFailed(commandList->QueryInterface(IID_PPV_ARGS(&m_dxrCommandList)), L"Couldn't get DirectX Raytracing interface for the command list.\n");
}

void Core::CreateDxilLibrarySubobjects(CD3DX12_STATE_OBJECT_DESC* raytracingPipeline)
{
    const unsigned char* compiledShaderByteCode[] = { g_pViewRG, g_pRadianceCH, g_pRadianceMS, g_pShadowMS };
    const unsigned int compiledShaderByteCodeSizes[] = { ARRAYSIZE(g_pViewRG), ARRAYSIZE(g_pRadianceCH), ARRAYSIZE(g_pRadianceMS), ARRAYSIZE(g_pShadowMS) };

    for (auto i : indices(compiledShaderByteCode)) {
        auto lib = raytracingPipeline->CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
        auto libDXIL = CD3DX12_SHADER_BYTECODE((void*)compiledShaderByteCode[i], compiledShaderByteCodeSizes[i]);
        lib->SetDXILLibrary(&libDXIL);
    };

    // Use default shader exports for a DXIL library/collection subobject ~ surface all shaders.
}

void Core::CreateHitGroupSubobjects(CD3DX12_STATE_OBJECT_DESC* raytracingPipeline)
{
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

void Core::CreateAuxilaryDeviceResources()
{
    auto device = m_deviceResources->GetD3DDevice();
    auto commandQueue = m_deviceResources->GetCommandQueue();

    for (auto& gpuTimer : m_gpuTimers)
    {
        gpuTimer.RestoreDevice(device, commandQueue, FrameCount);
    }
}

void Core::CreateRaytracingOutputResource()
{
    auto device = m_deviceResources->GetD3DDevice();
    auto backbufferFormat = m_deviceResources->GetBackBufferFormat();

    // Create the output resource. The dimensions and format should match the swap-chain.
    auto uavDesc = CD3DX12_RESOURCE_DESC::Tex2D(backbufferFormat, m_width, m_height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    auto defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(device->CreateCommittedResource(
        &defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &uavDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&m_raytracingOutput)));
    NAME_D3D12_OBJECT(m_raytracingOutput);

    D3D12_CPU_DESCRIPTOR_HANDLE uavDescriptorHandle;
    m_raytracingOutputResourceUAVDescriptorHeapIndex = AllocateDescriptor(&uavDescriptorHandle, m_raytracingOutputResourceUAVDescriptorHeapIndex);
    D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
    UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    device->CreateUnorderedAccessView(m_raytracingOutput.Get(), nullptr, &UAVDesc, uavDescriptorHandle);
    m_raytracingOutputResourceUAVGpuDescriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_descriptorHeap->GetGPUDescriptorHandleForHeapStart(), m_raytracingOutputResourceUAVDescriptorHeapIndex, m_descriptorSize);

    // Reflection buffer
    ThrowIfFailed(device->CreateCommittedResource(
        &defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &uavDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&m_reflectionBuffer)));
    NAME_D3D12_OBJECT(m_reflectionBuffer);

    D3D12_CPU_DESCRIPTOR_HANDLE uavDescriptorHandle2;
    m_reflectionBufferResourceUAVDescriptorHeapIndex = AllocateDescriptor(&uavDescriptorHandle2, m_reflectionBufferResourceUAVDescriptorHeapIndex);
    D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc2 = {};
    UAVDesc2.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    device->CreateUnorderedAccessView(m_reflectionBuffer.Get(), nullptr, &UAVDesc2, uavDescriptorHandle2);
    m_reflectionBufferResourceUAVGpuDescriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_descriptorHeap->GetGPUDescriptorHandleForHeapStart(), m_reflectionBufferResourceUAVDescriptorHeapIndex, m_descriptorSize);

    // Shadow buffer
    ThrowIfFailed(device->CreateCommittedResource(
        &defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &uavDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&m_shadowBuffer)));
    NAME_D3D12_OBJECT(m_shadowBuffer);

    D3D12_CPU_DESCRIPTOR_HANDLE uavDescriptorHandle3;
    m_shadowBufferResourceUAVDescriptorHeapIndex = AllocateDescriptor(&uavDescriptorHandle3, m_shadowBufferResourceUAVDescriptorHeapIndex);
    D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc3 = {};
    UAVDesc3.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    device->CreateUnorderedAccessView(m_shadowBuffer.Get(), nullptr, &UAVDesc3, uavDescriptorHandle3);
    m_shadowBufferResourceUAVGpuDescriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_descriptorHeap->GetGPUDescriptorHandleForHeapStart(), m_shadowBufferResourceUAVDescriptorHeapIndex, m_descriptorSize);
}

void Core::UpdateForSizeChange(UINT width, UINT height)
{
    DXCore::UpdateForSizeChange(width, height);
}

void Core::CopyRaytracingOutputToBackbuffer()
{
    auto commandList = m_deviceResources->GetCommandList();
    auto renderTarget = m_deviceResources->GetRenderTarget();

    D3D12_RESOURCE_BARRIER preCopyBarriers[2];
    preCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(renderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);
    preCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(m_raytracingOutput.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
    commandList->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);

    commandList->CopyResource(renderTarget, m_raytracingOutput.Get());

    D3D12_RESOURCE_BARRIER postCopyBarriers[2];
    postCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(renderTarget, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);
    postCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(m_raytracingOutput.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    commandList->ResourceBarrier(ARRAYSIZE(postCopyBarriers), postCopyBarriers);
}

void Core::CreateWindowSizeDependentResources()
{
    CreateRaytracingOutputResource();
    UpdateCameraMatrices();
}

void Core::ReleaseWindowSizeDependentResources()
{
    m_raytracingOutput.Reset();
}

void Core::ReleaseDeviceDependentResources()
{
    for (auto& gpuTimer : m_gpuTimers)
    {
        gpuTimer.ReleaseDevice();
    }

    m_raytracingGlobalRootSignature.Reset();
    ResetComPtrArray(&m_raytracingLocalRootSignature);

    m_dxrDevice.Reset();
    m_dxrCommandList.Reset();
    m_dxrStateObject.Reset();

    m_raytracingGlobalRootSignature.Reset();
    ResetComPtrArray(&m_raytracingLocalRootSignature);

    m_descriptorHeap.reset();
    m_descriptorsAllocated = 0;
    m_sceneCB.Release();

    m_trianglePrimitiveAttributeBuffer.Release();
    for (int i = 0; i < TriangleGeometry::Count; i++)
    {
        m_indexBuffer[i].resource.Reset();
        m_vertexBuffer[i].resource.Reset();
    }

    ResetComPtrArray(&m_bottomLevelAS);
    m_topLevelAS.Reset();

    m_raytracingOutput.Reset();
    m_raytracingOutputResourceUAVDescriptorHeapIndex = UINT_MAX;
    m_rayGenShaderTable.Reset();
    m_missShaderTable.Reset();
    m_hitGroupShaderTable.Reset();
}

void Core::RecreateD3D()
{
    // Give GPU a chance to finish its execution in progress.
    try
    {
        m_deviceResources->WaitForGpu();
    }
    catch (HrException&)
    {
        // Do nothing, currently attached adapter is unresponsive.
    }
    m_deviceResources->HandleDeviceLost();
}

void Core::Compose()
{
    auto commandList = m_deviceResources->GetCommandList();
    auto device = m_deviceResources->GetD3DDevice();

    CD3DX12_DESCRIPTOR_RANGE ranges[3];
    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
    ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);

    CD3DX12_ROOT_PARAMETER rootParameters[3];
    rootParameters[0].InitAsDescriptorTable(1, &ranges[0]);
    rootParameters[1].InitAsDescriptorTable(1, &ranges[1]);
    rootParameters[2].InitAsDescriptorTable(1, &ranges[2]);

    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters);
    SerializeAndCreateRaytracingRootSignature(device, rootSignatureDesc, &m_composeRootSig, L"Root signature: CompositionCS");

    // create pso
    D3D12_COMPUTE_PIPELINE_STATE_DESC descComputePSO = {};
    descComputePSO.pRootSignature = m_composeRootSig.Get();
    descComputePSO.CS = CD3DX12_SHADER_BYTECODE((void*)g_pCompositionCS, ARRAYSIZE(g_pCompositionCS));

    ThrowIfFailed(device->CreateComputePipelineState(&descComputePSO, IID_PPV_ARGS(&m_composePSO[0])));
    m_composePSO[0]->SetName(L"PSO: CompositionCS");

    commandList->SetDescriptorHeaps(1, m_descriptorHeap->GetAddressOf());
    commandList->SetComputeRootSignature(m_composeRootSig.Get());
    commandList->SetPipelineState(m_composePSO[0].Get());
    m_composePSO[0]->AddRef();

    commandList->SetComputeRootDescriptorTable(0, m_raytracingOutputResourceUAVGpuDescriptor); // Input/Output
    commandList->SetComputeRootDescriptorTable(1, m_reflectionBufferResourceUAVGpuDescriptor); // Input
    commandList->SetComputeRootDescriptorTable(2, m_shadowBufferResourceUAVGpuDescriptor); // Input

    commandList->Dispatch(1280/8, 720/8, 1);
}

void Core::OnRender()
{
    if (!m_deviceResources->IsWindowVisible())
    {
        return;
    }

    auto device = m_deviceResources->GetD3DDevice();
    auto commandList = m_deviceResources->GetCommandList();

    // Begin frame.
    m_deviceResources->Prepare();
    for (auto& gpuTimer : m_gpuTimers)
    {
        gpuTimer.BeginFrame(commandList);
    }

    DoRaytracing();
    Compose();
    CopyRaytracingOutputToBackbuffer();

    // End frame.
    for (auto& gpuTimer : m_gpuTimers)
    {
        gpuTimer.EndFrame(commandList);
    }

    m_deviceResources->Present(D3D12_RESOURCE_STATE_PRESENT);
}

void Core::OnDestroy() {
    // Let GPU finish before releasing D3D resources.
    m_deviceResources->WaitForGpu();
    OnDeviceLost();
}

void Core::OnDeviceLost()
{
    ReleaseWindowSizeDependentResources();
    ReleaseDeviceDependentResources();
}

void Core::OnDeviceRestored()
{
    CreateDeviceDependentResources();
    CreateWindowSizeDependentResources();
}

void Core::CalculateFrameStats()
{
    static int frameCnt = 0;
    static double prevTime = 0.0f;
    double totalTime = m_timer.GetTotalSeconds();

    frameCnt++;

    // Compute averages over one second period.
    if ((totalTime - prevTime) >= 1.0f)
    {
        float diff = static_cast<float>(totalTime - prevTime);
        float fps = static_cast<float>(frameCnt) / diff; // Normalize to an exact second.

        frameCnt = 0;
        prevTime = totalTime;
        float raytracingTime = static_cast<float>(m_gpuTimers[GpuTimers::Raytracing].GetElapsedMS());
        float MRaysPerSecond = NumMRaysPerSecond(m_width, m_height, raytracingTime);

        wstringstream windowText;
        windowText << setprecision(2) << fixed
            << L"    FPS: " << fps
            << L"    Raytracing time: " << raytracingTime << " ms"
            << L"    Ray throughput: " << MRaysPerSecond << " MRPS"
            << L"    GPU[" << m_deviceResources->GetAdapterID() << L"]: " << m_deviceResources->GetAdapterDescription();
        SetCustomWindowText(windowText.str().c_str());
    }
}

void Core::OnSizeChanged(UINT width, UINT height, bool minimized)
{
    if (!m_deviceResources->WindowSizeChanged(width, height, minimized))
    {
        return;
    }

    UpdateForSizeChange(width, height);

    ReleaseWindowSizeDependentResources();
    CreateWindowSizeDependentResources();
}

UINT Core::AllocateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* cpuDescriptor, UINT descriptorIndexToUse)
{
    auto descriptorHeapCpuBase = m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    if (descriptorIndexToUse >= m_descriptorHeap->GetDesc().NumDescriptors)
    {
        ThrowIfFalse(m_descriptorsAllocated < m_descriptorHeap->GetDesc().NumDescriptors, L"Ran out of descriptors on the heap!");
        descriptorIndexToUse = m_descriptorsAllocated++;
    }
    *cpuDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(descriptorHeapCpuBase, descriptorIndexToUse, m_descriptorSize);
    return descriptorIndexToUse;
}

UINT Core::CreateBufferSRV(D3DBuffer* buffer, UINT numElements, UINT elementSize)
{
    auto device = m_deviceResources->GetD3DDevice();

    // SRV
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.NumElements = numElements;
    if (elementSize == 0)
    {
        srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
        srvDesc.Buffer.StructureByteStride = 0;
    }
    else
    {
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        srvDesc.Buffer.StructureByteStride = elementSize;
    }
    UINT descriptorIndex = AllocateDescriptor(&buffer->cpuDescriptorHandle);
    device->CreateShaderResourceView(buffer->resource.Get(), &srvDesc, buffer->cpuDescriptorHandle);
    buffer->gpuDescriptorHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_descriptorHeap->GetGPUDescriptorHandleForHeapStart(), descriptorIndex, m_descriptorSize);
    return descriptorIndex;
}

UINT Core::CreateTextureSRV(UINT numElements, UINT elementSize)
{
    auto device = m_deviceResources->GetD3DDevice();

    auto srvDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, 512, 512, 1, 1, 1, 0);

    auto stoneTex = m_textures["stoneTex"]->Resource;

    auto defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(device->CreateCommittedResource(
        &defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &srvDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&stoneTex)));

    D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
    SRVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    SRVDesc.Texture2D.MostDetailedMip = 0;
    SRVDesc.Texture2D.MipLevels = 1;
    SRVDesc.Texture2D.PlaneSlice = 0;
    SRVDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());

    UINT descriptorIndex = AllocateDescriptor(&hDescriptor);
    device->CreateShaderResourceView(stoneTex.Get(), &SRVDesc, hDescriptor);
    //texture->gpuDescriptorHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_descriptorHeap->GetGPUDescriptorHandleForHeapStart(), descriptorIndex, m_descriptorSize);
    return descriptorIndex;
}

void Core::OnUpdate()
{
    m_timer.Tick();
    CalculateFrameStats();
    float elapsedTime = static_cast<float>(m_timer.GetElapsedSeconds());
    auto frameIndex = m_deviceResources->GetCurrentFrameIndex();
    auto prevFrameIndex = m_deviceResources->GetPreviousFrameIndex();

    if (m_orbitalCamera)
    {
        float secondsToRotateAround = 48.0f;
        float angleToRotateBy = 360.0f * (elapsedTime / secondsToRotateAround);
        XMMATRIX rotate = XMMatrixRotationY(XMConvertToRadians(angleToRotateBy));
        m_eye = XMVector3Transform(m_eye, rotate);
        m_up = XMVector3Transform(m_up, rotate);
        m_at = XMVector3Transform(m_at, rotate);
        UpdateCameraMatrices();
    }

    // Rotate the second light around Y axis.
    if (m_animateLight)
    {
        float secondsToRotateAround = 8.0f;
        float angleToRotateBy = -360.0f * (elapsedTime / secondsToRotateAround);
        XMMATRIX rotate = XMMatrixRotationY(XMConvertToRadians(angleToRotateBy));
        const XMVECTOR& prevLightPosition = m_sceneCB->lightPosition;
        m_sceneCB->lightPosition = XMVector3Transform(prevLightPosition, rotate);
    }

    // Transform the procedural geometry.
    if (m_animateGeometry)
    {
        m_animateGeometryTime += elapsedTime;
    }
    m_sceneCB->elapsedTime = m_animateGeometryTime;
}

void Core::BuildAccelerationStructures()
{
    auto device = m_deviceResources->GetD3DDevice();
    auto commandList = m_deviceResources->GetCommandList();
    auto commandQueue = m_deviceResources->GetCommandQueue();
    auto commandAllocator = m_deviceResources->GetCommandAllocator();

    // Reset the command list for the acceleration structure construction.
    commandList->Reset(commandAllocator, nullptr);

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

        std::size_t pos = add.find("\\");
        std::string str3 = add.substr(pos + 1);
        string path = base + str3;        
        if(add != "")
            LoadDDSTexture(device, commandList, wstring(path.begin(), path.end()).c_str(), m_descriptorHeap.get(), &m_templeTextures[i]);
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

AccelerationStructureBuffers Core::BuildTopLevelAS(AccelerationStructureBuffers bottomLevelAS[BottomLevelASType::Count], D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags)
{
    auto device = m_deviceResources->GetD3DDevice();
    auto commandList = m_deviceResources->GetCommandList();
    ComPtr<ID3D12Resource> scratch;
    ComPtr<ID3D12Resource> topLevelAS;

    // Get required sizes for an acceleration structure.
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelBuildDesc = {};
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& topLevelInputs = topLevelBuildDesc.Inputs;
    topLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    topLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    topLevelInputs.Flags = buildFlags;
    topLevelInputs.NumDescs = NUM_BLAS;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topLevelPrebuildInfo = {};
    m_dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&topLevelInputs, &topLevelPrebuildInfo);
    ThrowIfFalse(topLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

    AllocateUAVBuffer(device, topLevelPrebuildInfo.ScratchDataSizeInBytes, &scratch, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");
    {
        D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
        AllocateUAVBuffer(device, topLevelPrebuildInfo.ResultDataMaxSizeInBytes, &topLevelAS, initialResourceState, L"TopLevelAccelerationStructure");
    }

    // Create instance descs for the bottom-level acceleration structures.
    ComPtr<ID3D12Resource> instanceDescsResource;
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
            bottomLevelAS[6].accelerationStructure->GetGPUVirtualAddress()
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
    m_dxrCommandList->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);

    AccelerationStructureBuffers topLevelASBuffers;
    topLevelASBuffers.accelerationStructure = topLevelAS;
    topLevelASBuffers.instanceDesc = instanceDescsResource;
    topLevelASBuffers.scratch = scratch;
    topLevelASBuffers.ResultDataMaxSizeInBytes = topLevelPrebuildInfo.ResultDataMaxSizeInBytes;
    return topLevelASBuffers;
}

template <class InstanceDescType, class BLASPtrType> void Core::BuildBottomLevelASInstanceDescs(BLASPtrType* bottomLevelASaddresses, ComPtr<ID3D12Resource>* instanceDescsResource)
{
    // Transformations here are moving the whole bottom level acceleration structure in world space.

    auto device = m_deviceResources->GetD3DDevice();

    vector<InstanceDescType> instanceDescs;
    instanceDescs.resize(NUM_BLAS);

    // Width of a bottom-level AS geometry.
    // Make the room a little larger in all dimensions.
    const XMFLOAT3 fWidth = { 1.0f, 1.0f, 1.0f };

    // Bottom-level AS with the room geometry.
    {
        auto& instanceDesc = instanceDescs[BottomLevelASType::Triangle];
        instanceDesc = {};
        instanceDesc.InstanceMask = 1;
        instanceDesc.InstanceContributionToHitGroupIndex = 0;
        instanceDesc.AccelerationStructure = bottomLevelASaddresses[BottomLevelASType::Triangle];

        // Calculate transformation matrix.
        auto position = XMFLOAT3(500.0f, 0.0f, 0.0f);
        const XMVECTOR vBasePosition = XMLoadFloat3(&fWidth) * XMLoadFloat3(&position);

        // Scale in all dimensions.
        XMMATRIX mScale = XMMatrixScaling(fWidth.x, fWidth.y, fWidth.z);
        XMMATRIX mTranslation = XMMatrixTranslationFromVector(vBasePosition);
        XMMATRIX mTransform = mScale * mTranslation;
        XMStoreFloat3x4(reinterpret_cast<XMFLOAT3X4*>(instanceDesc.Transform), mTransform);
    }

    // Coordinate
    {
        auto& instanceDesc = instanceDescs[BottomLevelASType::Coordinates];
        instanceDesc = {};
        instanceDesc.InstanceMask = 1;
        instanceDesc.InstanceContributionToHitGroupIndex = BottomLevelASType::Coordinates * RayType::Count;
        instanceDesc.AccelerationStructure = bottomLevelASaddresses[BottomLevelASType::Coordinates];

        // Calculate transformation matrix.
        auto position = XMFLOAT3(0.0f, 0.0f, 0.0f);
        const XMVECTOR vBasePosition = XMLoadFloat3(&fWidth) * XMLoadFloat3(&position);

        // Scale in all dimensions.
        XMMATRIX mScale = XMMatrixScaling(fWidth.x, fWidth.y, fWidth.z);
        XMMATRIX mTranslation = XMMatrixTranslationFromVector(vBasePosition);
        XMMATRIX mTransform = mScale * mTranslation;
        XMStoreFloat3x4(reinterpret_cast<XMFLOAT3X4*>(instanceDesc.Transform), mTransform);
    }

    // Skull
    {
        auto& instanceDesc = instanceDescs[BottomLevelASType::Skull];
        instanceDesc = {};
        instanceDesc.InstanceMask = 1;
        instanceDesc.InstanceContributionToHitGroupIndex = BottomLevelASType::Skull * RayType::Count;
        instanceDesc.AccelerationStructure = bottomLevelASaddresses[BottomLevelASType::Skull];

        // Calculate transformation matrix.
        auto position = XMFLOAT3(0.5f, 0.6f, -0.5f);
        const XMVECTOR vBasePosition = XMLoadFloat3(&fWidth) * XMLoadFloat3(&position);

        // Scale in all dimensions.
        XMMATRIX mScale = XMMatrixScaling(0.15f, 0.15f, 0.15f);
        XMMATRIX mTranslation = XMMatrixTranslationFromVector(vBasePosition);
        XMMATRIX mRotation = XMMatrixRotationY(XMConvertToRadians(45));
        XMMATRIX mTransform = mScale * mRotation * mTranslation;
        XMStoreFloat3x4(reinterpret_cast<XMFLOAT3X4*>(instanceDesc.Transform), mTransform);
    }

    // Table
    {
        auto& instanceDesc = instanceDescs[BottomLevelASType::Table];
        instanceDesc = {};
        instanceDesc.InstanceMask = 1;
        instanceDesc.InstanceContributionToHitGroupIndex = BottomLevelASType::Table * RayType::Count;
        instanceDesc.AccelerationStructure = bottomLevelASaddresses[BottomLevelASType::Table];

        // Calculate transformation matrix.
        auto position = XMFLOAT3(-2.75f, -3.3f, -4.75f);
        const XMVECTOR vBasePosition = XMLoadFloat3(&fWidth) * XMLoadFloat3(&position);

        // Scale in all dimensions.
        XMMATRIX mScale = XMMatrixScaling(0.05f, 0.05f, 0.05f);
        XMMATRIX mTranslation = XMMatrixTranslationFromVector(vBasePosition);
        XMMATRIX mTransform = mScale * mTranslation;
        XMStoreFloat3x4(reinterpret_cast<XMFLOAT3X4*>(instanceDesc.Transform), mTransform);
    }

    // Lamps
    {
        auto& instanceDesc = instanceDescs[BottomLevelASType::Lamps];
        instanceDesc = {};
        instanceDesc.InstanceMask = 1;
        instanceDesc.InstanceContributionToHitGroupIndex = 7 * RayType::Count;
        instanceDesc.AccelerationStructure = bottomLevelASaddresses[BottomLevelASType::Lamps];

        // Calculate transformation matrix.
        auto position = XMFLOAT3(0.5f, 0.6f, 1.0f);
        const XMVECTOR vBasePosition = XMLoadFloat3(&fWidth) * XMLoadFloat3(&position);

        // Scale in all dimensions.
        XMMATRIX mScale = XMMatrixScaling(0.05f, 0.05f, 0.05f);
        XMMATRIX mTranslation = XMMatrixTranslationFromVector(vBasePosition);
        XMMATRIX mRotation= XMMatrixRotationY(XMConvertToRadians(180));
        XMMATRIX mTransform = mScale * mRotation * mTranslation;
        XMStoreFloat3x4(reinterpret_cast<XMFLOAT3X4*>(instanceDesc.Transform), mTransform);
    }

    // Sun Temple
    {
        auto& instanceDesc = instanceDescs[BottomLevelASType::SunTemple];
        instanceDesc = {};
        instanceDesc.InstanceMask = 1;
        instanceDesc.InstanceContributionToHitGroupIndex = 11 * RayType::Count;
        instanceDesc.AccelerationStructure = bottomLevelASaddresses[BottomLevelASType::SunTemple];

        // Calculate transformation matrix.
        auto position = XMFLOAT3(0.0f, -5.0f, 210.0f);
        const XMVECTOR vBasePosition = XMLoadFloat3(&fWidth) * XMLoadFloat3(&position);

        // Scale in all dimensions.
        XMMATRIX mScale = XMMatrixScaling(0.05f, 0.05f, 0.05f);
        XMMATRIX mTranslation = XMMatrixTranslationFromVector(vBasePosition);
        XMMATRIX mRotation = XMMatrixRotationY(XMConvertToRadians(180));
        XMMATRIX mTransform = mScale * mRotation * mTranslation;
        XMStoreFloat3x4(reinterpret_cast<XMFLOAT3X4*>(instanceDesc.Transform), mTransform);
    }

    UINT64 bufferSize = static_cast<UINT64>(instanceDescs.size() * sizeof(instanceDescs[0]));
    AllocateUploadBuffer(device, instanceDescs.data(), bufferSize, &(*instanceDescsResource), L"InstanceDescs");
};

AccelerationStructureBuffers Core::BuildBottomLevelAS(const vector<D3D12_RAYTRACING_GEOMETRY_DESC>& geometryDescs, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags)
{
    auto device = m_deviceResources->GetD3DDevice();
    auto commandList = m_deviceResources->GetCommandList();
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

    // Get maximum required space for data, scratch, and update buffers for these geometries with these parameters. You have to know this before creating the buffers, so that the are big enough.
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottomLevelPrebuildInfo = {};
    m_dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&bottomLevelInputs, &bottomLevelPrebuildInfo);
    ThrowIfFalse(bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

    // Create a scratch buffer.
    AllocateUAVBuffer(device, bottomLevelPrebuildInfo.ScratchDataSizeInBytes, &scratch, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");

    {
        D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
        AllocateUAVBuffer(device, bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes, &bottomLevelAS, initialResourceState, L"BottomLevelAccelerationStructure");
    }

    // bottom-level AS desc.
    {
        bottomLevelBuildDesc.ScratchAccelerationStructureData = scratch->GetGPUVirtualAddress();
        bottomLevelBuildDesc.DestAccelerationStructureData = bottomLevelAS->GetGPUVirtualAddress();
    }

    // Build the acceleration structure.
    m_dxrCommandList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);

    AccelerationStructureBuffers bottomLevelASBuffers;
    bottomLevelASBuffers.accelerationStructure = bottomLevelAS;
    bottomLevelASBuffers.scratch = scratch;
    bottomLevelASBuffers.ResultDataMaxSizeInBytes = bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes;
    return bottomLevelASBuffers;
}