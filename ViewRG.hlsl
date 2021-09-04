#include "Raytracing.hlsl"

[shader("raygeneration")]
void MyRaygenShader() {
    const uint2 dtid = DispatchRaysIndex().xy;
    const float4x4 projToWorld = g_sceneCB.projectionToWorld;
    const float3 cameraPos = g_sceneCB.cameraPosition.xyz;
    const int frameId = g_sceneCB.frameIndex;

    Ray ray = GenerateCameraRay(dtid, cameraPos, projToWorld, frameId);

    RayPayload payload = TraceRadianceRay(ray, 0);

    g_renderTarget[dtid] = payload.color;
    g_shadowBuffer[dtid] = payload.inShadow;
    g_normalDepth[dtid] = payload.depth;
}