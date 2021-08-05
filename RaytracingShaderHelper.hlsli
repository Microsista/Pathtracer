#ifndef RAYTRACINGSHADERHELPER_H
#define RAYTRACINGSHADERHELPER_H

#include "RayTracingHlslCompat.h"

#define INFINITY (1.0/0.0)



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
float CheckersTextureBoxFilter(in float2 uv, in float2 dpdx, in float2 dpdy, in UINT ratio);

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
    return f0 + (1 - f0)*pow(1 - cosi, 5);
}

// Analytically integrated checkerboard grid (box filter).
// Ref: http://iquilezles.org/www/articles/filterableprocedurals/filterableprocedurals.htm
// ratio - Center fill to border ratio.
float CheckersTextureBoxFilter(in float2 uv, in float2 dpdx, in float2 dpdy, in UINT ratio)
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
        return mul(pObj, ObjectToWorld);
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