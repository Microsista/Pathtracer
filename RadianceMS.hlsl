#include "Raytracing.hlsli"

[shader("miss")]
void MyMissShader(inout RayPayload payload)
{
    payload.color = BackgroundColor;
}