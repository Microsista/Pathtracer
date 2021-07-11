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
    float depth;
};


//
// Resources
//

// Global
RaytracingAccelerationStructure g_scene : register(t0);
RWTexture2D<float3> g_renderTarget : register(u0);
RWTexture2D<float3> g_reflectionBuffer : register(u1);
RWTexture2D<float3> g_shadowBuffer : register(u2);
RWTexture2D<float3> g_normalDepth : register(u3);
Texture2D<float3> g_normalMap : register(t5);
Texture2D<float3> g_specularMap : register(t6);
Texture2D<float3> g_emissiveMap : register(t7);

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
        info3.inShadow = 1.0f;
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
    RayPayload rayPayload = { float4(0, 0, 0, 0), currentRayRecursionDepth + 1, 1.0f, 1.0f, float3(0, 0, 0) };
    TraceRay(g_scene,
        RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
        TraceRayParameters::InstanceMask,
        TraceRayParameters::HitGroup::Offset[RayType::Radiance],
        TraceRayParameters::HitGroup::GeometryStride,
        TraceRayParameters::MissShader::Offset[RayType::Radiance],
        rayDesc, rayPayload);
    

    float depth = length(g_sceneCB.lightPosition - rayPayload.prevHitPosition) / 200;
    float c1 = 3.0f, c2 = 3.0f;
    float attenuation = 1.0f / (1.0f + c1 * depth + c2 * depth * depth);

    Info info;
    info.color = rayPayload.color * attenuation;
    info.inShadow = rayPayload.inShadow;
    info.depth = rayPayload.depth;
    return info;
}
//
//Info TraceReflectionRay(in Ray ray, in UINT currentRayRecursionDepth, in float3 hitPosition)
//{
//    if (currentRayRecursionDepth >= MAX_RAY_RECURSION_DEPTH)
//    {
//        Info info3;
//        info3.color = float4(0, 0, 0, 0);
//        info3.inShadow = 1.0f;
//        return info3;
//    }
//
//    // Set the ray's extents.
//    RayDesc rayDesc;
//    rayDesc.Origin = ray.origin;
//    rayDesc.Direction = ray.direction;
//    // Set TMin to a zero value to avoid aliasing artifacts along contact areas.
//    // Note: make sure to enable face culling so as to avoid surface face fighting.
//    rayDesc.TMin = 0;
//    rayDesc.TMax = 10000;
//    RayPayload rayPayload = { float4(0, 0, 0, 0), currentRayRecursionDepth + 1, 1.0f, 1.0f/*, hitPosition*/};
//    TraceRay(g_scene,
//        RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
//        TraceRayParameters::InstanceMask,
//        TraceRayParameters::HitGroup::Offset[RayType::Radiance],
//        TraceRayParameters::HitGroup::GeometryStride,
//        TraceRayParameters::MissShader::Offset[RayType::Radiance],
//        rayDesc, rayPayload);
//    Info info;
//    info.color = rayPayload.color;
//    info.inShadow = rayPayload.inShadow;
//    info.depth = rayPayload.depth;
//    return info;
//}

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
    const float3 Ke = material.Ke;
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


        RayPayload reflectedRayPayload = rayPayload;
        // Ref: eq 24.4, [Ray-tracing from the Ground Up]
        Ray reflectionRay = { HitWorldPosition(), normalize(Disk::Sample(noiseUV, roughness, 10, objectToWorld)) };
        float3 fresnelR = FresnelReflectanceSchlick(WorldRayDirection(), N, Ks);

        Info info2 = TraceRadianceRay(reflectionRay, rayPayload.recursionDepth/*, hitPosition*/);
        // Trace a reflection ray.
        g_reflectionBuffer[DTID] = Fr * info2.color; // TraceReflectedGBufferRay(hitPosition, wi, N, objectNormal, reflectedRayPayLoad);
        //UpdateAOGBufferOnLargerDiffuseComponent(rayPayload, reflectedRayPayLoad, Fr);
    }

    L += float4(Ke.x, Ke.y, Ke.z, 1.0f);
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





#endif // RAYTRACING_HLSL