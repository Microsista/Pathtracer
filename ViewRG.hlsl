#include "Raytracing.hlsl"

bool vecEqual(float4 first, float4 second) {
    float4 diff = first - second;
    if (diff.x < 0.001f && diff.y < 0.001f && diff.z < 0.001f && diff.w < 0.001f) diff = 0.0;
    return !any(diff);
}

//bool matEqual(float4x4 first, float4x4 second) {
//    float4 diff;
//    diff.x = vecEqual(first[0], second[0]);
//    diff.y = vecEqual(first[1], second[1]);
//    diff.z = vecEqual(first[2], second[2]);
//    diff.w = vecEqual(first[3], second[3]);
//    return !any(diff);
//}

[shader("raygeneration")]
void MyRaygenShader()
{
    uint2 DTID = DispatchRaysIndex().xy;
    // Generate a ray for a camera pixel corresponding to an index from the dispatched 2D grid.
    Ray ray = GenerateCameraRay(DispatchRaysIndex().xy, g_sceneCB.cameraPosition.xyz, g_sceneCB.projectionToWorld, g_sceneCB.frameIndex);

    // Cast a ray into the scene and retrieve a shaded color.
    UINT currentRecursionDepth = 0;
    g_shadowBuffer[DTID] = float3(0, 0, 0);
    g_renderTarget[DTID] = float4(0, 0, 0, 0);
    g_reflectionBuffer[DTID] = float3(0, 0, 0);

    Info info = TraceRadianceRay(ray, currentRecursionDepth);

    // Clip position
    float _depth;
    float2 motionVector = CalculateMotionVector(info.prevHit, _depth, DTID);

    if (vecEqual(inverse(g_sceneCB.projectionToWorld)[0], g_sceneCB.prevFrameViewProj[0]) &&
        vecEqual(inverse(g_sceneCB.projectionToWorld)[1], g_sceneCB.prevFrameViewProj[1]) &&
        vecEqual(inverse(g_sceneCB.projectionToWorld)[2], g_sceneCB.prevFrameViewProj[2]) &&
        vecEqual(inverse(g_sceneCB.projectionToWorld)[3], g_sceneCB.prevFrameViewProj[3])) {
        g_rtTextureSpaceMotionVector[DTID] = 1.0f;
    }
    else {
        g_rtTextureSpaceMotionVector[DTID] = 0.0f;
    }

    g_previousFrameHitPosition[DispatchRaysIndex().xy] = info.prevHit;
    float3 inShadow = info.inShadow;
    float3 color = info.color;
    g_shadowBuffer[DTID] = inShadow;
    g_renderTarget[DTID] = color;
    g_normalDepth[DTID] = float3(info.depth, info.depth, info.depth);
}