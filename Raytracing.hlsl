#ifndef RAYTRACING_HLSL
#define RAYTRACING_HLSL

#define HLSL
#include "RaytracingHlslCompat.hlsli"


#ifndef RAYTRACINGSHADERHELPER_H
#define RAYTRACINGSHADERHELPER_H

#include "RayTracingHlslCompat.hlsli"

#define INFINITY (1.0/0.0)

//debug
//if (DTid.x < 50 && DTid.y < 50) {
//    g_renderTarget[DTid.xy] = float4(1.0f, 0.0f, 0.0f, 1.0f);
//    return;
//}




inline bool vecEqual(const float4 lhs, const float4 rhs) { return !any(lhs - rhs > 0.001f); }

bool matEqual(const float4x4 lhs, const float4x4 rhs) {
    int4 result = 0;
    for (int i = 0; i < 4; i++) {
        result[i] = (int)vecEqual(lhs[i], rhs[i]);
    }
    return all(result);
}

struct Ray
{
    float3 origin;
    float3 direction;
};

float length_toPow2(float2 p)
{
    return dot(p, p);
}

float length_toPow2(float3 p)
{
    return dot(p, p);
}

// Returns a cycling <0 -> 1 -> 0> animation interpolant 
float CalculateAnimationInterpolant(in float elapsedTime, in float cycleDuration)
{
    float curLinearCycleTime = fmod(elapsedTime, cycleDuration) / cycleDuration;
    curLinearCycleTime = (curLinearCycleTime <= 0.5f) ? 2 * curLinearCycleTime : 1 - 2 * (curLinearCycleTime - 0.5f);
    return smoothstep(0, 1, curLinearCycleTime);
}

void swap(inout float a, inout float b)
{
    float temp = a;
    a = b;
    b = temp;
}

bool IsInRange(in float val, in float min, in float max)
{
    return (val >= min && val <= max);
}

// Load three 16 bit indices.
static
uint3 Load3x16BitIndices(uint offsetBytes, ByteAddressBuffer Indices)
{
    uint3 indices;

    // ByteAdressBuffer loads must be aligned at a 4 byte boundary.
    // Since we need to read three 16 bit indices: { 0, 1, 2 } 
    // aligned at a 4 byte boundary as: { 0 1 } { 2 0 } { 1 2 } { 0 1 } ...
    // we will load 8 bytes (~ 4 indices { a b | c d }) to handle two possible index triplet layouts,
    // based on first index's offsetBytes being aligned at the 4 byte boundary or not:
    //  Aligned:     { 0 1 | 2 - }
    //  Not aligned: { - 0 | 1 2 }
    const uint dwordAlignedOffset = offsetBytes & ~3;
    const uint2 four16BitIndices = Indices.Load2(dwordAlignedOffset);

    // Aligned: { 0 1 | 2 - } => retrieve first three 16bit indices
    if (dwordAlignedOffset == offsetBytes)
    {
        indices.x = four16BitIndices.x & 0xffff;
        indices.y = (four16BitIndices.x >> 16) & 0xffff;
        indices.z = four16BitIndices.y & 0xffff;
    }
    else // Not aligned: { - 0 | 1 2 } => retrieve last three 16bit indices
    {
        indices.x = (four16BitIndices.x >> 16) & 0xffff;
        indices.y = four16BitIndices.y & 0xffff;
        indices.z = (four16BitIndices.y >> 16) & 0xffff;
    }

    return indices;
}

// Retrieve hit world position.
float3 HitWorldPosition()
{
    return WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
}

// Retrieve attribute at a hit position interpolated from vertex attributes using the hit's barycentrics.
//float3 HitAttribute(float3 vertexAttribute[3], float2 barycentrics)
//{
//    return vertexAttribute[0] +
//        barycentrics.x * (vertexAttribute[1] - vertexAttribute[0]) +
//        barycentrics.y * (vertexAttribute[2] - vertexAttribute[0]);
//}

// Retrieve attribute at a hit position interpolated from vertex attributes using the hit's barycentrics.
float3 HitAttribute(float3 vertexAttribute[3], BuiltInTriangleIntersectionAttributes attr)
{
    return vertexAttribute[0] +
        attr.barycentrics.x * (vertexAttribute[1] - vertexAttribute[0]) +
        attr.barycentrics.y * (vertexAttribute[2] - vertexAttribute[0]);
}


// Retrieve attribute at a hit position interpolated from vertex attributes using the hit's barycentrics.
float2 HitAttribute(float2 vertexAttribute[3], BuiltInTriangleIntersectionAttributes attr)
{
    return vertexAttribute[0] +
        attr.barycentrics.x * (vertexAttribute[1] - vertexAttribute[0]) +
        attr.barycentrics.y * (vertexAttribute[2] - vertexAttribute[0]);
}

// Generate a ray in world space for a camera pixel corresponding to an index from the dispatched 2D grid.
inline Ray GenerateCameraRay(uint2 index, in float3 cameraPosition, in float4x4 projectionToWorld, in int frameIndex)
{
    float2 offset = float2(0.0f, 0.0f);
    float o[] = { 0.125f, 0.375f, 0.625f, 0.875f };
    //float o[] = { 0.0625f, 0.1875f, 0.3125f, 0.4375f, 0.5625f, 0.6875f, 0.8125f, 0.9375f };
   /* float o[16];
    for (int i = 0; i < 16; i++)
    {
        o[i] = 0.03125f + i * 0.0625f;
    }*/
    //frameIndex = 0;
    offset = float2(o[frameIndex % 4], o[frameIndex / 4]);

    /*switch (frameIndex) {
        case 0:
            offset = float2(o[0], o[0]);
            break;
        case 1:
            offset = float2(o[1], o[0]);
            break;
        case 2:
            offset = float2(o[2], o[0]);
            break;
        case 3:
            offset = float2(o[3], o[0]);
            break;
        case 4:
            offset = float2(o[0], o[1]);
            break;
        case 5:
            offset = float2(o[1], o[1]);
            break;
        case 6:
            offset = float2(o[2], o[1]);
            break;
        case 7:
            offset = float2(o[3], o[1]);
            break;
        case 8:
            offset = float2(o[0], o[2]);
            break;
        case 9:
            offset = float2(o[1], o[2]);
            break;
        case 10:
            offset = float2(o[2], o[2]);
            break;
        case 11:
            offset = float2(o[3], o[2]);
            break;
        case 12:
            offset = float2(o[0], o[3]);
            break;
        case 13:
            offset = float2(o[1], o[3]);
            break;
        case 14:
            offset = float2(o[2], o[3]);
            break;
        case 15:
            offset = float2(o[3], o[3]);
            break;
        }*/
        //offset = float2(0.5f, 0.5f);
    float2 xy = index + offset; // center in the middle of the pixel.
    float2 screenPos = xy / DispatchRaysDimensions().xy * 2.0 - 1.0;

    // Invert Y for DirectX-style coordinates.
    screenPos.y = -screenPos.y;

    // Unproject the pixel coordinate into a world positon.
    float4 world = mul(float4(screenPos, 0, 1), projectionToWorld);
    world.xyz /= world.w;

    Ray ray;
    ray.origin = cameraPosition;
    ray.direction = normalize(world.xyz - ray.origin);

    return ray;
}



// Test if a hit is culled based on specified RayFlags.
bool IsCulled(in Ray ray, in float3 hitSurfaceNormal)
{
    float rayDirectionNormalDot = dot(ray.direction, hitSurfaceNormal);

    bool isCulled =
        ((RayFlags() & RAY_FLAG_CULL_BACK_FACING_TRIANGLES) && (rayDirectionNormalDot > 0))
        ||
        ((RayFlags() & RAY_FLAG_CULL_FRONT_FACING_TRIANGLES) && (rayDirectionNormalDot < 0));

    return isCulled;
}

// Test if a hit is valid based on specified RayFlags and <RayTMin, RayTCurrent> range.
bool IsAValidHit(in Ray ray, in float thit, in float3 hitSurfaceNormal)
{
    return IsInRange(thit, RayTMin(), RayTCurrent()) && !IsCulled(ray, hitSurfaceNormal);
}

// Texture coordinates on a horizontal plane.
float2 TexCoords(in float3 position)
{
    return position.xz;
}

// Calculate ray differentials.
void CalculateRayDifferentials(out float2 ddx_uv, out float2 ddy_uv, in float2 uv, in float3 hitPosition, in float3 surfaceNormal, in float3 cameraPosition, in float4x4 projectionToWorld)
{
    // Compute ray differentials by intersecting the tangent plane to the  surface.
    Ray ddx = GenerateCameraRay(DispatchRaysIndex().xy + uint2(1, 0), cameraPosition, projectionToWorld, 0);
    Ray ddy = GenerateCameraRay(DispatchRaysIndex().xy + uint2(0, 1), cameraPosition, projectionToWorld, 0);

    // Compute ray differentials.
    float3 ddx_pos = ddx.origin - ddx.direction * dot(ddx.origin - hitPosition, surfaceNormal) / dot(ddx.direction, surfaceNormal);
    float3 ddy_pos = ddy.origin - ddy.direction * dot(ddy.origin - hitPosition, surfaceNormal) / dot(ddy.direction, surfaceNormal);

    // Calculate texture sampling footprint.
    ddx_uv = TexCoords(ddx_pos) - uv;
    ddy_uv = TexCoords(ddy_pos) - uv;
}

// Forward declaration.
float CheckersTextureBoxFilter(in float2 uv, in float2 dpdx, in float2 dpdy, in uint ratio);

// Return analytically integrated checkerboard texture (box filter).
float AnalyticalCheckersTexture(in float3 hitPosition, in float3 surfaceNormal, in float3 cameraPosition, in float4x4 projectionToWorld)
{
    float2 ddx_uv;
    float2 ddy_uv;
    float2 uv = TexCoords(hitPosition);

    CalculateRayDifferentials(ddx_uv, ddy_uv, uv, hitPosition, surfaceNormal, cameraPosition, projectionToWorld);
    return CheckersTextureBoxFilter(uv, ddx_uv, ddy_uv, 50);
}

// Fresnel reflectance - schlick approximation.
float3 FresnelReflectanceSchlick(in float3 I, in float3 N, in float3 f0)
{
    float cosi = saturate(dot(-I, N));
    return f0 + (1 - f0) * pow(1 - cosi, 5);
}

// Analytically integrated checkerboard grid (box filter).
// Ref: http://iquilezles.org/www/articles/filterableprocedurals/filterableprocedurals.htm
// ratio - Center fill to border ratio.
float CheckersTextureBoxFilter(in float2 uv, in float2 dpdx, in float2 dpdy, in uint ratio)
{
    float2 w = max(abs(dpdx), abs(dpdy));   // Filter kernel
    float2 a = uv + 0.5 * w;
    float2 b = uv - 0.5 * w;

    // Analytical integral (box filter).
    float2 i = (floor(a) + min(frac(a) * ratio, 1.0) -
        floor(b) - min(frac(b) * ratio, 1.0)) / (ratio * w);
    return (1.0 - i.x) * (1.0 - i.y);
}

float nrand(float2 uv)
{
    return frac(sin(dot(uv, float2(12.9898, 78.233))) * 43758.5453);
}

// [PBR, 13.6.1]
typedef float3 Vector3f;
typedef float2 Point2f;
typedef float3 Point3f;
static const float Pi = 3.14159265358979323846;
static const float InvPi = 0.31830988618379067154;
static const float Inv2Pi = 0.15915494309189533577;
static const float Inv4Pi = 0.07957747154594766788;
static const float PiOver2 = 1.57079632679489661923;
static const float PiOver4 = 0.78539816339744830961;
static const float Sqrt2 = 1.41421356237309504880;

Vector3f UniformSampleHemisphere(const Point2f u) {
    float z = u.x;
    float r = sqrt(max(0.0f, 1.0f - z * z));
    float phi = 2 * Pi * u.y;
    return Vector3f(r * cos(phi), r * sin(phi), z);
}


Vector3f UniformSampleSphere(const Point2f u) {
    float z = 1 - 2 * u.x;
    float r = sqrt(max(0.0f, 1.0f - z * z));
    float phi = 2 * Pi * u.y;
    return Vector3f(r * cos(phi), r * sin(phi), z);
}

// [PBR, 14.2.2]
namespace Sphere
{
    Vector3f Sample(const Point2f u, float radius, float4x4 ObjectToWorld) {
        Point3f pObj = Point3f(0, 0, 0) + radius * UniformSampleSphere(u);
        pObj *= radius / length(pObj - Point3f(0, 0, 0));
        return mul(float4(pObj, 1.0f), ObjectToWorld).xyz;
    }
}

void Sample_Li()
{

}


void Sample_Disk() {

}

// [PBR, 13.6.2]
Point2f ConcentricSampleDisk(const Point2f u) {
    // Map uniform random numbers to [-1,1]^2
    //Point2f uOffset = 2.0f * u - Vector2f(1, 1);
    // ALREADY DONE IN THE SHADER

    // Handle degeneracy at the origin
    if (u.x == 0 && u.y == 0)
        return Point2f(0, 0);

    // Apply concentric mapping to point
    float theta, r;
    if (abs(u.x) > abs(u.y)) {
        r = u.x;
        theta = PiOver4 * (u.y / u.x);
    }
    else {
        r = u.y;
        theta = PiOver2 - PiOver4 * (u.x / u.y);
    }
    return r * Point2f(cos(theta), sin(theta));
}

namespace Disk
{
    Vector3f Sample(const Point2f u, const float lightSize, float height, float4x4 ObjectToWorld) {
        Point2f pd = ConcentricSampleDisk(u);
        Point3f pObj = float3(pd.x * lightSize, pd.y * lightSize, height);
        return mul(pObj, ObjectToWorld);
    }
}

double copysign(double x, double s) {
    return (s >= 0) ? abs(x) : -abs(x);
}

Vector3f OrthoNormalVector(double x, double y, double z) {
    const double g = copysign(1., z);
    const double h = z + g;
    return float3(g - x * x / h, -x * y / h, -x);
}

float4x4 BuildLookAtMatrix(float3 origin, float3 zAxis) {
    float3 yAxis = OrthoNormalVector(zAxis.x, zAxis.y, zAxis.z);
    float3 xAxis = cross(zAxis, yAxis);

    float4x4 m;

    m[0][0] = xAxis.x;
    m[0][1] = yAxis.x;
    m[0][2] = zAxis.x;
    m[0][3] = 0;

    m[1][0] = xAxis.y;
    m[1][1] = yAxis.y;
    m[1][2] = zAxis.y;
    m[1][3] = 0;

    m[2][0] = xAxis.z;
    m[2][1] = yAxis.z;
    m[2][2] = zAxis.z;
    m[2][3] = 0;

    m[3][0] = dot(xAxis, origin);
    m[3][1] = dot(yAxis, origin);
    m[3][2] = dot(zAxis, origin);
    m[3][3] = 1;

    return m;
}

float4x4 inverse(float4x4 m) {
    float n11 = m[0][0], n12 = m[1][0], n13 = m[2][0], n14 = m[3][0];
    float n21 = m[0][1], n22 = m[1][1], n23 = m[2][1], n24 = m[3][1];
    float n31 = m[0][2], n32 = m[1][2], n33 = m[2][2], n34 = m[3][2];
    float n41 = m[0][3], n42 = m[1][3], n43 = m[2][3], n44 = m[3][3];

    float t11 = n23 * n34 * n42 - n24 * n33 * n42 + n24 * n32 * n43 - n22 * n34 * n43 - n23 * n32 * n44 + n22 * n33 * n44;
    float t12 = n14 * n33 * n42 - n13 * n34 * n42 - n14 * n32 * n43 + n12 * n34 * n43 + n13 * n32 * n44 - n12 * n33 * n44;
    float t13 = n13 * n24 * n42 - n14 * n23 * n42 + n14 * n22 * n43 - n12 * n24 * n43 - n13 * n22 * n44 + n12 * n23 * n44;
    float t14 = n14 * n23 * n32 - n13 * n24 * n32 - n14 * n22 * n33 + n12 * n24 * n33 + n13 * n22 * n34 - n12 * n23 * n34;

    float det = n11 * t11 + n21 * t12 + n31 * t13 + n41 * t14;
    float idet = 1.0f / det;

    float4x4 ret;

    ret[0][0] = t11 * idet;
    ret[0][1] = (n24 * n33 * n41 - n23 * n34 * n41 - n24 * n31 * n43 + n21 * n34 * n43 + n23 * n31 * n44 - n21 * n33 * n44) * idet;
    ret[0][2] = (n22 * n34 * n41 - n24 * n32 * n41 + n24 * n31 * n42 - n21 * n34 * n42 - n22 * n31 * n44 + n21 * n32 * n44) * idet;
    ret[0][3] = (n23 * n32 * n41 - n22 * n33 * n41 - n23 * n31 * n42 + n21 * n33 * n42 + n22 * n31 * n43 - n21 * n32 * n43) * idet;

    ret[1][0] = t12 * idet;
    ret[1][1] = (n13 * n34 * n41 - n14 * n33 * n41 + n14 * n31 * n43 - n11 * n34 * n43 - n13 * n31 * n44 + n11 * n33 * n44) * idet;
    ret[1][2] = (n14 * n32 * n41 - n12 * n34 * n41 - n14 * n31 * n42 + n11 * n34 * n42 + n12 * n31 * n44 - n11 * n32 * n44) * idet;
    ret[1][3] = (n12 * n33 * n41 - n13 * n32 * n41 + n13 * n31 * n42 - n11 * n33 * n42 - n12 * n31 * n43 + n11 * n32 * n43) * idet;

    ret[2][0] = t13 * idet;
    ret[2][1] = (n14 * n23 * n41 - n13 * n24 * n41 - n14 * n21 * n43 + n11 * n24 * n43 + n13 * n21 * n44 - n11 * n23 * n44) * idet;
    ret[2][2] = (n12 * n24 * n41 - n14 * n22 * n41 + n14 * n21 * n42 - n11 * n24 * n42 - n12 * n21 * n44 + n11 * n22 * n44) * idet;
    ret[2][3] = (n13 * n22 * n41 - n12 * n23 * n41 - n13 * n21 * n42 + n11 * n23 * n42 + n12 * n21 * n43 - n11 * n22 * n43) * idet;

    ret[3][0] = t14 * idet;
    ret[3][1] = (n13 * n24 * n31 - n14 * n23 * n31 + n14 * n21 * n33 - n11 * n24 * n33 - n13 * n21 * n34 + n11 * n23 * n34) * idet;
    ret[3][2] = (n14 * n22 * n31 - n12 * n24 * n31 - n14 * n21 * n32 + n11 * n24 * n32 + n12 * n21 * n34 - n11 * n22 * n34) * idet;
    ret[3][3] = (n12 * n23 * n31 - n13 * n22 * n31 + n13 * n21 * n32 - n11 * n23 * n32 - n12 * n21 * n33 + n11 * n22 * n33) * idet;

    return ret;
}

float4x4 BuildInverseLookAtMatrix(float3 origin, float3 zAxis) {
    return inverse(BuildLookAtMatrix(origin, zAxis));
}
#endif // RAYTRACINGSHADERHELPER_H

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