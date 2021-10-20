#ifndef RAYTRACING_HLSL
#define RAYTRACING_HLSL

#define HLSL
#include "RaytracingShaderHelper.hlsli"

#ifndef RANDOMNUMBERGENERATOR_H
#define RANDOMNUMBERGENERATOR_H

// Ref: http://www.reedbeta.com/blog/quick-and-easy-gpu-random-numbers-in-d3d11/
namespace RNG
{
    // Create an initial random number for this thread
    uint SeedThread(uint seed)
    {
        // Thomas Wang hash 
        // Ref: http://www.burtleburtle.net/bob/hash/integer.html
        seed = (seed ^ 61) ^ (seed >> 16);
        seed *= 9;
        seed = seed ^ (seed >> 4);
        seed *= 0x27d4eb2d;
        seed = seed ^ (seed >> 15);
        return seed;
    }

    // Generate a random 32-bit integer
    uint Random(inout uint state)
    {
        // Xorshift algorithm from George Marsaglia's paper.
        state ^= (state << 13);
        state ^= (state >> 17);
        state ^= (state << 5);
        return state;
    }

    // Generate a random float in the range [0.0f, 1.0f)
    float Random01(inout uint state)
    {
        return asfloat(0x3f800000 | Random(state) >> 9) - 1.0;
    }

    // Generate a random float in the range [0.0f, 1.0f]
    float Random01inclusive(inout uint state)
    {
        return Random(state) / float(0xffffffff);
    }


    // Generate a random integer in the range [lower, upper]
    uint Random(inout uint state, uint lower, uint upper)
    {
        return lower + uint(float(upper - lower + 1) * Random01(state));
    }
}
#endif // RANDOMNUMBERGENERATOR_H

// A family of BRDF, BSDF and BTDF functions.
// BRDF, BSDF, BTDF - bidirectional reflectance, scattering, transmission distribution function.
// Ref: Ray Tracing from the Ground Up (RTG), Suffern
// Ref: Real-Time Rendering (RTR), Fourth Edition
// Ref: Real Shading in Unreal Engine 4 (Karis_UE4), Karis2013
// Ref: PBR Diffuse Lighting for GGX+Smith Microsurfaces, Hammon2017

// BRDF terms generally include 1 / PI factor, but this is removed in the implementations below as it cancels out
// with the omitted PI factor in the reflectance equation. Ref: eq 9.14, RTR

// Parameters:
// iorIn - ior of media ray is coming from
// iorOut - ior of media ray is going to
// eta - relative index of refraction, namely iorIn / iorOut
// G - shadowing/masking function.
// Fo - specular reflectance at normal incidence (AKA specular color).
// Albedo - material color
// Roughness - material roughness
// N - surface normal
// V - direction to viewer
// L - incoming "to-light" direction
// T - transmission scale factor (aka transmission color)
// thetai - incident angle

#ifndef BXDF_HLSL
#define BXDF_HLSL

namespace BxDF {

    //
    // This namespace implements BTDF for a perfect transmitter that uses a single index of refraction (ior)
    // and iorOut represent air, i.e. 1.
    bool IsBlack(float3 color)
    {
        return !any(color);
    }

    namespace Diffuse {

        //
        namespace Lambert {

            float3 F(in float3 albedo)
            {
                return albedo;
            }
        }

        //
        namespace Hammon {

            // Compute the value of BRDF
            // Ref: Hammon2017
            float3 F(in float3 Albedo, in float Roughness, in float3 N, in float3 V, in float3 L, in float3 Fo)
            {
                float3 diffuse = 0;

                float3 H = normalize(V + L);
                float NoH = dot(N, H);
                if (NoH > 0)
                {
                    float a = Roughness * Roughness;

                    float NoV = saturate(dot(N, V));
                    float NoL = saturate(dot(N, L));
                    float LoV = saturate(dot(L, V));

                    float facing = 0.5 + 0.5 * LoV;
                    float rough = facing * (0.9 - 0.4 * facing) * ((0.5 + NoH) / NoH);
                    float3 smooth = 1.05 * (1 - pow(1 - NoL, 5)) * (1 - pow(1 - NoV, 5));

                    // Extract 1 / PI from the single equation since it's ommited in the reflectance function.
                    float3 single = lerp(smooth, rough, a);
                    float multi = 0.3641 * a; // 0.3641 = PI * 0.1159

                    diffuse = Albedo * (single + Albedo * multi);
                }
                return diffuse;
            }
        }
    }

    //
    // Fresnel reflectance - schlick approximation.
    // Calculates how much colour is coming from reflection vs how much is coming from it's own colour
    float3 Fresnel(in float3 F0, in float cos_thetai)
    {
        F0 /= 4.0f;
        return F0 + (1 - F0) * pow(1 - cos_thetai, 5);
    }

    typedef int BOOL;

    namespace Specular {

        // Perfect/Specular reflection.
        namespace Reflection {

            // Calculates L and returns BRDF value for that direction.
            // Assumptions: V and N are in the same hemisphere.
            // Note: to avoid unnecessary precision issues and for the sake of performance the function doesn't divide by the cos term
            // so as to nullify the cos term in the rendering equation. Therefore the caller should skip the cos term in the rendering equation.
            float3 Sample_Fr(in float3 V, out float3 L, in float3 N, in float3 Fo)
            {
                L = reflect(-V, N);
                float cos_thetai = dot(N, L);
                return Fresnel(Fo, cos_thetai);
            }

            // Calculate whether a total reflection occurs at a given V and a normal
            // Ref: eq 27.5, Ray Tracing from the Ground Up
            BOOL IsTotalInternalReflection(
                in float3 V,
                in float3 normal)
            {
                float ior = 1;
                float eta = ior;
                float cos_thetai = dot(normal, V); // Incident angle

                return 1 - 1 * (1 - cos_thetai * cos_thetai) / (eta * eta) < 0;
            }

        }

        // Perfect/Specular trasmission.
        namespace Transmission {

            // Calculates transmitted ray wt and return BRDF value for that direction.
            // Assumptions: V and N are in the same hemisphere.
            // Note: to avoid unnecessary precision issues and for the sake of performance the function doesn't divide by the cos term
            // so as to nullify the cos term in the rendering equation. Therefore the caller should skip the cos term in the rendering equation.
            float3 Sample_Ft(in float3 V, out float3 wt, in float3 N, in float3 Fo)
            {
                float ior = 1;
                wt = -V; // TODO: refract(-V, N, ior);
                float cos_thetai = dot(V, N);
                float3 Kr = Fresnel(Fo, cos_thetai);

                return (1 - Kr);
            }
        }

        //
        // Ref: Chapter 9.8, RTR
        namespace GGX {

            // Compute the value of BRDF
            float3 F(in float Roughness, in float3 N, in float3 V, in float3 L, in float3 Fo)
            {
                float3 H = V + L;
                float NoL = dot(N, L);
                float NoV = dot(N, V);
                float3 specular = 0;

                if (NoL > 0 && NoV > 0 && all(H)) // do all H coefficient's have to be non-zero?
                {
                    H = normalize(H);
                    float a = Roughness;        // The roughness has already been remapped to alpha.
                    float3 M = H;               // microfacet normal, equals h, since BRDF is 0 for all m =/= h. Ref: 9.34, RTR
                    float NoM = saturate(dot(N, M));
                    float HoL = saturate(dot(H, L));

                    // D (probability density function of normal m - which facet normals exist, but not their arrangement) [Hammon17, p. 19]
                    // Ref: eq 9.41, RTR
                    float denom = 1 + NoM * NoM * (a * a - 1);
                    float D = a * a / (denom * denom);  // Karis

                    // F (BRDF, how an individual facet responds to light) [Hammon17, p. 18]
                    // only the fresnel reflection fraction of incoming light does specular reflection
                    // Fresnel reflectance - Schlick approximation for F(h,l)
                    // Ref: 9.16, RTR
                    float3 F = Fresnel(Fo, HoL);

                    // G (Occlusion due to actual microfacet arrangement, actual shape - probability microfacet m sees both light l and viewer v) [Hammon17, p. 20]
                    // Visibility due to shadowing/masking of a microfacet.
                    // G coupled with BRDF's {1 / 4 DotNL * DotNV} factor.
                    // Ref: eq 9.45, RTR
                    float G = 0.5 / lerp(2 * NoL * NoV, NoL + NoV, a);

                    // D * G - Probability density of having microfacet normal m that is both lit and seen, i.e. probability density of using BRDF F.  [Hammon17, p. 24]
                    // Specular BRDF term
                    // Ref: eq 9.34, RTR
                    specular = F * G * D;
                }

                return specular;
            }
        }
    }

    //
    namespace DirectLighting
    {
        // Calculate a color of the shaded surface due to direct lighting.
        // Returns a shaded color.
        float3 Shade(in float3 Albedo, in float3 N, in float3 L, in float3 V, in bool inShadow, SceneConstantBuffer g_sceneCB, in float Radiance = 1.0, in float Fo = 0.1, in float Roughness = 0.0)
        {
            float3 directLighting = 0;
            if (dot(N, V) < 0)
                N *= -1;
            float NoL = dot(N, L);
            if (!inShadow && NoL > 0)
            {

                float3 directDiffuse = 0;
                if (!IsBlack(Albedo))
                {
                    //if (materialType == MaterialType::Default)
                    {
                        directDiffuse = BxDF::Diffuse::Hammon::F(Albedo, Roughness, N, V, L, Fo);
                    }
                    //else
                    {
                        //directDiffuse = BxDF::Diffuse::Lambert::F(Albedo);
                    }
                }

                float3 directSpecular = 0;
                //if (materialType == MaterialType::Default)
                {
                    directSpecular = BxDF::Specular::GGX::F(Roughness, N, V, L, Fo);
                }

                directLighting = NoL * Radiance * (directDiffuse + directSpecular);
            }

            return directLighting;
        }
    }

    // Calculate a color of the shaded surface.
    float3 Shade(
        in MaterialType::Type materialType,
        in float3 Albedo,
        in float3 Fo,
        in float3 Radiance,
        in bool isInShadow,
        in float AmbientCoef,
        in float Roughness,
        in float3 N,
        in float3 V,
        in float3 L)
    {
        float NoL = dot(N, L);
        Roughness = max(0.1, Roughness);
        float3 directLighting = 0;

        if (!isInShadow && NoL > 0)
        {
            // Diffuse.
            float3 diffuse;
            if (materialType == MaterialType::Default)
            {
                diffuse = BxDF::Diffuse::Hammon::F(Albedo, Roughness, N, V, L, Fo);
            }
            else
            {
                diffuse = BxDF::Diffuse::Lambert::F(Albedo);
            }

            // Specular.
            float3 directDiffuse = diffuse;
            float3 directSpecular = 0;

            if (materialType == MaterialType::Default)
            {
                directSpecular = BxDF::Specular::GGX::F(Roughness, N, V, L, Fo);
            }

            directLighting = NoL * Radiance * (directDiffuse + directSpecular);
        }

        float3 indirectDiffuse = AmbientCoef * Albedo;
        float3 indirectLighting = indirectDiffuse;

        return directLighting + indirectLighting;
    }
}


#endif // BXDF_HLSL






static const float LIGHT_SIZE = 0.6f;

//
// Resources
//

// Global
RaytracingAccelerationStructure g_scene : register(t0);
RWTexture2D<float3> g_renderTarget : register(u0);
RWTexture2D<float3> g_reflectionBuffer : register(u1);
RWTexture2D<float3> g_shadowBuffer : register(u2);
RWTexture2D<float3> g_normalDepth : register(u3);
RWTexture2D<int4> g_rtTextureSpaceMotionVector : register(u4);
RWTexture2D<float3> g_previousFrameHitPosition : register(u5);

Texture2D<float3> g_normalMap : register(t5);
Texture2D<float3> g_specularMap : register(t6);
Texture2D<float3> g_emissiveMap : register(t7);

ConstantBuffer<SceneConstantBuffer> g_sceneCB : register(b0);

SamplerState LinearWrapSampler : register(s0);

// Local
StructuredBuffer<Index> l_indices : register(t1);
StructuredBuffer<VertexPositionNormalTextureTangent> l_vertices : register(t2);
ConstantBuffer<PrimitiveConstantBuffer> l_materialCB : register(b1);
Texture2D<float3> l_texDiffuse : register(t3);

//***************************************************************************
//*****------ TraceRay wrappers for radiance and shadow rays. -------********
//***************************************************************************

// Trace a radiance ray into the scene and returns a shaded color.
RayPayload TraceRadianceRay(in Ray ray, in UINT currentRayRecursionDepth)
{
    RayPayload rayPayload;
    rayPayload.color = float4(0, 0, 0, 0);
    rayPayload.recursionDepth = currentRayRecursionDepth + 1;
    rayPayload.inShadow = 1.0f;
    rayPayload.depth = 1.0f;
    rayPayload.prevHitPosition = float3(0.0f, 0.0f, 0.0f);

    if (currentRayRecursionDepth >= MAX_RAY_RECURSION_DEPTH)
    {
        return rayPayload;
    }

    // Set the ray's extents.
    RayDesc rayDesc;
    rayDesc.Origin = ray.origin;
    rayDesc.Direction = ray.direction;
    // Set TMin to a zero value to avoid aliasing artifacts along contact areas.
    // Note: make sure to enable face culling so as to avoid surface face fighting.
    rayDesc.TMin = 0;
    rayDesc.TMax = 10000;
    
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
    rayPayload.color *= attenuation;

    return rayPayload;
}

// Trace a shadow ray and return true if it hits any geometry.
bool TraceShadowRayAndReportIfHit(in Ray ray, in UINT currentRayRecursionDepth, float3 N, int frameIndex)
{
    if (currentRayRecursionDepth >= MAX_RAY_RECURSION_DEPTH)
    {
        return false;
    }

    // Soft shadows
    uint threadId = DispatchRaysIndex().x + DispatchRaysIndex().y * DispatchRaysDimensions().x;
    uint RNGState = RNG::SeedThread(threadId + RNG::SeedThread(frameIndex));
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

// Retrieve hit object space position.
float3 HitObjectPosition()
{
    return ObjectRayOrigin() + RayTCurrent() * ObjectRayDirection();
}

//nshade
RayPayload Shade(
    float3 hitPosition,
    RayPayload rayPayload,
    float3 N,
    in PrimitiveMaterialBuffer material, in int frameIndex, in BuiltInTriangleIntersectionAttributes attr, uint3 indices)
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
    bool shadowRayHit = TraceShadowRayAndReportIfHit(shadowRay, rayPayload.recursionDepth, N, frameIndex);
    uint2 DTID = DispatchRaysIndex().xy;
    //
    // Diffuse and specular
    //
    float3 wi = normalize(g_sceneCB.lightPosition.xyz - hitPosition);
    L += float4(BxDF::DirectLighting::Shade(Kd, N, wi, V, false, g_sceneCB, Kd, Ks, roughness), 0.0f);

    //
    // INDIRECT ILLUMINATION
    //

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
        float3 Fr = (1 - (Fo.x + Fo.y + Fo.z)/1.1f ) * BxDF::Specular::Reflection::Sample_Fr(V, wi, N, Fo);    // Calculates wi
        Fr = 1 - Fr;
        Fr = dot(1, Fr)/3;

        // Fuzzy reflections
        uint threadId = DispatchRaysIndex().x + DispatchRaysIndex().y * DispatchRaysDimensions().x;
        uint RNGState = RNG::SeedThread(threadId + RNG::SeedThread(frameIndex));
        float2 noiseUV = {
            2 * RNG::Random01(RNGState) - 1,            // [0, 1) -> [-1, 1)
            2 * RNG::Random01(RNGState) - 1
        };

        float4x4 objectToWorld = BuildInverseLookAtMatrix(hitPosition, normalize(wi));

        RayPayload reflectedRayPayload = rayPayload;
        // Ref: eq 24.4, [Ray-tracing from the Ground Up]
        Ray reflectionRay = { HitWorldPosition(), normalize(Disk::Sample(noiseUV, roughness, 10, objectToWorld)) };
        
        RayPayload payload = TraceRadianceRay(reflectionRay, rayPayload.recursionDepth);
   
        // Trace a reflection ray.
        if (dot(N, V) < 0)
            N *= -1;

        float F = (Fo.x + Fo.y + Fo.z * 3.5f) / 3;
        float3 fresnel = FresnelReflectanceSchlick(WorldRayDirection(), N, F);

        g_reflectionBuffer[DTID] = fresnel * payload.color;
    }

    L += float4(Ke.x, Ke.y, Ke.z, 1.0f);
    RayPayload info{};
    info.color = l_materialCB.shaded ? L : l_materialCB.albedo;
    info.inShadow = shadowRayHit;

    return info;
}


// Generate camera's forward direction ray
inline float3 GenerateForwardCameraRayDirection(in float4x4 projectionToWorldWithCameraAtOrigin)
{
    float2 screenPos = float2(0, 0);

    // Unproject the pixel coordinate into a world positon.
    float4 world = mul(float4(screenPos, 0, 1), projectionToWorldWithCameraAtOrigin);
    return normalize(world.xyz);
}

float2 ClipSpaceToTexturePosition(in float4 clipSpacePosition)
{
    float3 NDCposition = clipSpacePosition.xyz / clipSpacePosition.w;   // Perspective divide to get Normal Device Coordinates: {[-1,1], [-1,1], (0, 1]}
    NDCposition.y = -NDCposition.y;                                     // Invert Y for DirectX-style coordinates.
    float2 texturePosition = (NDCposition.xy + 1) * 0.5f;               // [-1,1] -> [0, 1]
    return texturePosition;
}

// Calculate a texture space motion vector from previous to current frame.
float2 CalculateMotionVector(
    in float3 _hitPosition,
    out float _depth,
    in uint2 DTid)
{
    // Variables prefixed with underscore _ denote values in the previous frame.
    float3 _hitViewPosition = _hitPosition - g_sceneCB.prevFrameCameraPosition;
    float3 _cameraDirection = GenerateForwardCameraRayDirection(g_sceneCB.prevFrameProjToViewCameraAtOrigin);
    _depth = dot(_hitViewPosition, _cameraDirection);

    // Calculate screen space position of the hit in the previous frame.
    float4 _clipSpacePosition = mul(float4(_hitPosition, 1), g_sceneCB.prevFrameViewProj);
    float2 _texturePosition = ClipSpaceToTexturePosition(_clipSpacePosition);

    float2 xy = DispatchRaysIndex().xy + 0.5f;   // Center in the middle of the pixel.
    float2 texturePosition = xy / DispatchRaysDimensions().xy;

    return texturePosition - _texturePosition;
}



#endif // RAYTRACING_HLSL