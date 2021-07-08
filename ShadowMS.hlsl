#include "Raytracing.hlsl"

[shader("miss")]
void MyMissShader_ShadowRay(inout ShadowRayPayload rayPayload)
{
    rayPayload.hit = false;
    uint2 DTID = DispatchRaysIndex().xy;
}
