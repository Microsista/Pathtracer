#ifndef RAYTRACING_HLSL
#define RAYTRACING_HLSL

#define HLSL
#include "RaytracingHlslCompat.h"
#include "RaytracingShaderHelper.hlsli"
#include "RandomNumberGenerator.hlsli"
#include "BxDF.hlsli"

static const float LIGHT_SIZE = 0.6f;

struct Info {
    float4 color;
    float inShadow;
};


//
// Resources
//

// Global
RaytracingAccelerationStructure g_scene : register(t0);
RWTexture2D<float3> g_renderTarget : register(u0);
RWTexture2D<float3> g_reflectionBuffer : register(u1);
RWTexture2D<float3> g_shadowBuffer : register(u2);

ConstantBuffer<SceneConstantBuffer> g_sceneCB : register(b0);

SamplerState LinearWrapSampler : register(s0);

//RWTexture2D<float3> GBUFFER_POSITION : register(u1);

// Local
StructuredBuffer<Index> l_indices : register(t1);
StructuredBuffer<VertexPositionNormalTextureTangent> l_vertices : register(t2);
ConstantBuffer<PrimitiveConstantBuffer> l_materialCB : register(b1);
Texture2D<float3> l_texDiffuse : register(t3);

//***************************************************************************
//*****------ TraceRay wrappers for radiance and shadow rays. -------********
//***************************************************************************

// Trace a radiance ray into the scene and returns a shaded color.
Info TraceRadianceRay(in Ray ray, in UINT currentRayRecursionDepth)
{
    if (currentRayRecursionDepth >= MAX_RAY_RECURSION_DEPTH)
    {
        Info info3;
        info3.color = float4(0, 0, 0, 0);
        info3.inShadow = 0.0f;
        return info3;
    }

    // Set the ray's extents.
    RayDesc rayDesc;
    rayDesc.Origin = ray.origin;
    rayDesc.Direction = ray.direction;
    // Set TMin to a zero value to avoid aliasing artifacts along contact areas.
    // Note: make sure to enable face culling so as to avoid surface face fighting.
    rayDesc.TMin = 0;
    rayDesc.TMax = 10000;
    RayPayload rayPayload = { float4(0, 0, 0, 0), currentRayRecursionDepth + 1, 0.0f };
    TraceRay(g_scene,
        RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
        TraceRayParameters::InstanceMask,
        TraceRayParameters::HitGroup::Offset[RayType::Radiance],
        TraceRayParameters::HitGroup::GeometryStride,
        TraceRayParameters::MissShader::Offset[RayType::Radiance],
        rayDesc, rayPayload);
    Info info;
    info.color = rayPayload.color;
    info.inShadow = rayPayload.inShadow;
    return info;
}

// Trace a shadow ray and return true if it hits any geometry.
bool TraceShadowRayAndReportIfHit(in Ray ray, in UINT currentRayRecursionDepth, float3 N)
{
    if (currentRayRecursionDepth >= MAX_RAY_RECURSION_DEPTH)
    {
        return false;
    }

    // Soft shadows
    uint threadId = DispatchRaysIndex().x + DispatchRaysIndex().y * DispatchRaysDimensions().x;
    uint RNGState = RNG::SeedThread(threadId);
    float2 noiseUV = {
        2 * RNG::Random01(RNGState) - 1,            // [0, 1) -> [-1, 1)
        2 * RNG::Random01(RNGState) - 1
    };

    float distToLight = length(ray.direction) - LIGHT_SIZE - 0.01;
    
    float4x4 objectToWorld = BuildInverseLookAtMatrix(ray.origin, normalize(ray.direction));

    // Set the ray's extents.
    RayDesc rayDesc;
    rayDesc.Origin = ray.origin;
    //rayDesc.Direction = UniformSampleHemisphere(noiseUV);//normalize(ray.direction);
    rayDesc.Direction = normalize(Disk::Sample(noiseUV, LIGHT_SIZE, length(ray.direction), objectToWorld));
    //rayDesc.Direction = Sphere::Sample(noiseUV, LIGHT_SIZE/distToLight, objectToWorld);
    // Set TMin to a zero value to avoid aliasing artifcats along contact areas.
    // Note: make sure to enable back-face culling so as to avoid surface face fighting.
    rayDesc.TMin = 0.001;
    rayDesc.TMax = distToLight;

    // Initialize shadow ray payload.
    // Set the initial value to true since closest and any hit shaders are skipped. 
    // Shadow miss shader, if called, will set it to false.
    ShadowRayPayload shadowPayload = { true };
   /* if (dot(ray.direction, N) > 0) {*/
        TraceRay(g_scene,
            RAY_FLAG_CULL_BACK_FACING_TRIANGLES
            | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH
            | RAY_FLAG_FORCE_OPAQUE             // ~skip any hit shaders
            | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, // ~skip closest hit shaders,
            TraceRayParameters::InstanceMask,
            TraceRayParameters::HitGroup::Offset[RayType::Shadow],
            TraceRayParameters::HitGroup::GeometryStride,
            TraceRayParameters::MissShader::Offset[RayType::Shadow],
            rayDesc, shadowPayload);
    //}

    return shadowPayload.hit;
}

//nshade
Info Shade(
    float3 hitPosition,
    RayPayload rayPayload,
    float3 N,
    in PrimitiveMaterialBuffer material)
{
    float3 V = normalize(g_sceneCB.cameraPosition.xyz - hitPosition);
    float3 indirectContribution = 0;
    float4 L = 0;
  
    const float3 Kd = material.Kd;
    const float3 Ks = material.Ks;
    const float3 Kr = material.Kr;
    /*const float3 Kt = material.Kt;*/
    const float roughness = material.roughness;

    //
    // DIRECT ILLUMINATION
    // 

    // Compute coordinate system for sphere sampling. [PBR, 14.2.2] 
    Ray shadowRay = { hitPosition, g_sceneCB.lightPosition.xyz - hitPosition };
    // Trace a shadow ray.
    bool shadowRayHit = TraceShadowRayAndReportIfHit(shadowRay, rayPayload.recursionDepth, N);
    uint2 DTID = DispatchRaysIndex().xy;
    //
    // Diffuse and specular
    //
    float3 wi = normalize(g_sceneCB.lightPosition.xyz - hitPosition);
    L += float4(BxDF::DirectLighting::Shade(Kd, N, wi, V, false, g_sceneCB, Kd, Ks, roughness), 0.0f);
    if (shadowRayHit)
        L += float4(0.0f, 0.0f, 0.0f, 1.0f);
    else
        L += float4(0.0f, 0.0f, 0.0f, 0.0f);

    //
    // INDIRECT ILLUMINATION
    //

    //
    // Ambient
    //
    
    //L += g_sceneCB.lightAmbientColor;

    //
    // Reflected component
    //

    bool isReflective = !BxDF::IsBlack(float3(l_materialCB.reflectanceCoef, 0, 0));
    //bool isTransmissive = !BxDF::IsBlack(Kt);

    // Handle cases where ray is coming from behind due to imprecision,
    // don't cast reflection rays in that case.
    float smallValue = 1e-6f;
    isReflective = dot(V, N) > smallValue ? isReflective : false;
        
    float3 Fo = Ks;
    {
        // Radiance contribution from reflection.
        float3 wi;
        float3 Fr = Kr * BxDF::Specular::Reflection::Sample_Fr(V, wi, N, Fo);    // Calculates wi

        // Fuzzy reflections
        uint threadId = DispatchRaysIndex().x + DispatchRaysIndex().y * DispatchRaysDimensions().x;
        uint RNGState = RNG::SeedThread(threadId);
        float2 noiseUV = {
            2 * RNG::Random01(RNGState) - 1,            // [0, 1) -> [-1, 1)
            2 * RNG::Random01(RNGState) - 1
        };

        float4x4 objectToWorld = BuildInverseLookAtMatrix(hitPosition, normalize(wi));


        RayPayload reflectedRayPayLoad = rayPayload;
        // Ref: eq 24.4, [Ray-tracing from the Ground Up]
        Ray reflectionRay = { HitWorldPosition(), normalize(Disk::Sample(noiseUV, roughness, 10, objectToWorld)) };
        float3 fresnelR = FresnelReflectanceSchlick(WorldRayDirection(), N, Kd.xyz);

        Info info2 = TraceRadianceRay(reflectionRay, rayPayload.recursionDepth);
        // Trace a reflection ray.
        g_reflectionBuffer[DTID] = Fr* info2.color; // TraceReflectedGBufferRay(hitPosition, wi, N, objectNormal, reflectedRayPayLoad);
        //UpdateAOGBufferOnLargerDiffuseComponent(rayPayload, reflectedRayPayLoad, Fr);
    }
    Info info;
    info.color = L;
    info.inShadow = shadowRayHit;

    Info info2;
    info2.color = float4(l_materialCB.albedo.x, l_materialCB.albedo.y, l_materialCB.albedo.z, 0.0f);
    info2.inShadow = shadowRayHit;
    if (l_materialCB.shaded)
        return info;
    else
        return info2;
    //return  == 1.0f ? info : info2;
}

[shader("raygeneration")]
void MyRaygenShader()
{

    uint2 DTID = DispatchRaysIndex().xy;
    // Generate a ray for a camera pixel corresponding to an index from the dispatched 2D grid.
    Ray ray = GenerateCameraRay(DispatchRaysIndex().xy, g_sceneCB.cameraPosition.xyz, g_sceneCB.projectionToWorld);

    // Cast a ray into the scene and retrieve a shaded color.
    UINT currentRecursionDepth = 0;
    g_shadowBuffer[DTID] = float3(0, 0, 0);
    Info info = TraceRadianceRay(ray, currentRecursionDepth);

    float3 inShadow = info.inShadow;
    float3 color = info.color;
    g_shadowBuffer[DTID] = inShadow;
    g_renderTarget[DTID] = color;
}


//***************************************************************************
//******************------ Closest hit shaders -------***********************
//***************************************************************************

[shader("closesthit")]
void MyClosestHitShader_Triangle(inout RayPayload rayPayload, in BuiltInTriangleIntersectionAttributes attr)
{
    PrimitiveMaterialBuffer material;
    material.Kd = l_materialCB.albedo.xyz;
    material.Ks = float3(l_materialCB.metalness, l_materialCB.metalness, l_materialCB.metalness);
    material.Kr = float3(l_materialCB.reflectanceCoef, l_materialCB.reflectanceCoef, l_materialCB.reflectanceCoef);
    /*const float3 Kt = material.Kt;*/
    material.roughness = l_materialCB.roughness;

    uint startIndex = PrimitiveIndex() * 3;
    const uint3 indices = { l_indices[startIndex], l_indices[startIndex + 1], l_indices[startIndex + 2] };

    // Retrieve corresponding vertex normals for the triangle vertices.
    VertexPositionNormalTextureTangent vertices[3] = {
        l_vertices[indices[0]],
        l_vertices[indices[1]],
        l_vertices[indices[2]]
    };

    float3 normals[3] = { vertices[0].normal, vertices[1].normal, vertices[2].normal };
    float3 localNormal = HitAttribute(normals, attr);

    float orientation = HitKind() == HIT_KIND_TRIANGLE_FRONT_FACE ? 1 : -1;
    localNormal *= orientation;

    float2 vertexTexCoords[3] = { vertices[0].textureCoordinate, vertices[1].textureCoordinate, vertices[2].textureCoordinate };
    float2 texCoord = HitAttribute(vertexTexCoords, attr);

    float3 texSample = l_texDiffuse.SampleLevel(LinearWrapSampler, texCoord, 0).xyz;
    material.Kd = texSample;
    float3 hitPosition = HitWorldPosition();
    Info info = Shade(hitPosition, rayPayload, localNormal, material);
    rayPayload.color = info.color;
    rayPayload.inShadow = info.inShadow;
}

//***************************************************************************
//**********************------ Miss shaders -------**************************
//***************************************************************************

[shader("miss")]
void MyMissShader(inout RayPayload rayPayload)
{
    float4 backgroundColor = float4(BackgroundColor);
    rayPayload.color = backgroundColor;
}

[shader("miss")]
void MyMissShader_ShadowRay(inout ShadowRayPayload rayPayload)
{
    rayPayload.hit = false;
    uint2 DTID = DispatchRaysIndex().xy;
}

#endif // RAYTRACING_HLSL