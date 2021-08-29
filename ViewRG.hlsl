#include "Raytracing.hlsl"

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

    //float4 currentClip = mul(info.prevHit, inverse(g_sceneCB.projectionToWorld));
    //float2 current = (currentClip.xy / currentClip.w) * float2(0.5f, -0.5f) + 0.5f;

    //float4 previousClip = mul(info.prevHit, g_sceneCB.prevFrameViewProj);
    //float2 previous = (previousClip.xy / previousClip.w) * float2(0.5f, -0.5f) + 0.5f;

    //float2 motionVector = previous - current;







   // float _depth;
   // 
   // //g_rtReprojectedNormalDepth[DTid] = EncodeNormalDepth(DecodeNormal(rayPayload.AOGBuffer._encodedNormal), _depth);
  float4 currentFramePosition = mul(float4(info.prevHit, 1.0f), inverse(g_sceneCB.projectionToWorld));
   float4 previousFramePosition =  mul(float4(info.prevHit, 1.0f), g_sceneCB.prevFrameViewProj);
   // //float3 previousFramePosition = mul(float4(info.prevHit, 1.0f), g_sceneCB.prevFrameViewProj).xyz;
  float2 motionVector = (currentFramePosition.xy / currentFramePosition.w - previousFramePosition.xy / previousFramePosition.w) * float2(2.0f, -2.0f)+ float2(0.5f, 0.5f);
 // motionVector = motionVector/* * float2(1.0f, -1.0f)*/ + float2(0.5f, 0.0F);
   // float2 motionVector = CalculateMotionVector(info.prevHit, _depth, DTID);



   ///* float4 clipSpacePosition = mul(float4(previousFramePositio.xy, 1), g_sceneCB.prevFrameViewProj);
   // float2 texturePosition = ClipSpaceToTexturePosition(_clipSpacePosition);*/

   //


   /* float2 motionVector = CalculateMotionVector(previousFramePosition, _depth, DTID);
    g_rtTextureSpaceMotionVector[DTID] = motionVector;*/
    g_rtTextureSpaceMotionVector[DTID] = motionVector;
    g_previousFrameHitPosition[DispatchRaysIndex().xy] = info.prevHit;
    float3 inShadow = info.inShadow;
    float3 color = info.color;
    g_shadowBuffer[DTID] = inShadow;
    g_renderTarget[DTID] = color;//float3(motionVector, 0.0f);
    g_normalDepth[DTID] = float3(info.depth, info.depth, info.depth);
}