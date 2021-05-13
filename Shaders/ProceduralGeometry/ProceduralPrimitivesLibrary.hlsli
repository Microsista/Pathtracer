//**********************************************************************************************
//
// ProceduralPrimitivesLibrary.hlsli
//
// An interface to call per geometry intersection tests based on as primitive type.
//
//**********************************************************************************************

#ifndef PROCEDURALPRIMITIVESLIBRARY_H
#define PROCEDURALPRIMITIVESLIBRARY_H

#include "../RaytracingShaderHelper.hlsli"

#include "AnalyticPrimitives.hlsli"

// Analytic geometry intersection test.
// AABB local space dimensions: <-1,1>.
bool RayAnalyticGeometryIntersectionTest(in Ray ray, in AnalyticPrimitive::Enum analyticPrimitive, out float thit, out ProceduralPrimitiveAttributes attr)
{
    float3 aabb[2] = {
        float3(-1,-1,-1),
        float3(1,1,1)
    };
    float tmax;

    switch (analyticPrimitive)
    {
    case AnalyticPrimitive::AABB: return RayAABBIntersectionTest(ray, aabb, thit, attr);
    case AnalyticPrimitive::Spheres: return RaySpheresIntersectionTest(ray, thit, attr);
    case AnalyticPrimitive::Sun: return RaySpheresIntersectionTest(ray, thit, attr);
    case AnalyticPrimitive::Ball : return RaySpheresIntersectionTest(ray, thit, attr);
    default: return false;
    }
}

#endif // PROCEDURALPRIMITIVESLIBRARY_H