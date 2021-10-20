#include "Raytracing.hlsli"

[shader("miss")]
void MyMissShader_ShadowRay(inout ShadowRayPayload payload)
{
    payload.hit = false;
}
