#ifndef RAYTRACING_HLSL
#define RAYTRACING_HLSL

#define HLSL
#include "RaytracingHlslCompat.h"
#include "RaytracingShaderHelper.hlsli"
#include "RandomNumberGenerator.hlsli"
#include "BxDF.hlsli"

static const float LIGHT_SIZE = 0.6f;

//
// Resources
//

RaytracingAccelerationStructure g_scene : register(t0);
RWTexture2D<float3> g_renderTarget : register(u0);
ConstantBuffer<SceneConstantBuffer> g_sceneCB : register(b0);

StructuredBuffer<Index> l_indices : register(t1);
StructuredBuffer<Vertex> l_vertices : register(t2);
ConstantBuffer<PrimitiveConstantBuffer> l_materialCB : register(b1);

//***************************************************************************
//*****------ TraceRay wrappers for radiance and shadow rays. -------********
//***************************************************************************

// Trace a radiance ray into the scene and returns a shaded color.
float3 TraceRadianceRay(in Ray ray, in UINT currentRayRecursionDepth)
{
    if (currentRayRecursionDepth >= MAX_RAY_RECURSION_DEPTH)
    {
        return float4(0, 0, 0, 0);
    }

    // Set the ray's extents.
    RayDesc rayDesc;
    rayDesc.Origin = ray.origin;
    rayDesc.Direction = ray.direction;
    // Set TMin to a zero value to avoid aliasing artifacts along contact areas.
    // Note: make sure to enable face culling so as to avoid surface face fighting.
    rayDesc.TMin = 0;
    rayDesc.TMax = 10000;
    RayPayload rayPayload = { float3(0, 0, 0), currentRayRecursionDepth + 1 };
    TraceRay(g_scene,
        RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
        TraceRayParameters::InstanceMask,
        TraceRayParameters::HitGroup::Offset[RayType::Radiance],
        TraceRayParameters::HitGroup::GeometryStride,
        TraceRayParameters::MissShader::Offset[RayType::Radiance],
        rayDesc, rayPayload);

    return rayPayload.color;
}

// Trace a shadow ray and return true if it hits any geometry.
bool TraceShadowRayAndReportIfHit(in Ray ray, in UINT currentRayRecursionDepth)
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

    return shadowPayload.hit;
}


//nshade
float3 Shade(
    float3 hitPosition,
    RayPayload rayPayload,
    float3 N)
{
    float3 V = normalize(g_sceneCB.cameraPosition.xyz - hitPosition);
    float3 indirectContribution = 0;
    float3 L = 0;
  
    const float3 Kd = l_materialCB.diffuseCoef;
    const float3 Ks = l_materialCB.specularCoef;
    const float3 Kr = l_materialCB.reflectanceCoef;
    /*const float3 Kt = material.Kt;*/
    const float roughness = l_materialCB.specularPower;

    //
    // DIRECT ILLUMINATION
    // 
    if (!BxDF::IsBlack(l_materialCB.diffuseCoef) || !BxDF::IsBlack(l_materialCB.specularCoef))
    {
        //
        // Shadow component
        //

        // Compute coordinate system for sphere sampling. [PBR, 14.2.2] 
        Ray shadowRay = { hitPosition, g_sceneCB.lightPosition.xyz - hitPosition };
        // Trace a shadow ray.
        bool shadowRayHit = TraceShadowRayAndReportIfHit(shadowRay, rayPayload.recursionDepth);

        //
        // Diffuse and specular
        //
        float3 wi = normalize(g_sceneCB.lightPosition.xyz - hitPosition);
        
        L += BxDF::DirectLighting::Shade(l_materialCB.albedo, N, wi, V, shadowRayHit, g_sceneCB, l_materialCB.diffuseCoef, l_materialCB.specularCoef, l_materialCB.specularPower);
    }
    //
    // INDIRECT ILLUMINATION
    //

    //
    // Ambient
    //
    
    L += g_sceneCB.lightAmbientColor;

    //
    // Reflected component
    //

    bool isReflective = !BxDF::IsBlack(float3(l_materialCB.reflectanceCoef, 0, 0));
    //bool isTransmissive = !BxDF::IsBlack(Kt);

    // Handle cases where ray is coming from behind due to imprecision,
    // don't cast reflection rays in that case.
    float smallValue = 1e-6f;
    isReflective = dot(V, N) > smallValue ? isReflective : false;

    //if (isReflective /*|| isTransmissive*/)
    {
        //if (isReflective
        //    && (BxDF::Specular::Reflection::IsTotalInternalReflection(V, N)
        //        /*|| material.type == MaterialType::Mirror*/))
        //{
        //    RayPayload reflectedRayPayLoad = rayPayload;
        //    //float3 wi = reflect(-V, N);
        //    Ray reflectionRay = { HitWorldPosition(), reflect(WorldRayDirection(), N) };
        //    float3 fresnelR = FresnelReflectanceSchlick(WorldRayDirection(), N, l_materialCB.albedo.xyz);
        //    // Trace a reflection ray.
        //    L += Kr * TraceRadianceRay(reflectionRay, rayPayload.recursionDepth) * fresnelR;
        //    //L += Kr * TraceReflectedGBufferRay(hitPosition, wi, N, objectNormal, reflectedRayPayLoad);
        //   // UpdateAOGBufferOnLargerDiffuseComponent(rayPayload, reflectedRayPayLoad, Kr);
        //}
        //else // No total internal reflection
        {
            float3 Fo = Ks;
            //if (isReflective)
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

                //float dist = length(ray.direction) - LIGHT_SIZE - 0.01;

                float4x4 objectToWorld = BuildInverseLookAtMatrix(hitPosition, normalize(wi));


                RayPayload reflectedRayPayLoad = rayPayload;
                // Ref: eq 24.4, [Ray-tracing from the Ground Up]
                Ray reflectionRay = { HitWorldPosition(), normalize(Disk::Sample(noiseUV, l_materialCB.specularPower, 10, objectToWorld)) };
                float3 fresnelR = FresnelReflectanceSchlick(WorldRayDirection(), N, l_materialCB.albedo.xyz);
                // Trace a reflection ray.
                L += Fr * TraceRadianceRay(reflectionRay, rayPayload.recursionDepth); // TraceReflectedGBufferRay(hitPosition, wi, N, objectNormal, reflectedRayPayLoad);
                //UpdateAOGBufferOnLargerDiffuseComponent(rayPayload, reflectedRayPayLoad, Fr);
            }

            //if (isTransmissive)
            //{
            //    // Radiance contribution from refraction.
            //    float3 wt;
            //    float3 Ft = Kt * BxDF::Specular::Transmission::Sample_Ft(V, wt, N, Fo);    // Calculates wt

            //    PathtracerRayPayload refractedRayPayLoad = rayPayload;

            //    L += Ft * TraceRefractedGBufferRay(hitPosition, wt, N, objectNormal, refractedRayPayLoad);
            //    UpdateAOGBufferOnLargerDiffuseComponent(rayPayload, refractedRayPayLoad, Ft);
            //}
        }
    }

    return l_materialCB.shaded == 1.0f ? L : l_materialCB.albedo;
}

//***************************************************************************
//********************------ Ray gen shader.. -------************************
//***************************************************************************

[shader("raygeneration")]
void MyRaygenShader()
{
    // Generate a ray for a camera pixel corresponding to an index from the dispatched 2D grid.
    Ray ray = GenerateCameraRay(DispatchRaysIndex().xy, g_sceneCB.cameraPosition.xyz, g_sceneCB.projectionToWorld);
 
    // Cast a ray into the scene and retrieve a shaded color.
    UINT currentRecursionDepth = 0;
    float3 color = TraceRadianceRay(ray, currentRecursionDepth);

    // Write the raytraced color to the output texture.
    g_renderTarget[DispatchRaysIndex().xy] = color;
}

//***************************************************************************
//******************------ Closest hit shaders -------***********************
//***************************************************************************

[shader("closesthit")]
void MyClosestHitShader_Triangle(inout RayPayload rayPayload, in BuiltInTriangleIntersectionAttributes attr)
{
    uint startIndex = PrimitiveIndex() * 3;
    const uint3 indices = { l_indices[startIndex], l_indices[startIndex + 1], l_indices[startIndex + 2] };

    // Retrieve corresponding vertex normals for the triangle vertices.
    float3 vertexNormals[3] = {
        l_vertices[indices[0]].normal,
        l_vertices[indices[1]].normal,
        l_vertices[indices[2]].normal
    };

    float3 triangleNormal = HitAttribute(vertexNormals, attr);

    float3 hitPosition = HitWorldPosition();

    rayPayload.color = Shade(hitPosition, rayPayload, triangleNormal);
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
}

#endif // RAYTRACING_HLSL