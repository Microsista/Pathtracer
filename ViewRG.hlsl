#include "Raytracing.hlsl"

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