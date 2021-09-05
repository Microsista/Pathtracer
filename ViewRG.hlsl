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

    float4 currentFramePosition = mul(float4(payload.prevHitPosition, 1.0f), inverse(g_sceneCB.projectionToWorld));
    float4 previousFramePosition = mul(float4(payload.prevHitPosition, 1.0f), g_sceneCB.prevFrameViewProj);
    g_rtTextureSpaceMotionVector[dtid] = float2(0.0f, 0.0f);
    g_rtTextureSpaceMotionVector[dtid] = (currentFramePosition.xy / currentFramePosition.w - previousFramePosition.xy / previousFramePosition.w) * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);
}