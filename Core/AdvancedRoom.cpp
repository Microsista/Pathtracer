#include "../Core/stdafx.h"
#include "AdvancedRoom.h"
#include "../Obj/Debug/CompiledShaders/Raytracing.hlsl.h"
#include "../Util/SimpleGeometry.h"
#include "../Util/Geometry.h"

using namespace std;
using namespace DX;

// Shader entry points.
const wchar_t* Room::c_raygenShaderName = L"MyRaygenShader";
const wchar_t* Room::c_intersectionShaderNames[] =
{
    L"MyIntersectionShader_AnalyticPrimitive",
};
const wchar_t* Room::c_closestHitShaderNames[] =
{
    L"MyClosestHitShader_Triangle",
    L"MyClosestHitShader_AABB",
};
const wchar_t* Room::c_missShaderNames[] =
{
    L"MyMissShader", L"MyMissShader_ShadowRay"
};
// Hit groups.
const wchar_t* Room::c_hitGroupNames_TriangleGeometry[] = 
{ 
    L"MyHitGroup_Triangle", L"MyHitGroup_Triangle_ShadowRay" 
};
const wchar_t* Room::c_hitGroupNames_AABBGeometry[][RayType::Count] = 
{
    { L"MyHitGroup_AABB_AnalyticPrimitive", L"MyHitGroup_AABB_AnalyticPrimitive_ShadowRay" },
};

Room::Room(UINT width, UINT height, std::wstring name) :
    DXSample(width, height, name),
    m_raytracingOutputResourceUAVDescriptorHeapIndex(UINT_MAX),
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


void Room::OnInit()
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
    m_deviceResources->SetWindow(Win32Application::GetHwnd(), m_width, m_height);
    m_deviceResources->InitializeDXGIAdapter();

    ThrowIfFalse(IsDirectXRaytracingSupported(m_deviceResources->GetAdapter()),
        L"ERROR: DirectX Raytracing is not supported by your OS, GPU and/or driver.\n\n");

    m_deviceResources->CreateDeviceResources();
    m_deviceResources->CreateWindowSizeDependentResources();

    InitializeScene();

    CreateDeviceDependentResources();
    CreateWindowSizeDependentResources();
}

// Update camera matrices passed into the shader.
void Room::UpdateCameraMatrices()
{
    auto frameIndex = m_deviceResources->GetCurrentFrameIndex();

    //m_sceneCB->cameraPosition = m_eye;
    m_sceneCB->cameraPosition = m_camera.GetPosition();
    float fovAngleY = 45.0f;
    XMMATRIX view = m_camera.GetView();
    XMMATRIX proj = m_camera.GetProj();
    /*XMMATRIX view = XMMatrixLookAtLH(m_eye, m_at, m_up);
    XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(fovAngleY), m_aspectRatio, 0.01f, 125.0f);*/
    XMMATRIX viewProj = view * proj;
    //m_sceneCB->projectionToWorld = XMMatrixInverse(nullptr, viewProj);
    
    m_sceneCB->projectionToWorld = XMMatrixInverse(nullptr, XMMatrixMultiply(m_camera.GetView(), m_camera.GetProj()));
}

// ntransform
// Update AABB primite attributes buffers passed into the shader.
void Room::UpdateAABBPrimitiveAttributes(float animationTime)
{
    // Move objects within a BLAS(or World) space, every frame.
    auto frameIndex = m_deviceResources->GetCurrentFrameIndex();

    XMMATRIX mIdentity = XMMatrixIdentity();
    
    XMMATRIX mScale15y = XMMatrixScaling(1, 1.5f, 1);
    XMMATRIX mScaleMirror = XMMatrixScaling(0.1f, 1.5f, 1);
    XMMATRIX mRotationMirror = XMMatrixRotationZ(XMConvertToRadians(22.5));
    XMMATRIX mScale15 = XMMatrixScaling(1.5, 1.5, 1.5);
    XMMATRIX mScale2 = XMMatrixScaling(2, 2, 2);
    XMMATRIX mScale3 = XMMatrixScaling(3, 3, 3);

    XMMATRIX mRotation = XMMatrixRotationY(-2 * animationTime);

    

    XMMATRIX mTranslationMirror = XMMatrixTranslation(-4.25f, -1.0f, 3.75f);
    XMMATRIX mTranslationSphere = XMMatrixTranslation(0.0f, 1.0f, 0.0f);

    XMVECTOR vTranslationLight = m_sceneCB->lightPosition + XMVECTOR{ 0.0f, 2.0f, 0.0f, 0.0f };
    XMMATRIX mTranslationLight = XMMatrixTranslationFromVector(vTranslationLight);

    // Apply scale, rotation and translation transforms.
    // The intersection shader tests in this sample work with local space, so here
    // we apply the BLAS object space translation that was passed to geometry descs.
    auto SetTransformForAABB = [&](UINT primitiveIndex, XMMATRIX& mScale, XMMATRIX& mRotation)
    {
        XMVECTOR vTranslation = 
            0.5f * ( XMLoadFloat3(reinterpret_cast<XMFLOAT3*>(&m_aabbs[primitiveIndex + 1].MinX))
                   + XMLoadFloat3(reinterpret_cast<XMFLOAT3*>(&m_aabbs[primitiveIndex + 1].MaxX)));
        XMMATRIX mTranslation = XMMatrixTranslationFromVector(vTranslation);
     
        XMMATRIX mTransform = mScale * mRotation * mTranslation;
        m_aabbPrimitiveAttributeBuffer[primitiveIndex].localSpaceToBottomLevelAS = mTransform;
        m_aabbPrimitiveAttributeBuffer[primitiveIndex].bottomLevelASToLocalSpace = XMMatrixInverse(nullptr, mTransform);
    };

    auto SetTransformForAABB2 = [&](UINT primitiveIndex, XMMATRIX& mScale, XMMATRIX& mRotation)
    {
        XMVECTOR vTranslation = m_sceneCB->lightPosition;
        XMMATRIX mTranslation = XMMatrixTranslationFromVector(vTranslation);

        XMMATRIX mTransform = mScale * mRotation * mTranslation;
        m_aabbPrimitiveAttributeBuffer[primitiveIndex].localSpaceToBottomLevelAS = mTransform;
        m_aabbPrimitiveAttributeBuffer[primitiveIndex].bottomLevelASToLocalSpace = XMMatrixInverse(nullptr, mTransform);
    };

    auto SetTransformForAABBCustom = [&](UINT primitiveIndex, XMMATRIX& mScale, XMMATRIX& mRotation, XMMATRIX& mTranslation)
    {
        XMMATRIX mTransform = mScale * mRotation * mTranslation;
        m_aabbPrimitiveAttributeBuffer[primitiveIndex].localSpaceToBottomLevelAS = mTransform;
        m_aabbPrimitiveAttributeBuffer[primitiveIndex].bottomLevelASToLocalSpace = XMMatrixInverse(nullptr, mTransform);
    };
    
    // Analytic primitives.
    {
        using namespace AnalyticPrimitive;
        SetTransformForAABB(AABB, mScaleMirror, mRotationMirror);
        SetTransformForAABB(Spheres, mIdentity, mIdentity);
        SetTransformForAABB2(Sun, mIdentity, mIdentity);
        //SetTransformForAABBCustom(Ball, mIdentity, mIdentity, mIdentity);
    }
}

// Update Triangle primite attributes buffers passed into the shader.
void Room::UpdateTrianglePrimitiveAttributes(float animationTime)
{
    auto frameIndex = m_deviceResources->GetCurrentFrameIndex();

    XMMATRIX mIdentity = XMMatrixIdentity();

    XMMATRIX mScale15y = XMMatrixScaling(1, 1.5, 1);
    XMMATRIX mScaleMirror = XMMatrixScaling(0.1, 1.5, 1);
    XMMATRIX mRotationMirror = XMMatrixRotationZ(XMConvertToRadians(22.5));
    XMMATRIX mScale15 = XMMatrixScaling(1.5, 1.5, 1.5);
    XMMATRIX mScale2 = XMMatrixScaling(2, 2, 2);
    XMMATRIX mScale3 = XMMatrixScaling(3, 3, 3);

    XMMATRIX mRotation = XMMatrixRotationY(-2 * animationTime);

    XMMATRIX mTranslation = XMMatrixTranslation(0.0f, 2.5f, 0.0f);

    // Apply scale, rotation and translation transforms.
    // The intersection shader tests in this sample work with local space, so here
    // we apply the BLAS object space translation that was passed to geometry descs.
    /*auto SetTransformForTriangle = [&](UINT primitiveIndex, XMMATRIX& mScale, XMMATRIX& mRotation)
    {
        XMVECTOR vTranslation =
            0.5f * (XMLoadFloat3(reinterpret_cast<XMFLOAT3*>(&m_aabbs[primitiveIndex].MinX))
                + XMLoadFloat3(reinterpret_cast<XMFLOAT3*>(&m_aabbs[primitiveIndex].MaxX)));
        XMMATRIX mTranslation = XMMatrixTranslationFromVector(vTranslation);

        XMMATRIX mTransform = mScale * mRotation * mTranslation;
        m_trianglePrimitiveAttributeBuffer[primitiveIndex].localSpaceToBottomLevelAS = mTransform;
        m_trianglePrimitiveAttributeBuffer[primitiveIndex].bottomLevelASToLocalSpace = XMMatrixInverse(nullptr, mTransform);
    };*/

    auto SetTransformForTriangle2 = [&](UINT primitiveIndex, XMMATRIX& mScale, XMMATRIX& mRotation, XMMATRIX& mTranslation)
    {
        XMMATRIX mTransform = mScale * mRotation * mTranslation;
        m_trianglePrimitiveAttributeBuffer[primitiveIndex].localSpaceToBottomLevelAS = mTransform;
        m_trianglePrimitiveAttributeBuffer[primitiveIndex].bottomLevelASToLocalSpace = XMMatrixInverse(nullptr, mTransform);
    };

    UINT offset = 0;
    // Analytic primitives.
    {
        using namespace TriangleGeometry;
        SetTransformForTriangle2(offset + TriangleGeometry::Room, mScaleMirror, mRotationMirror, mTranslation);
        SetTransformForTriangle2(offset + Coordinates, mScale15, mIdentity, mIdentity);
        offset += TriangleGeometry::Count;
    }
}

//nscene, nmaterials, ninit
// Initialize scene rendering parameters.
void Room::InitializeScene()
{
    auto frameIndex = m_deviceResources->GetCurrentFrameIndex();

    // Setup materials.
    {
        auto SetAttributes = [&](
            UINT primitiveIndex, 
            const XMFLOAT4& albedo, 
            float reflectanceCoef = 0.0f,
            float diffuseCoef = 0.9f,
            float specularCoef = 0.7f,
            float specularPower = 0.0f,
            float stepScale = 1.0f,
            float shaded = 1.0f)
        {
            auto& attributes = m_aabbMaterialCB[primitiveIndex];
            attributes.albedo = albedo;
            attributes.reflectanceCoef = reflectanceCoef;
            attributes.diffuseCoef = diffuseCoef;
            attributes.specularCoef = specularCoef;
            attributes.specularPower = specularPower;
            attributes.stepScale = stepScale;
            attributes.shaded = shaded;
        };

        auto SetAttributes2 = [&](
            UINT primitiveIndex,
            const XMFLOAT4& albedo,
            float reflectanceCoef = 0.0f,
            float diffuseCoef = 0.9f,
            float specularCoef = 0.7f,
            float specularPower = 0.0f,
            float stepScale = 1.0f,
            float shaded = 1.0f)
        {
            auto& attributes = m_triangleMaterialCB[primitiveIndex];
            attributes.albedo = albedo;
            attributes.reflectanceCoef = reflectanceCoef;
            attributes.diffuseCoef = diffuseCoef;
            attributes.specularCoef = specularCoef;
            attributes.specularPower = specularPower;
            attributes.stepScale = stepScale;
            attributes.shaded = shaded;
        };

        // Albedos
        XMFLOAT4 green = XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f);
        XMFLOAT4 red = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);
        XMFLOAT4 yellow = XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f);
        XMFLOAT4 white = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        

        SetAttributes2(TriangleGeometry::Room, XMFLOAT4(1.000f, 0.766f, 0.336f, 1.000f), 0.1f, 1.0f, 0.3f, 0.5f, 1.0f, 1.0f);
        SetAttributes2(TriangleGeometry::Coordinates, red, 0, 0.9, 0.7, 1.0, 1.0f, 0.0f);
     
        // Analytic primitives.
        {
            using namespace AnalyticPrimitive;
            SetAttributes(AABB, ChromiumReflectance, 1);
            SetAttributes(Spheres, ChromiumReflectance, 1, 1.0f, 1.0f, 0.1f, 1.0f);
            SetAttributes(Sun, white, 1, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f);
            //SetAttributes(Ball, ChromiumReflectance, 1, 1.0f, 1.0f, 50.0f, 1.0f);

        }
    }

    // Setup camera.
    {
        m_camera.SetPosition(0.0f, 2.0f, -15.0f);
        // Initialize the view and projection inverse matrices.
        m_eye = { 0.0f, 1.6f, -5.0f, 1.0f }; 
        m_at = m_eye + XMVECTOR{ 0.0f, 0.0f, 1.0f, 0.0f };
        XMVECTOR right = { 1.0f, 0.0f, 0.0f, 0.0f };

        XMVECTOR direction = XMVector4Normalize(m_at - m_eye);
        m_up = XMVector3Normalize(XMVector3Cross(direction, right));

        // Rotate camera around Y axis.
       /* XMMATRIX rotate = XMMatrixRotationY(XMConvertToRadians(45.0f));
        m_eye = XMVector3Transform(m_eye, rotate);
        m_up = XMVector3Transform(m_up, rotate);*/
        m_camera.UpdateViewMatrix();
        UpdateCameraMatrices();
    }

    // Setup lights.
    {
        // Initialize the lighting parameters.
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

// Create constant buffers.
void Room::CreateConstantBuffers()
{
    auto device = m_deviceResources->GetD3DDevice();
    auto frameCount = m_deviceResources->GetBackBufferCount();

    m_sceneCB.Create(device, frameCount, L"Scene Constant Buffer");
}

// Create AABB primitive attributes buffers.
void Room::CreateAABBPrimitiveAttributesBuffers()
{
    auto device = m_deviceResources->GetD3DDevice();
    auto frameCount = m_deviceResources->GetBackBufferCount();
    m_aabbPrimitiveAttributeBuffer.Create(device, IntersectionShaderType::TotalPrimitiveCount, frameCount, L"AABB primitive attributes");
}

void Room::CreateTrianglePrimitiveAttributesBuffers()
{
    auto device = m_deviceResources->GetD3DDevice();
    auto frameCount = m_deviceResources->GetBackBufferCount();
    m_trianglePrimitiveAttributeBuffer.Create(device, TriangleGeometry::Count, frameCount, L"Triangle primitive attributes");
}

// Create resources that depend on the device.
void Room::CreateDeviceDependentResources()
{
    CreateAuxilaryDeviceResources();

    // Initialize raytracing pipeline.

    // Create raytracing interfaces: raytracing device and commandlist.
    CreateRaytracingInterfaces();

    // Create root signatures for the shaders.
    CreateRootSignatures();

    // Create a raytracing pipeline state object which defines the binding of shaders, state and resources to be used during raytracing.
    CreateRaytracingPipelineStateObject();

    // Create a heap for descriptors.
    CreateDescriptorHeap();

    // Build geometry to be used in the sample.
    BuildGeometry();

    // Build raytracing acceleration structures from the generated geometry.
    BuildAccelerationStructures();

    // Create constant buffers for the geometry and the scene.
    CreateConstantBuffers();

    // Create AABB primitive attribute buffers.
    CreateAABBPrimitiveAttributesBuffers();

    // Create triangle primitive attribute buffers.
    CreateTrianglePrimitiveAttributesBuffers();

    // Build shader tables, which define shaders and their local root arguments.
    BuildShaderTables();

    // Create an output 2D texture to store the raytracing result to.
    CreateRaytracingOutputResource();

    m_denoiser.Setup(m_deviceResources, m_cbvSrvUavHeap);
}

void Room::SerializeAndCreateRaytracingRootSignature(D3D12_ROOT_SIGNATURE_DESC& desc, ComPtr<ID3D12RootSignature>* rootSig)
{
    auto device = m_deviceResources->GetD3DDevice();
    ComPtr<ID3DBlob> blob;
    ComPtr<ID3DBlob> error;

    ThrowIfFailed(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error), error ? static_cast<wchar_t*>(error->GetBufferPointer()) : nullptr);
    ThrowIfFailed(device->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&(*rootSig))));
}
//nroot
void Room::CreateRootSignatures()
{
    auto device = m_deviceResources->GetD3DDevice();

    // Global Root Signature
    // This is a root signature that is shared across all raytracing shaders invoked during a DispatchRays() call.
    {
        CD3DX12_DESCRIPTOR_RANGE ranges[2]; // Perfomance TIP: Order from most frequent to least frequent.
        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);  // 1 output texture
        ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 1);  // 2 static index and vertex buffers.

        CD3DX12_ROOT_PARAMETER rootParameters[GlobalRootSignature::Slot::Count];
        rootParameters[GlobalRootSignature::Slot::OutputView].InitAsDescriptorTable(1, &ranges[0]);
        rootParameters[GlobalRootSignature::Slot::AccelerationStructure].InitAsShaderResourceView(0);
        rootParameters[GlobalRootSignature::Slot::SceneConstant].InitAsConstantBufferView(0);
        rootParameters[GlobalRootSignature::Slot::AABBattributeBuffer].InitAsShaderResourceView(3);
        rootParameters[GlobalRootSignature::Slot::VertexBuffers].InitAsDescriptorTable(1, &ranges[1]);
        rootParameters[GlobalRootSignature::Slot::TriangleAttributeBuffer].InitAsShaderResourceView(4);
        CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters);
        SerializeAndCreateRaytracingRootSignature(globalRootSignatureDesc, &m_raytracingGlobalRootSignature);
    }

    // Local Root Signature
    // This is a root signature that enables a shader to have unique arguments that come from shader tables.
    {
        // Triangle geometry
        {
            namespace RootSignatureSlots = LocalRootSignature::Triangle::Slot;
            CD3DX12_ROOT_PARAMETER rootParameters[RootSignatureSlots::Count];
            rootParameters[RootSignatureSlots::MaterialConstant].InitAsConstants(SizeOfInUint32(PrimitiveConstantBuffer), 1);
            rootParameters[RootSignatureSlots::GeometryIndex].InitAsConstants(SizeOfInUint32(PrimitiveInstanceConstantBuffer), 3);

            CD3DX12_ROOT_SIGNATURE_DESC localRootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters);
            localRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
            SerializeAndCreateRaytracingRootSignature(localRootSignatureDesc, &m_raytracingLocalRootSignature[LocalRootSignature::Type::Triangle]);
        }

        // AABB geometry
        {
            namespace RootSignatureSlots = LocalRootSignature::AABB::Slot;
            CD3DX12_ROOT_PARAMETER rootParameters[RootSignatureSlots::Count];
            rootParameters[RootSignatureSlots::MaterialConstant].InitAsConstants(SizeOfInUint32(PrimitiveConstantBuffer), 1);
            rootParameters[RootSignatureSlots::GeometryIndex].InitAsConstants(SizeOfInUint32(PrimitiveInstanceConstantBuffer), 2);

            CD3DX12_ROOT_SIGNATURE_DESC localRootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters);
            localRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
            SerializeAndCreateRaytracingRootSignature(localRootSignatureDesc, &m_raytracingLocalRootSignature[LocalRootSignature::Type::AABB]);
        }
    }
}

// Create raytracing device and command list.
void Room::CreateRaytracingInterfaces()
{
    auto device = m_deviceResources->GetD3DDevice();
    auto commandList = m_deviceResources->GetCommandList();

    ThrowIfFailed(device->QueryInterface(IID_PPV_ARGS(&m_dxrDevice)), L"Couldn't get DirectX Raytracing interface for the device.\n");
    ThrowIfFailed(commandList->QueryInterface(IID_PPV_ARGS(&m_dxrCommandList)), L"Couldn't get DirectX Raytracing interface for the command list.\n");
}

// DXIL library
// This contains the shaders and their entrypoints for the state object.
// Since shaders are not considered a subobject, they need to be passed in via DXIL library subobjects.
void Room::CreateDxilLibrarySubobject(CD3DX12_STATE_OBJECT_DESC* raytracingPipeline)
{
    auto lib = raytracingPipeline->CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
    D3D12_SHADER_BYTECODE libdxil = CD3DX12_SHADER_BYTECODE((void *)g_pRaytracing, ARRAYSIZE(g_pRaytracing));
    lib->SetDXILLibrary(&libdxil);
    // Use default shader exports for a DXIL library/collection subobject ~ surface all shaders.
}

// Hit groups
// A hit group specifies closest hit, any hit and intersection shaders 
// to be executed when a ray intersects the geometry.
void Room::CreateHitGroupSubobjects(CD3DX12_STATE_OBJECT_DESC* raytracingPipeline)
{
    // Triangle geometry hit groups
    {
        for (UINT rayType = 0; rayType < RayType::Count; rayType++)
        {
            auto hitGroup = raytracingPipeline->CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
            if (rayType == RayType::Radiance)
            {
                hitGroup->SetClosestHitShaderImport(c_closestHitShaderNames[GeometryType::Triangle]);
            }
            hitGroup->SetHitGroupExport(c_hitGroupNames_TriangleGeometry[rayType]);
            hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);
        }
    }

    // AABB geometry hit groups
    {
        // Create hit groups for each intersection shader.
        for (UINT t = 0; t < IntersectionShaderType::Count; t++)
            for (UINT rayType = 0; rayType < RayType::Count; rayType++)
            {
                auto hitGroup = raytracingPipeline->CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
                hitGroup->SetIntersectionShaderImport(c_intersectionShaderNames[t]);
                if (rayType == RayType::Radiance)
                {
                    hitGroup->SetClosestHitShaderImport(c_closestHitShaderNames[GeometryType::AABB]);
                }
                hitGroup->SetHitGroupExport(c_hitGroupNames_AABBGeometry[t][rayType]);
                hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_PROCEDURAL_PRIMITIVE);
            }
    }
}

// Local root signature and shader association
// This is a root signature that enables a shader to have unique arguments that come from shader tables.
void Room::CreateLocalRootSignatureSubobjects(CD3DX12_STATE_OBJECT_DESC* raytracingPipeline)
{
    // Ray gen and miss shaders in this sample are not using a local root signature and thus one is not associated with them.

    // Hit groups
    // Triangle geometry
    {
        auto localRootSignature = raytracingPipeline->CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
        localRootSignature->SetRootSignature(m_raytracingLocalRootSignature[LocalRootSignature::Type::Triangle].Get());
        // Shader association
        auto rootSignatureAssociation = raytracingPipeline->CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
        rootSignatureAssociation->SetSubobjectToAssociate(*localRootSignature);
        rootSignatureAssociation->AddExports(c_hitGroupNames_TriangleGeometry);
    }

    // AABB geometry
    {
        auto localRootSignature = raytracingPipeline->CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
        localRootSignature->SetRootSignature(m_raytracingLocalRootSignature[LocalRootSignature::Type::AABB].Get());
        // Shader association
        auto rootSignatureAssociation = raytracingPipeline->CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
        rootSignatureAssociation->SetSubobjectToAssociate(*localRootSignature);
        for (auto& hitGroupsForIntersectionShaderType : c_hitGroupNames_AABBGeometry)
        {
            rootSignatureAssociation->AddExports(hitGroupsForIntersectionShaderType);
        }
    }
}

//npipeline, npso
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

void Room::CreateAuxilaryDeviceResources()
{
    auto device = m_deviceResources->GetD3DDevice();
    auto commandQueue = m_deviceResources->GetCommandQueue();

    for (auto& gpuTimer : m_gpuTimers)
    {
        gpuTimer.RestoreDevice(device, commandQueue, FrameCount);
    }
}

// Create a 2D output texture for raytracing.
void Room::CreateRaytracingOutputResource()
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
}


void Room::CreateDescriptorHeap()
{
    auto device = m_deviceResources->GetD3DDevice();

    D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
    // Allocate a heap for 6 descriptors:
    // 2 - vertex and index  buffer SRVs
    // 1 - raytracing output texture SRV
    descriptorHeapDesc.NumDescriptors = 3;
    descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    descriptorHeapDesc.NodeMask = 0;
    device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&m_descriptorHeap));
    NAME_D3D12_OBJECT(m_descriptorHeap);

    m_descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

//naabbs
// Build AABBs for procedural geometry within a bottom-level acceleration structure.
void Room::BuildProceduralGeometryAABBs()
{
    auto device = m_deviceResources->GetD3DDevice();

    auto InitializeAABB = [&](auto offset, auto size)
    {
        return D3D12_RAYTRACING_AABB { 
            offset.x, 
            offset.y,
            offset.z,
            offset.x + size.x,
            offset.y + size.y,
            offset.z + size.z,
        };
    };
    m_aabbs.resize(IntersectionShaderType::TotalPrimitiveCount);
    UINT offset = 1;
    // Analytic primitives.
    {
        using namespace AnalyticPrimitive;
       // m_aabbs[AABB] = InitializeAABB(XMFLOAT3(-5.25f, -1.0f, 2.5f), XMFLOAT3(2, 3, 2));
       // m_aabbs[Spheres] = InitializeAABB(XMFLOAT3(-1.5f, 0, -1.5f), XMFLOAT3(3, 3, 3));
        //m_aabbs[offset + AABB] = InitializeAABB(XMFLOAT3(-100, -100, -100), XMFLOAT3(100, 100, 100));
        m_aabbs[offset + AABB] = InitializeAABB(XMFLOAT3(-5.25f, -2.55f, 2.5f), XMFLOAT3(2, 3, 2)); // this actually controls the AABB
        m_aabbs[offset + Spheres] = InitializeAABB(XMFLOAT3(-1.5f, 0, -1.5f), XMFLOAT3(3, 3, 3)); // this actually controls the Sphere
        m_aabbs[offset + Sun] = InitializeAABB(XMFLOAT3(-100, -100, -100), XMFLOAT3(200, 200, 200)); // this controls the sun
        //m_aabbs[Null] = InitializeAABB(XMFLOAT3(-100, -100, -100), XMFLOAT3(200, 200, 200));
        
    }

    AllocateUploadBuffer(device, m_aabbs.data(), m_aabbs.size()*sizeof(m_aabbs[0]), &m_aabbBuffer.resource);
}

//
void Room::BuildTriangleGeometry()
{
    auto device = m_deviceResources->GetD3DDevice();
    
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData room = geoGen.CreateRoom(10.0f, 5.0f, 10.0f);
    GeometryGenerator::MeshData coordinateSystem = geoGen.CreateCoordinates(20.0f, 0.01f, 0.01f);

    //
    // We are concatenating all the geometry into one big vertex/index buffer.  So
    // define the regions in the buffer each submesh covers.
    //

    // Cache the vertex offsets to each object in the concatenated vertex buffer.
    // This is basically an integer value you add to each index of such geometry.
    UINT roomVertexOffset = 0;
    UINT coordinateSystemVertexOffset = (UINT)room.Vertices.size();

    // Cache the starting index for each object in the concatenated index buffer.
    // This let's you distinguish between geometries in the buffers.
    UINT roomIndexOffset = 0;
    UINT coordinateSystemIndexOffset = (UINT)room.Indices32.size();

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

    //
    // Extract the vertex elements we are interested in and pack the
    // vertices of all the meshes into one vertex buffer.
    //

    auto totalVertexCount = room.Vertices.size() + coordinateSystem.Vertices.size();

    std::vector<Vertex> vertices(totalVertexCount);

    UINT k = 0;
    for (size_t i = 0; i < room.Vertices.size(); ++i, ++k)
    {
        vertices[k].position = room.Vertices[i].Position;
        vertices[k].normal = room.Vertices[i].Normal;
    }

    for (size_t i = 0; i < coordinateSystem.Vertices.size(); ++i, ++k)
    {
        vertices[k].position = coordinateSystem.Vertices[i].Position;
        vertices[k].normal = coordinateSystem.Vertices[i].Normal;
    }

    std::vector<std::uint16_t> indices;
    indices.insert(indices.end(), begin(room.GetIndices16()), end(room.GetIndices16()));
    //indices.insert(indices.end(), begin(coordinateSystem.GetIndices16()), end(coordinateSystem.GetIndices16()));

    for (int i = 0; i < coordinateSystem.GetIndices16().size(); i++)
    {
        indices.push_back(coordinateSystem.GetIndices16()[i] + coordinateSystemSubmesh.BaseVertexLocation);
    }


    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "triangleGeo";

    AllocateUploadBuffer(device, &indices[0], ibByteSize, &m_indexBuffer.resource);
    AllocateUploadBuffer(device, &vertices[0], vbByteSize, &m_vertexBuffer.resource);

    // Vertex buffer is passed to the shader along with index buffer as a descriptor range.
    UINT descriptorIndexIB = CreateBufferSRV(&m_indexBuffer, indices.size() / 2, 0);
    UINT descriptorIndexVB = CreateBufferSRV(&m_vertexBuffer, vertices.size(), sizeof(vertices[0]));

    geo->VertexBufferGPU = m_vertexBuffer.resource;
    geo->IndexBufferGPU = m_indexBuffer.resource;

    geo->VertexByteStride = sizeof(Vertex);
    geo->VertexBufferByteSize = vbByteSize;
    geo->IndexFormat = DXGI_FORMAT_R16_UINT;
    geo->IndexBufferByteSize = ibByteSize;

    geo->DrawArgs["room"] = roomSubmesh;
    geo->DrawArgs["coordinateSystem"] = coordinateSystemSubmesh;

    m_geometries[geo->Name] = std::move(geo);

    ThrowIfFalse(descriptorIndexVB == descriptorIndexIB + 1, L"Vertex Buffer descriptor index must follow that of Index Buffer descriptor index");


    //for (int i = 0; i < room.GetIndices16().size(); i++)
    //{
    //    m_indices.push_back(room.GetIndices16()[i] + roomSubmesh.BaseVertexLocation);
    //}

    //for (int i = 0; i < room.Vertices.size(); i++)
    //{
    //    m_vertices.push_back({ room.Vertices[i].Position, room.Vertices[i].Normal });
    //}

    //for (int i = 0; i < coordinateSystem.GetIndices16().size(); i++)
    //{
    //    m_indices.push_back(coordinateSystem.GetIndices16()[i] + coordinateSystemSubmesh.BaseVertexLocation);
    //}

    //for (int i = 0; i < coordinateSystem.Vertices.size(); i++)
    //{
    //    m_vertices.push_back({ coordinateSystem.Vertices[i].Position, coordinateSystem.Vertices[i].Normal });
    //}

    //AllocateUploadBuffer(device, &indices[0], ibByteSize, &m_indexBuffer.resource);
    //AllocateUploadBuffer(device, &vertices[0], vbByteSize, &m_vertexBuffer.resource);

    //// Vertex buffer is passed to the shader along with index buffer as a descriptor range.
    //UINT descriptorIndexIB = CreateBufferSRV(&m_indexBuffer, m_indices.size() / 2, 0);
    //UINT descriptorIndexVB = CreateBufferSRV(&m_vertexBuffer, m_vertices.size(), sizeof(m_vertices[0]));

    //ThrowIfFalse(descriptorIndexVB == descriptorIndexIB + 1, L"Vertex Buffer descriptor index must follow that of Index Buffer descriptor index");
}


//
// Build geometry used in the sample.
void Room::BuildGeometry()
{
    BuildProceduralGeometryAABBs();
    BuildTriangleGeometry();
}

//nas, nbottom, ntop
// Build geometry descs for bottom-level AS.
void Room::BuildGeometryDescsForBottomLevelAS(array<vector<D3D12_RAYTRACING_GEOMETRY_DESC>, BottomLevelASType::Count>& geometryDescs)
{
    // Mark the geometry as opaque. 
    // PERFORMANCE TIP: mark geometry as opaque whenever applicable as it can enable important ray processing optimizations.
    // Note: When rays encounter opaque geometry an any hit shader will not be executed whether it is present or not.
    D3D12_RAYTRACING_GEOMETRY_FLAGS geometryFlags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

    // Triangle geometry desc
    {
        D3D12_RAYTRACING_GEOMETRY_DESC triangleDescTemplate{};
        triangleDescTemplate.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;

        
        triangleDescTemplate.Triangles.IndexFormat = DXGI_FORMAT_R16_UINT;

        triangleDescTemplate.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
        triangleDescTemplate.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);

        triangleDescTemplate.Flags = geometryFlags;

        // Triangle bottom-level AS contains room geometry, and coordinates.
        geometryDescs[BottomLevelASType::Triangle].resize(TriangleGeometry::Count, triangleDescTemplate);
        

        // Create triangle geometries
        // Having separate geometries allows of separate shader record binding per geometry.
        // In this app, this lets us specify custom hit groups per triangle geometry.
        for (UINT i = 0; i < TriangleGeometry::Count; i++)
        {
            auto& geometryDesc = geometryDescs[BottomLevelASType::Triangle][i];
            //geometryDesc.Triangles.IndexBuffer = i==0 ? m_geometries["triangleGeo"]->DrawArgs["room"].StartIndexLocation : m_geometries["triangleGeo"]->DrawArgs["coordinateSystem"].StartIndexLocation;
            /*geometryDesc.Triangles.IndexBuffer = (D3D12_GPU_VIRTUAL_ADDRESS)m_geometries["triangleGeo"]->IndexBufferGPU.GetAddressOf();*/
            geometryDesc.Triangles.IndexBuffer = i == 0 ? m_indexBuffer.resource->GetGPUVirtualAddress() : m_indexBuffer.resource->GetGPUVirtualAddress() + m_geometries["triangleGeo"]->DrawArgs["room"].IndexCount * sizeof(Index);

            geometryDesc.Triangles.IndexCount = i == 0 ? m_geometries["triangleGeo"]->DrawArgs["room"].IndexCount : m_geometries["triangleGeo"]->DrawArgs["coordinateSystem"].IndexCount;
            /*geometryDesc.Triangles.IndexCount = static_cast<UINT>(m_indexBuffer.resource->GetDesc().Width) / sizeof(Index);*/

            /*geometryDesc.Triangles.VertexBuffer.StartAddress = i == 0 ? m_geometries["triangleGeo"]->DrawArgs["room"].BaseVertexLocation : m_geometries["triangleGeo"]->DrawArgs["coordinateSystem"].BaseVertexLocation;*/
            geometryDesc.Triangles.VertexBuffer.StartAddress = m_vertexBuffer.resource->GetGPUVirtualAddress();
            geometryDesc.Triangles.VertexCount = i == 0 ? m_geometries["triangleGeo"]->DrawArgs["room"].VertexCount : m_geometries["triangleGeo"]->DrawArgs["coordinateSystem"].VertexCount;
            /*geometryDesc.Triangles.VertexCount = static_cast<UINT>(m_vertexBuffer.resource->GetDesc().Width) / sizeof(Vertex);*/
        }
    }

    // AABB geometry desc
    {
        D3D12_RAYTRACING_GEOMETRY_DESC aabbDescTemplate = {};
        aabbDescTemplate.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS;
        // Each geometry contains only one AABB.
        aabbDescTemplate.AABBs.AABBCount = 1;
        aabbDescTemplate.AABBs.AABBs.StrideInBytes = sizeof(D3D12_RAYTRACING_AABB);
        aabbDescTemplate.Flags = geometryFlags;

        // One AABB primitive per geometry.
        geometryDescs[BottomLevelASType::AABB].resize(IntersectionShaderType::TotalPrimitiveCount, aabbDescTemplate);

        // Create AABB geometries. 
        // Having separate geometries allows of separate shader record binding per geometry.
        // In this sample, this lets us specify custom hit groups per AABB geometry.
        for (UINT i = 0; i < IntersectionShaderType::TotalPrimitiveCount; i++)
        {
            auto& geometryDesc = geometryDescs[BottomLevelASType::AABB][i];
            geometryDesc.AABBs.AABBs.StartAddress = m_aabbBuffer.resource->GetGPUVirtualAddress() + i * sizeof(D3D12_RAYTRACING_AABB);
        }
    }
}

//
AccelerationStructureBuffers Room::BuildBottomLevelAS(const vector<D3D12_RAYTRACING_GEOMETRY_DESC>& geometryDescs, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags)
{
    auto device = m_deviceResources->GetD3DDevice();
    auto commandList = m_deviceResources->GetCommandList();
    ComPtr<ID3D12Resource> scratch;
    ComPtr<ID3D12Resource> bottomLevelAS;

    // Get the size requirements for the scratch and AS buffers.
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS &bottomLevelInputs = bottomLevelBuildDesc.Inputs;
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

    // Allocate resources for acceleration structures.
    // Acceleration structures can only be placed in resources that are created in the default heap (or custom heap equivalent). 
    // Default heap is OK since the application doesn�t need CPU read/write access to them. 
    // The resources that will contain acceleration structures must be created in the state D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, 
    // and must have resource flag D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS. The ALLOW_UNORDERED_ACCESS requirement simply acknowledges both: 
    //  - the system will be doing this type of access in its implementation of acceleration structure builds behind the scenes.
    //  - from the app point of view, synchronization of writes/reads to acceleration structures is accomplished using UAV barriers.
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

//
template <class InstanceDescType, class BLASPtrType>
void Room::BuildBottomLevelASInstanceDescs(BLASPtrType *bottomLevelASaddresses, ComPtr<ID3D12Resource>* instanceDescsResource)
{
    // Transformations here are moving the whole bottom level acceleration structure in world space.

    auto device = m_deviceResources->GetD3DDevice();
    
    vector<InstanceDescType> instanceDescs;
    instanceDescs.resize(NUM_BLAS);

    // Width of a bottom-level AS geometry.
    // Make the room a little larger in all dimensions.
    const XMFLOAT3 fWidth = {1.0f, 1.0f, 1.0f};

    // Bottom-level AS with the room geometry.
    {
        auto& instanceDesc = instanceDescs[BottomLevelASType::Triangle];
        instanceDesc = {};
        instanceDesc.InstanceMask = 1;
        instanceDesc.InstanceContributionToHitGroupIndex = 0;
        instanceDesc.AccelerationStructure = bottomLevelASaddresses[BottomLevelASType::Triangle];

        // Calculate transformation matrix.
        auto position = XMFLOAT3(0.0f, 0.0f, 0.0f);
        const XMVECTOR vBasePosition = XMLoadFloat3(&fWidth) * XMLoadFloat3(&position);
        
        // Scale in all dimensions.
        XMMATRIX mScale = XMMatrixScaling(fWidth.x, fWidth.y, fWidth.z);
        XMMATRIX mTranslation = XMMatrixTranslationFromVector(vBasePosition);
        XMMATRIX mTransform = mScale * mTranslation;  
        XMStoreFloat3x4(reinterpret_cast<XMFLOAT3X4*>(instanceDesc.Transform), mTransform);
    }

    // Create instanced bottom-level AS with procedural geometry AABBs.
    // Instances share all the data, except for a transform.
    {
        auto& instanceDesc = instanceDescs[BottomLevelASType::AABB];
        instanceDesc = {};
        instanceDesc.InstanceMask = 1;
        
        // Set hit group offset to beyond the shader records for the triangle AABB.
        instanceDesc.InstanceContributionToHitGroupIndex = BottomLevelASType::AABB * RayType::Count;
        instanceDesc.AccelerationStructure = bottomLevelASaddresses[BottomLevelASType::AABB];

        // Move all AABBS above the ground plane.
        auto vec = XMFLOAT3(0, 0, 0);
        XMMATRIX mTranslation = XMMatrixTranslationFromVector(XMLoadFloat3(&vec));
        XMStoreFloat3x4(reinterpret_cast<XMFLOAT3X4*>(instanceDesc.Transform), mTranslation);
    }
    UINT64 bufferSize = static_cast<UINT64>(instanceDescs.size() * sizeof(instanceDescs[0]));
    AllocateUploadBuffer(device, instanceDescs.data(), bufferSize, &(*instanceDescsResource), L"InstanceDescs");
};

//
AccelerationStructureBuffers Room::BuildTopLevelAS(AccelerationStructureBuffers bottomLevelAS[BottomLevelASType::Count], D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags)
{
    auto device = m_deviceResources->GetD3DDevice();
    auto commandList = m_deviceResources->GetCommandList();
    ComPtr<ID3D12Resource> scratch;
    ComPtr<ID3D12Resource> topLevelAS;

    // Get required sizes for an acceleration structure.
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelBuildDesc = {};
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS &topLevelInputs = topLevelBuildDesc.Inputs;
    topLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    topLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    topLevelInputs.Flags = buildFlags;
    topLevelInputs.NumDescs = NUM_BLAS;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topLevelPrebuildInfo = {};
    m_dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&topLevelInputs, &topLevelPrebuildInfo);
    ThrowIfFalse(topLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

    AllocateUAVBuffer(device, topLevelPrebuildInfo.ScratchDataSizeInBytes, &scratch, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");

    // Allocate resources for acceleration structures.
    // Acceleration structures can only be placed in resources that are created in the default heap (or custom heap equivalent). 
    // Default heap is OK since the application doesn�t need CPU read/write access to them. 
    // The resources that will contain acceleration structures must be created in the state D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, 
    // and must have resource flag D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS. The ALLOW_UNORDERED_ACCESS requirement simply acknowledges both: 
    //  - the system will be doing this type of access in its implementation of acceleration structure builds behind the scenes.
    //  - from the app point of view, synchronization of writes/reads to acceleration structures is accomplished using UAV barriers.
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
            bottomLevelAS[1].accelerationStructure->GetGPUVirtualAddress()
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

//
// Build acceleration structure needed for raytracing.
void Room::BuildAccelerationStructures()
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
        {
            bottomLevelAS[i] = BuildBottomLevelAS(geometryDescs[i]);
        }
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

//nshadertable
// Build shader tables.
// This encapsulates all shader records - shaders and the arguments for their local root signatures.
void Room::BuildShaderTables()
{
    auto device = m_deviceResources->GetD3DDevice();

    void* rayGenShaderID;
    void* missShaderIDs[RayType::Count];
    void* hitGroupShaderIDs_TriangleGeometry[RayType::Count];
    void* hitGroupShaderIDs_AABBGeometry[IntersectionShaderType::Count][RayType::Count];

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
        for (UINT r = 0; r < IntersectionShaderType::Count; r++)
            for (UINT c = 0; c < RayType::Count; c++)        
            {
                hitGroupShaderIDs_AABBGeometry[r][c] = stateObjectProperties->GetShaderIdentifier(c_hitGroupNames_AABBGeometry[r][c]); 
                shaderIdToStringMap[hitGroupShaderIDs_AABBGeometry[r][c]] = c_hitGroupNames_AABBGeometry[r][c];
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

    /*************--------- Shader table layout -------*******************
    | --------------------------------------------------------------------
    | Shader table - HitGroupShaderTable: 
    | [0] : MyHitGroup_Triangle
    | [1] : MyHitGroup_Triangle_ShadowRay
    | [2] : MyHitGroup_AABB_AnalyticPrimitive
    | [3] : MyHitGroup_AABB_AnalyticPrimitive_ShadowRay 
    | ...
    | [6] : MyHitGroup_AABB_VolumetricPrimitive
    | [7] : MyHitGroup_AABB_VolumetricPrimitive_ShadowRay
    | [8] : MyHitGroup_AABB_SignedDistancePrimitive 
    | [9] : MyHitGroup_AABB_SignedDistancePrimitive_ShadowRay,
    | ...
    | [20] : MyHitGroup_AABB_SignedDistancePrimitive
    | [21] : MyHitGroup_AABB_SignedDistancePrimitive_ShadowRay
    | --------------------------------------------------------------------
    **********************************************************************/

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
        UINT numShaderRecords = TriangleGeometry::Count * RayType::Count + IntersectionShaderType::TotalPrimitiveCount * RayType::Count;
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

                // Ray types
                for (UINT r = 0; r < RayType::Count; r++)
                {
                    auto& hitGroupShaderID = hitGroupShaderIDs_TriangleGeometry[r];
                    hitGroupShaderTable.push_back(ShaderRecord(hitGroupShaderID, shaderIDSize, &rootArgs, sizeof(rootArgs)));
                }
            }
        }
      
        // AABB geometry hit groups.
        {
            LocalRootSignature::AABB::RootArguments rootArgs;
            UINT instanceIndex = 0;

            // Create a shader record for each primitive.
            for (UINT iShader = 0, instanceIndex = 0; iShader < IntersectionShaderType::Count; iShader++)
            {
                UINT numPrimitiveTypes = IntersectionShaderType::PerPrimitiveTypeCount(static_cast<IntersectionShaderType::Enum>(iShader));
                
                // Primitives for each intersection shader.
                for (UINT primitiveIndex = 0; primitiveIndex < numPrimitiveTypes; primitiveIndex++, instanceIndex++)
                {
                    rootArgs.materialCb = m_aabbMaterialCB[instanceIndex];
                    rootArgs.aabbCB.instanceIndex = instanceIndex;
                    rootArgs.aabbCB.primitiveType = primitiveIndex;
                    
                    // Ray types.
                    for (UINT r = 0; r < RayType::Count; r++)
                    {
                        auto& hitGroupShaderID = hitGroupShaderIDs_AABBGeometry[iShader][r];
                        hitGroupShaderTable.push_back(ShaderRecord(hitGroupShaderID, shaderIDSize, &rootArgs, sizeof(rootArgs)));
                    }
                }
            }
        }
        hitGroupShaderTable.DebugPrint(shaderIdToStringMap);
        m_hitGroupShaderTableStrideInBytes = hitGroupShaderTable.GetShaderRecordSize();
        m_hitGroupShaderTable = hitGroupShaderTable.GetResource();
    }
}

//ncontrols
void Room::OnKeyDown(UINT8 key)
{
    // rotation
    float elapsedTime = static_cast<float>(m_timer.GetElapsedSeconds());
    float secondsToRotateAround = 0.1f;
    float angleToRotateBy = -360.0f * (elapsedTime / secondsToRotateAround);
    const XMVECTOR& prevLightPosition = m_sceneCB->lightPosition;
    XMMATRIX rotate = XMMatrixRotationY(XMConvertToRadians(angleToRotateBy));
    XMMATRIX rotateClockwise = XMMatrixRotationY(XMConvertToRadians(-angleToRotateBy));

    // translation
    //auto frameIndex = m_deviceResources->GetCurrentFrameIndex();
    ////m_sceneCB->cameraPosition = m_eye;
    //float fovAngleY = 45.0f;
    //XMMATRIX view = XMMatrixLookAtLH(m_eye, m_at, m_up);
    //XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(fovAngleY), m_aspectRatio, 0.01f, 125.0f);
    //XMMATRIX viewProj = view * proj;
    ////m_sceneCB->projectionToWorld = XMMatrixInverse(nullptr, viewProj);

    //XMFLOAT3 forward;
    //XMStoreFloat3(&forward, m_at - m_eye);
    ////XMMATRIX viewForward = (0.0f, 0.0f )
    //XMMATRIX translate = XMMatrixTranslation(forward.x, forward.y, forward.z);
    //XMMATRIX negtranslate = XMMatrixTranslation(-forward.x, -forward.y, -forward.z);
    //XMMATRIX lefttranslate = XMMatrixTranslation(-1.0f, 0.0f, 0.0f);
    //XMMATRIX righttranslate = XMMatrixTranslation(1.0f, 0.0f, 0.0f);

    auto speed = 100.0f;
    if (GetKeyState(VK_SHIFT))
        speed *= 5;
    switch (key)
    {
    case 'W':
        /*m_eye = XMVector3Transform(m_eye, translate);
        
        m_at = XMVector3Transform(m_at, translate);
        UpdateCameraMatrices();*/
        
        m_camera.Walk(speed * elapsedTime);
        break;
    case 'S':
       /* m_eye = XMVector3Transform(m_eye, negtranslate);
       
        m_at = XMVector3Transform(m_at, negtranslate);
        UpdateCameraMatrices();*/
        m_camera.Walk(-speed * elapsedTime);
        break;
    case 'A':
       /* m_eye = XMVector3Transform(m_eye, lefttranslate);
        
        m_at = XMVector3Transform(m_at, lefttranslate);
        UpdateCameraMatrices();*/
        m_camera.Strafe(-speed * elapsedTime);
        break;
    case 'D':
        /*m_eye = XMVector3Transform(m_eye, righttranslate);
        
        m_at = XMVector3Transform(m_at, righttranslate);
        UpdateCameraMatrices();*/
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
        m_sceneCB->lightPosition += XMVECTOR{ 0.0f, 0.0f, 100.0f } * elapsedTime;
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

    }
    m_camera.UpdateViewMatrix();
    UpdateCameraMatrices();
}

void Room::OnMouseMove(int x, int y)
{
    /*auto frameIndex = m_deviceResources->GetCurrentFrameIndex();

    float fovAngleY = 45.0f;
    XMMATRIX view = XMMatrixLookAtLH(m_eye, m_at, m_up);
    XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(fovAngleY), m_aspectRatio, 0.01f, 125.0f);
    XMMATRIX viewProj = view * proj;
    m_sceneCB->projectionToWorld = XMMatrixInverse(nullptr, viewProj);

    UpdateCameraMatrices();*/

    //float elapsedTime = static_cast<float>(m_timer.GetElapsedSeconds());

    //XMMATRIX rotate = XMMatrixRotationY(elapsedTime * 5 * (x - m_lastMousePosition.x));
    ///*m_eye = XMVector3Transform(m_eye, rotate);*/
    //m_up = XMVector3Transform(m_up, rotate);
    //m_at = XMVector3Transform(m_at, rotate);

    //m_lastMousePosition.x = x;
    //m_lastMousePosition.y = y;
    //UpdateCameraMatrices();
    
    float dx = XMConvertToRadians(0.25f * static_cast<float>(x - m_lastMousePosition.x));
    float dy = XMConvertToRadians(0.25f * static_cast<float>(y - m_lastMousePosition.y));

    char msg[100];
    sprintf_s(msg, "%f, %f\n", dx, dy);
    OutputDebugStringA(msg);

    m_camera.RotateY(dx);
    m_camera.Pitch(dy);

  
    m_camera.UpdateViewMatrix();
    UpdateCameraMatrices();

    m_lastMousePosition.x = x;
    m_lastMousePosition.y = y;
}

void Room::OnLeftButtonDown(UINT x, UINT y)
{
     m_lastMousePosition = { static_cast<int>(x), static_cast<int>(y) };
}

// Update frame-based values.
void Room::OnUpdate()
{
    /*char msg[100];
    sprintf_s(msg, "%f, %f, %f\n", m_camera.GetPosition3f().x, m_camera.GetPosition3f().y, m_camera.GetPosition3f().z);
    OutputDebugStringA(msg);*/
    m_timer.Tick();
    CalculateFrameStats();
    float elapsedTime = static_cast<float>(m_timer.GetElapsedSeconds());
    auto frameIndex = m_deviceResources->GetCurrentFrameIndex();
    auto prevFrameIndex = m_deviceResources->GetPreviousFrameIndex();

    // Rotate the camera around Y axis.
    if (m_animateCamera)
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
    UpdateAABBPrimitiveAttributes(m_animateGeometryTime);
    UpdateTrianglePrimitiveAttributes(m_animateGeometryTime);
    m_sceneCB->elapsedTime = m_animateGeometryTime;
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
        descriptorSetCommandList->SetDescriptorHeaps(1, m_descriptorHeap.GetAddressOf());
        // Set index and successive vertex buffer decriptor tables.
        commandList->SetComputeRootDescriptorTable(GlobalRootSignature::Slot::VertexBuffers, m_indexBuffer.gpuDescriptorHandle);
        commandList->SetComputeRootDescriptorTable(GlobalRootSignature::Slot::OutputView, m_raytracingOutputResourceUAVGpuDescriptor);
    };

    commandList->SetComputeRootSignature(m_raytracingGlobalRootSignature.Get());

    // Copy dynamic buffers to GPU.
    {
        m_sceneCB.CopyStagingToGpu(frameIndex);
        commandList->SetComputeRootConstantBufferView(GlobalRootSignature::Slot::SceneConstant, m_sceneCB.GpuVirtualAddress(frameIndex));

        m_aabbPrimitiveAttributeBuffer.CopyStagingToGpu(frameIndex);
        commandList->SetComputeRootShaderResourceView(GlobalRootSignature::Slot::AABBattributeBuffer, m_aabbPrimitiveAttributeBuffer.GpuVirtualAddress(frameIndex));

        m_trianglePrimitiveAttributeBuffer.CopyStagingToGpu(frameIndex);
        commandList->SetComputeRootShaderResourceView(GlobalRootSignature::Slot::TriangleAttributeBuffer, m_trianglePrimitiveAttributeBuffer.GpuVirtualAddress(frameIndex));
    }

    // Bind the heaps, acceleration structure and dispatch rays.  
    D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
    SetCommonPipelineState(commandList);
    commandList->SetComputeRootShaderResourceView(GlobalRootSignature::Slot::AccelerationStructure, m_topLevelAS->GetGPUVirtualAddress());
    DispatchRays(m_dxrCommandList.Get(), m_dxrStateObject.Get(), &dispatchDesc);
}

// Update the application state with the new resolution.
void Room::UpdateForSizeChange(UINT width, UINT height)
{
    DXSample::UpdateForSizeChange(width, height);
}

// Copy the raytracing output to the backbuffer.
void Room::CopyRaytracingOutputToBackbuffer()
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

// Create resources that are dependent on the size of the main window.
void Room::CreateWindowSizeDependentResources()
{
    CreateRaytracingOutputResource();
    UpdateCameraMatrices();
}

// Release resources that are dependent on the size of the main window.
void Room::ReleaseWindowSizeDependentResources()
{
    m_raytracingOutput.Reset();
}

// Release all resources that depend on the device.
void Room::ReleaseDeviceDependentResources()
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

    m_descriptorHeap.Reset();
    m_descriptorsAllocated = 0;
    m_sceneCB.Release();
    m_aabbPrimitiveAttributeBuffer.Release();
    m_trianglePrimitiveAttributeBuffer.Release();
    m_indexBuffer.resource.Reset();
    m_vertexBuffer.resource.Reset();
    m_aabbBuffer.resource.Reset();

    ResetComPtrArray(&m_bottomLevelAS);
    m_topLevelAS.Reset();

    m_raytracingOutput.Reset();
    m_raytracingOutputResourceUAVDescriptorHeapIndex = UINT_MAX;
    m_rayGenShaderTable.Reset();
    m_missShaderTable.Reset();
    m_hitGroupShaderTable.Reset();
}

void Room::RecreateD3D()
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

// Render the scene.
void Room::OnRender()
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

    m_denoiser.Run(m_pathtracer/*, m_RTEffects*/);

    CopyRaytracingOutputToBackbuffer();

    // End frame.
    for (auto& gpuTimer : m_gpuTimers)
    {
        gpuTimer.EndFrame(commandList);
    }

    m_deviceResources->Present(D3D12_RESOURCE_STATE_PRESENT);
}

void Room::OnDestroy()
{
    // Let GPU finish before releasing D3D resources.
    m_deviceResources->WaitForGpu();
    OnDeviceLost();
}

// Release all device dependent resouces when a device is lost.
void Room::OnDeviceLost()
{
    ReleaseWindowSizeDependentResources();
    ReleaseDeviceDependentResources();
}

// Create all device dependent resources when a device is restored.
void Room::OnDeviceRestored()
{
    CreateDeviceDependentResources();
    CreateWindowSizeDependentResources();
}

// Compute the average frames per second and million rays per second.
void Room::CalculateFrameStats()
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
            << L"    Ray throughput: " << MRaysPerSecond <<" MRPS"
            << L"    GPU[" << m_deviceResources->GetAdapterID() << L"]: " << m_deviceResources->GetAdapterDescription();
        SetCustomWindowText(windowText.str().c_str());
    }
}

// Handle OnSizeChanged message event.
void Room::OnSizeChanged(UINT width, UINT height, bool minimized)
{
    if (!m_deviceResources->WindowSizeChanged(width, height, minimized))
    {
        return;
    }

    UpdateForSizeChange(width, height);

    ReleaseWindowSizeDependentResources();
    CreateWindowSizeDependentResources();
}

// Allocate a descriptor and return its index. 
// If the passed descriptorIndexToUse is valid, it will be used instead of allocating a new one.
UINT Room::AllocateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* cpuDescriptor, UINT descriptorIndexToUse)
{
    auto descriptorHeapCpuBase = m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    if (descriptorIndexToUse >= m_descriptorHeap->GetDesc().NumDescriptors)
    {
        ThrowIfFalse(m_descriptorsAllocated < m_descriptorHeap->GetDesc().NumDescriptors, L"Ran out of descriptors on the heap!" );
        descriptorIndexToUse = m_descriptorsAllocated++;
    }
    *cpuDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(descriptorHeapCpuBase, descriptorIndexToUse, m_descriptorSize);
    return descriptorIndexToUse;
}

// Create a SRV for a buffer.
UINT Room::CreateBufferSRV(D3DBuffer* buffer, UINT numElements, UINT elementSize)
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
