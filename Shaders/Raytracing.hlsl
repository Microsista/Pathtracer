#ifndef RAYTRACING_HLSL
#define RAYTRACING_HLSL

#define HLSL
#include "RaytracingHlslCompat.h"
#include "ProceduralGeometry/ProceduralPrimitivesLibrary.hlsli"
#include "RaytracingShaderHelper.hlsli"
#include "RandomNumberGenerator.hlsli"
#include "BxDF.hlsli"

//***************************************************************************
//*****------ Shader resources bound via root signatures -------*************
//***************************************************************************

// Scene wide resources.
//  g_* - bound via a global root signature.
//  l_* - bound via a local root signature.
RaytracingAccelerationStructure g_scene : register(t0, space0);
RWTexture2D<float3> g_renderTarget : register(u0);
ConstantBuffer<SceneConstantBuffer> g_sceneCB : register(b0);

// Triangle resources
ByteAddressBuffer g_indices : register(t1, space0);
StructuredBuffer<Vertex> g_vertices : register(t2, space0);

// Procedural geometry resources
StructuredBuffer<PrimitiveInstancePerFrameBuffer> g_AABBPrimitiveAttributes : register(t3, space0);
StructuredBuffer<PrimitiveInstancePerFrameBuffer> g_trianglePrimitiveAttributes : register(t4, space0);
ConstantBuffer<PrimitiveConstantBuffer> l_materialCB : register(b1);
ConstantBuffer<PrimitiveInstanceConstantBuffer> l_aabbCB: register(b2);
ConstantBuffer<PrimitiveInstanceConstantBuffer> l_triangleCB: register(b3);

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

    // Calculate final color.
    float3 color = l_materialCB.shaded == 1.0f ? L : l_materialCB.albedo;

    // Apply visibility falloff.
    float t = RayTCurrent();
    return lerp(color, BackgroundColor, 1.0 - exp(-0.000002 * t * t * t));
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
    // Get the base index of the triangle's first 16 bit index.
    uint indexSizeInBytes = 2;
    uint indicesPerTriangle = 3;
    uint triangleIndexStride = indicesPerTriangle * indexSizeInBytes;
    uint baseIndex = PrimitiveIndex() * triangleIndexStride;

    // Load up three 16 bit indices for the triangle.
    const uint3 indices = Load3x16BitIndices(baseIndex, g_indices);

    // Retrieve corresponding vertex normals for the triangle vertices.
    float3 triangleNormal = g_vertices[indices[0]].normal;

    // PERFORMANCE TIP: it is recommended to avoid values carry over across TraceRay() calls. 
    // Therefore, in cases like retrieving HitWorldPosition(), it is recomputed every time.

    float3 hitPosition = HitWorldPosition();

    rayPayload.color = Shade(hitPosition, rayPayload, triangleNormal);
}

[shader("closesthit")]
void MyClosestHitShader_AABB(inout RayPayload rayPayload, in ProceduralPrimitiveAttributes attr)
{
    // PERFORMANCE TIP: it is recommended to minimize values carry over across TraceRay() calls. 
    // Therefore, in cases like retrieving HitWorldPosition(), it is recomputed every time.

    // Shadow component.
    // Trace a shadow ray.
    float3 hitPosition = HitWorldPosition();
    //float3 z = g_sceneCB.lightPosition.xyz - hitPosition;
    //Ray shadowRay = { hitPosition, z };
    //bool shadowRayHit = TraceShadowRayAndReportIfHit(shadowRay, rayPayload.recursionDepth);

    //// Reflected component.
    //float3 reflectedColor = float4(0, 0, 0, 0);
    //if (l_materialCB.reflectanceCoef > 0.001)
    //{
    //    // Trace a reflection ray.
    //    Ray reflectionRay = { HitWorldPosition(), reflect(WorldRayDirection(), attr.normal) };
    //    float3 reflectionColor = TraceRadianceRay(reflectionRay, rayPayload.recursionDepth);

    //    float3 fresnelR = FresnelReflectanceSchlick(WorldRayDirection(), attr.normal, l_materialCB.albedo.xyz);
    //    reflectedColor = l_materialCB.reflectanceCoef * fresnelR * reflectionColor;
    //}

    //// Calculate final color.
    //float3 toLight = normalize(g_sceneCB.lightPosition.xyz - hitPosition);
    //float3 toEye = normalize(g_sceneCB.cameraPosition.xyz - hitPosition);
    //float3 phongColor = BxDF::DirectLighting::Shade(l_materialCB.albedo, attr.normal, toLight, toEye, shadowRayHit, g_sceneCB, l_materialCB.diffuseCoef, l_materialCB.specularCoef, l_materialCB.specularPower);
    //float3 color = l_materialCB.shaded == 1.0f ?phongColor + reflectedColor : l_materialCB.albedo;

    //// Apply visibility falloff.
    //float t = RayTCurrent();
    //color = lerp(color, BackgroundColor, 1.0 - exp(-0.000002*t*t*t));

    rayPayload.color = Shade(hitPosition, rayPayload, attr.normal);
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

//***************************************************************************
//*****************------ Intersection shaders-------************************
//***************************************************************************

// Get ray in AABB's local space.
Ray GetRayInAABBPrimitiveLocalSpace()
{
    PrimitiveInstancePerFrameBuffer attr = g_AABBPrimitiveAttributes[l_aabbCB.instanceIndex];

    // Retrieve a ray origin position and direction in bottom level AS space 
    // and transform them into the AABB primitive's local space.
    Ray ray;
    ray.origin = mul(float4(ObjectRayOrigin(), 1), attr.bottomLevelASToLocalSpace).xyz;
    ray.direction = mul(ObjectRayDirection(), (float3x3) attr.bottomLevelASToLocalSpace);
    return ray;
}

[shader("intersection")]
void MyIntersectionShader_AnalyticPrimitive()
{
    Ray localRay = GetRayInAABBPrimitiveLocalSpace();
    // Which actual geometry we hit.
    AnalyticPrimitive::Enum primitiveType = (AnalyticPrimitive::Enum) l_aabbCB.primitiveType;

    float thit;
    // normal
    ProceduralPrimitiveAttributes attr = (ProceduralPrimitiveAttributes)0;
    if (RayAnalyticGeometryIntersectionTest(localRay, primitiveType, thit, attr))
    {
        PrimitiveInstancePerFrameBuffer aabbAttribute = g_AABBPrimitiveAttributes[l_aabbCB.instanceIndex];
        attr.normal = mul(attr.normal, (float3x3) aabbAttribute.localSpaceToBottomLevelAS);
        attr.normal = normalize(mul((float3x3) ObjectToWorld3x4(), attr.normal));

        // return normal at the intersection
        ReportHit(thit, /*hitKind*/ 0, attr);
    }
}

#endif // RAYTRACING_HLSL