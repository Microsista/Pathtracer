#include "../Core/stdafx.h"
#include "other.h"
#include "../Util/DDSTextureLoader.h"

void Room::CreateRootSignatures()
{
    auto device = m_deviceResources->GetD3DDevice();

    // Global Root Signature
    {
        CD3DX12_DESCRIPTOR_RANGE ranges[1] = {}; // Perfomance TIP: Order from most frequent to least frequent.
        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);  // 1 output texture
        //ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 1);  // 2 static index and vertex buffers.

        CD3DX12_ROOT_PARAMETER rootParameters[GlobalRootSignature::Slot::Count];
        rootParameters[GlobalRootSignature::Slot::OutputView].InitAsDescriptorTable(1, &ranges[0]);
        rootParameters[GlobalRootSignature::Slot::AccelerationStructure].InitAsShaderResourceView(0);
        rootParameters[GlobalRootSignature::Slot::SceneConstant].InitAsConstantBufferView(0);
        //rootParameters[GlobalRootSignature::Slot::VertexBuffers].InitAsDescriptorTable(1, &ranges[1]);
        rootParameters[GlobalRootSignature::Slot::TriangleAttributeBuffer].InitAsShaderResourceView(4);

        CD3DX12_STATIC_SAMPLER_DESC staticSamplers[] =
        {
            // LinearWrapSampler
            CD3DX12_STATIC_SAMPLER_DESC(0, SAMPLER_FILTER),
        };

        CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters, ARRAYSIZE(staticSamplers), staticSamplers);
        SerializeAndCreateRaytracingRootSignature(device, globalRootSignatureDesc, &m_raytracingGlobalRootSignature, L"Global root signature");
    }

    using namespace LocalRootSignature::Triangle;
    // Local Root Signature
    CD3DX12_DESCRIPTOR_RANGE ranges[Slot::Count] = {}; // Perfomance TIP: Order from most frequent to least frequent.
    ranges[Slot::IndexBuffer].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
    ranges[Slot::VertexBuffer].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
    ranges[Slot::DiffuseTexture].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3);
    //ranges[Slot::MaterialConstant].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 1); 
    //ranges[Slot::GeometryIndex].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 1); 
    

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

// Local root signature and shader association
// This is a root signature that enables a shader to have unique arguments that come from shader tables.
void Room::CreateLocalRootSignatureSubobjects(CD3DX12_STATE_OBJECT_DESC* raytracingPipeline)
{
    // Ray gen and miss shaders in this sample are not using a local root signature and thus one is not associated with them.

    // Hit groups
    auto localRootSignature = raytracingPipeline->CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
    localRootSignature->SetRootSignature(m_raytracingLocalRootSignature[LocalRootSignature::Type::Triangle].Get());
    // Shader association
    auto rootSignatureAssociation = raytracingPipeline->CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
    rootSignatureAssociation->SetSubobjectToAssociate(*localRootSignature);
    rootSignatureAssociation->AddExports(c_hitGroupNames_TriangleGeometry);
}

// Create a raytracing pipeline state object (RTPSO).
// An RTPSO represents a full set of shaders reachable by a DispatchRays() call,
// with all configuration options resolved, such as local signatures and other state.
void Room::CreateRaytracingPipelineStateObject()
{
    // Create 18 subobjects that combine into a RTPSO:
    // Subobjects need to be associated with DXIL exports (i.e. shaders) either by way of default or explicit associations.
    // Default association applies to every exported shader entrypoint that doesn't have any of the same type of subobject associated with it.
    // This simple sample utilizes default shader association except for local root signature subobject
    // which has an explicit association specified purely for demonstration purposes.
    // 1 - DXIL library
    // 8 - Hit group types - 4 geometries (1 triangle, 3 aabb) x 2 ray types (ray, shadowRay)
    // 1 - Shader config
    // 6 - 3 x Local root signature and association
    // 1 - Global root signature
    // 1 - Pipeline config
    CD3DX12_STATE_OBJECT_DESC raytracingPipeline{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

    // DXIL library
    CreateDxilLibrarySubobject(&raytracingPipeline);

    // Hit groups
    CreateHitGroupSubobjects(&raytracingPipeline);

    // Shader config
    // Defines the maximum sizes in bytes for the ray rayPayload and attribute structure.
    auto shaderConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
    UINT payloadSize = max(sizeof(RayPayload), sizeof(ShadowRayPayload));
    UINT attributeSize = sizeof(struct ProceduralPrimitiveAttributes);
    shaderConfig->Config(payloadSize, attributeSize);

    // Local root signature and shader association
    // This is a root signature that enables a shader to have unique arguments that come from shader tables.
    CreateLocalRootSignatureSubobjects(&raytracingPipeline);

    // Global root signature
    // This is a root signature that is shared across all raytracing shaders invoked during a DispatchRays() call.
    auto globalRootSignature = raytracingPipeline.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
    globalRootSignature->SetRootSignature(m_raytracingGlobalRootSignature.Get());

    // Pipeline config
    // Defines the maximum TraceRay() recursion depth.
    auto pipelineConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
    // PERFOMANCE TIP: Set max recursion depth as low as needed
    // as drivers may apply optimization strategies for low recursion depths.
    UINT maxRecursionDepth = MAX_RAY_RECURSION_DEPTH;
    pipelineConfig->Config(maxRecursionDepth);

    PrintStateObjectDesc(raytracingPipeline);

    // Create the state object.
    ThrowIfFailed(m_dxrDevice->CreateStateObject(raytracingPipeline, IID_PPV_ARGS(&m_dxrStateObject)), L"Couldn't create DirectX Raytracing state object.\n");
}

void Room::CreateDescriptorHeap()
{
    auto device = m_deviceResources->GetD3DDevice();

    m_descriptorHeap = std::make_shared<DX::DescriptorHeap>(device, 100, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    //D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
    //// Allocate a heap for 3 descriptors:
    //// 1 - raytracing output texture SRV
    //descriptorHeapDesc.NumDescriptors = 7;
    //descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    //descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    //descriptorHeapDesc.NodeMask = 0;
    //auto heap = m_descriptorHeap.GetHeap();
    //device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&heap));
    //NAME_D3D12_OBJECT(m_descriptorHeap.);

    m_descriptorSize = m_descriptorHeap->DescriptorSize();//device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

// make sure vertex and index buffers are seperate
// Build geometry used in the sample.
void Room::BuildGeometry()
{
    auto device = m_deviceResources->GetD3DDevice();

    GeometryGenerator geoGen;
    GeometryGenerator::MeshData room = geoGen.CreateRoom(10.0f, 5.0f, 10.0f);
    GeometryGenerator::MeshData coordinateSystem = geoGen.CreateCoordinates(20.0f, 0.01f, 0.01f);
    GeometryGenerator::MeshData skull = geoGen.CreateSkull(0.0f, 0.0f, 0.0f);
    //GeometryGenerator::MeshData skull = geoGen.CreateSphere(2.0f, 5, 5);

    //
    // We are concatenating all the geometry into one big vertex/index buffer.  So
    // define the regions in the buffer each submesh covers.
    //

    // Cache the vertex offsets to each object in the concatenated vertex buffer.
    // This is basically an integer value you add to each index of such geometry.
    //UINT roomVertexOffset = 0;
    //UINT coordinateSystemVertexOffset = (UINT)room.Vertices.size();
    //UINT skullVertexOffset = (UINT)room.Vertices.size() + coordinateSystem.Vertices.size();

    //// Cache the starting index for each object in the concatenated index buffer.
    //// This let's you distinguish between geometries in the buffers.
    //UINT roomIndexOffset = 0;
    //UINT coordinateSystemIndexOffset = (UINT)room.Indices32.size();
    //UINT skullIndexOffset = (UINT)room.Indices32.size() + (UINT)coordinateSystem.Indices32.size();
    UINT roomVertexOffset = 0;
    UINT coordinateSystemVertexOffset = 0;
    UINT skullVertexOffset = 0;

    // Cache the starting index for each object in the concatenated index buffer.
    // This let's you distinguish between geometries in the buffers.
    UINT roomIndexOffset = 0;
    UINT coordinateSystemIndexOffset = 0;
    UINT skullIndexOffset = 0;

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
    //submesh.Bounds = bounds;

    //
    // Extract the vertex elements we are interested in and pack the
    // vertices of all the meshes into one vertex buffer.
    //

   // auto totalVertexCount = room.Vertices.size() + coordinateSystem.Vertices.size() + skull.Vertices.size();

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
        skullVertices[k].textureCoordinate = skull.Vertices[i].TexC;
        skullVertices[k].tangent = skull.Vertices[i].TangentU;
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

    AllocateUploadBuffer(device, &coordinateSystemIndices[0], csibByteSize, &m_indexBuffer[TriangleGeometry::Coordinates].resource);
    AllocateUploadBuffer(device, &coordinatesVertices[0], csvbByteSize, &m_vertexBuffer[TriangleGeometry::Coordinates].resource);

    AllocateUploadBuffer(device, &skullIndices[0], skullibByteSize, &m_indexBuffer[TriangleGeometry::Skull].resource);
    AllocateUploadBuffer(device, &skullVertices[0], skullvbByteSize, &m_vertexBuffer[TriangleGeometry::Skull].resource);

    // Vertex buffer is passed to the shader along with index buffer as a descriptor range.
    CreateBufferSRV(&m_indexBuffer[TriangleGeometry::Room], roomIndices.size(), 0);
    CreateBufferSRV(&m_vertexBuffer[TriangleGeometry::Room], roomVertices.size(), sizeof(roomVertices[0]));

    CreateBufferSRV(&m_indexBuffer[TriangleGeometry::Coordinates], coordinateSystemIndices.size(), 0);
    CreateBufferSRV(&m_vertexBuffer[TriangleGeometry::Coordinates], coordinatesVertices.size(), sizeof(coordinatesVertices[0]));

    CreateBufferSRV(&m_indexBuffer[TriangleGeometry::Skull], skullIndices.size(), 0);
    CreateBufferSRV(&m_vertexBuffer[TriangleGeometry::Skull], skullVertices.size(), sizeof(skullVertices[0]));

    auto roomGeo = std::make_unique<MeshGeometry>();
    auto csGeo = std::make_unique<MeshGeometry>();
    auto skullGeo = std::make_unique<MeshGeometry>();

    roomGeo->Name = "roomGeo";
    csGeo->Name = "csGeo";
    skullGeo->Name = "skullGeo";

    roomGeo->VertexBufferGPU = m_vertexBuffer[TriangleGeometry::Room].resource;
    roomGeo->IndexBufferGPU = m_indexBuffer[TriangleGeometry::Room].resource;
    
    csGeo->VertexBufferGPU = m_vertexBuffer[TriangleGeometry::Coordinates].resource;
    csGeo->IndexBufferGPU = m_indexBuffer[TriangleGeometry::Coordinates].resource;
    
    skullGeo->VertexBufferGPU = m_vertexBuffer[TriangleGeometry::Skull].resource;
    skullGeo->IndexBufferGPU = m_indexBuffer[TriangleGeometry::Skull].resource;

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

    //ThrowIfFalse(descriptorIndexVB == descriptorIndexIB + 1, L"Vertex Buffer descriptor index must follow that of Index Buffer descriptor index");
}

void Room::BuildGeometryDescsForBottomLevelAS(array<vector<D3D12_RAYTRACING_GEOMETRY_DESC>, BottomLevelASType::Count>& geometryDescs)
{
    D3D12_RAYTRACING_GEOMETRY_FLAGS geometryFlags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

    D3D12_RAYTRACING_GEOMETRY_DESC triangleDescTemplate{};
    triangleDescTemplate.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;        
    triangleDescTemplate.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
    triangleDescTemplate.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
    triangleDescTemplate.Triangles.VertexBuffer.StrideInBytes = sizeof(VertexPositionNormalTextureTangent);
    triangleDescTemplate.Flags = geometryFlags;

    geometryDescs[BottomLevelASType::Triangle].resize(TriangleGeometry::Count, triangleDescTemplate);

    // Seperate geometries for each object allows for seperate hit shaders.
    for (UINT i = 0; i < TriangleGeometry::Count; i++)
    {
        auto& geometryDesc = geometryDescs[BottomLevelASType::Triangle][i];
        
        switch (i)
        {
        case 0:
            geometryDesc.Triangles.IndexBuffer = m_indexBuffer[TriangleGeometry::Room].resource->GetGPUVirtualAddress();
            geometryDesc.Triangles.VertexBuffer.StartAddress = m_vertexBuffer[TriangleGeometry::Room].resource->GetGPUVirtualAddress();
            geometryDesc.Triangles.IndexCount = m_geometries["roomGeo"]->DrawArgs["room"].IndexCount;
            geometryDesc.Triangles.VertexCount = m_geometries["roomGeo"]->DrawArgs["room"].VertexCount;
            break;
        case 1:
            geometryDesc.Triangles.IndexBuffer = m_indexBuffer[TriangleGeometry::Coordinates].resource->GetGPUVirtualAddress();
            geometryDesc.Triangles.VertexBuffer.StartAddress = m_vertexBuffer[TriangleGeometry::Coordinates].resource->GetGPUVirtualAddress();
            geometryDesc.Triangles.IndexCount = m_geometries["csGeo"]->DrawArgs["coordinateSystem"].IndexCount;
            geometryDesc.Triangles.VertexCount = m_geometries["csGeo"]->DrawArgs["coordinateSystem"].VertexCount;
            break;
        case 2:
            geometryDesc.Triangles.IndexBuffer = m_indexBuffer[TriangleGeometry::Skull].resource->GetGPUVirtualAddress();
            geometryDesc.Triangles.VertexBuffer.StartAddress = m_vertexBuffer[TriangleGeometry::Skull].resource->GetGPUVirtualAddress();
            geometryDesc.Triangles.IndexCount = m_geometries["skullGeo"]->DrawArgs["skull"].IndexCount;
            geometryDesc.Triangles.VertexCount = m_geometries["skullGeo"]->DrawArgs["skull"].VertexCount;
            break;
        }
    }  
}




// This encapsulates all shader records - shaders and the arguments for their local root signatures.
void Room::BuildShaderTables()
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
        UINT numShaderRecords = TriangleGeometry::Count * RayType::Count;
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
                auto texture = m_stoneTexture.gpuDescriptorHandle;
                memcpy(&rootArgs.indexBufferGPUHandle, &ib, sizeof(ib));
                memcpy(&rootArgs.vertexBufferGPUHandle, &vb, sizeof(ib));
                memcpy(&rootArgs.diffuseTextureGPUHandle, &texture, sizeof(texture));

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



void Room::DoRaytracing()
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
        //commandList->SetComputeRootDescriptorTable(GlobalRootSignature::Slot::VertexBuffers, m_indexBuffer.gpuDescriptorHandle);
        commandList->SetComputeRootDescriptorTable(GlobalRootSignature::Slot::OutputView, m_raytracingOutputResourceUAVGpuDescriptor);
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

void Room::LoadTextures()
{
    auto device = m_deviceResources->GetD3DDevice();
    auto commandList = m_deviceResources->GetCommandList();
    auto commandQueue = m_deviceResources->GetCommandQueue();

    commandList->Reset(m_deviceResources->GetCommandAllocator(), nullptr);

    auto stoneTex = std::make_unique<Texture>();
    stoneTex->Name = "stoneTex";
    stoneTex->Filename = L"Textures/stone.dds";
    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(device,
        commandList, stoneTex->Filename.c_str(),
        stoneTex->Resource, stoneTex->UploadHeap));

    AllocateUploadBuffer(device, stoneTex->Resource.Get(), sizeof(stoneTex->Resource), &m_stoneTexture.resource);

    CreateTextureSRV(&m_stoneTexture, 1, sizeof(stoneTex->Resource));

    //m_textures[stoneTex->Name] = std::move(stoneTex);

    commandList->Close();
    ID3D12CommandList* cmdsLists[] = { commandList };
    commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    m_deviceResources->WaitForGpu();
}

