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

    float3 worldPosition = ray.origin + ray.direction * payload.depth;
    double4 currentFramePosition = mul(double4(worldPosition, 1.0f), inverse(g_sceneCB.projectionToWorld));
    double4 previousFramePosition = mul(double4(worldPosition, 1.0f), g_sceneCB.prevFrameViewProj);
    g_rtTextureSpaceMotionVector[dtid] = int4(0, 0, 0, 0);
    g_rtTextureSpaceMotionVector[dtid] = int4((previousFramePosition.xy / previousFramePosition.w - currentFramePosition.xy / currentFramePosition.w) * int2(960, -540), 0, 0);
}