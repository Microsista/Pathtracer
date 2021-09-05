#define HLSL
#include "RaytracingShaderHelper.hlsli"
#include "RayTracingHlslCompat.h"

RWTexture2D<float4> g_renderTarget : register(u0);
Texture2D<float3> g_reflectionBuffer : register(t0);
Texture2D<float3> g_shadowBuffer : register(t1);

RWTexture2D<float4> g_prevFrame : register(u1);
RWTexture2D<float4> g_prevReflection : register(u2);
RWTexture2D<float4> g_prevShadow : register(u3);

Texture2D<float3> g_inNormalDepth : register(t2);

ConstantBuffer<AtrousWaveletTransformFilterConstantBuffer> cb: register(b0);
ConstantBuffer<SceneConstantBuffer> g_sceneCB : register(b1);

RWTexture2D<float2> motionBuffer : register(u4);

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
	float4 reflection = 0.0f;
	float shadow = 0.0f;

	int blurRadius = 4;
	float weights[] = {
		0.05f, 0.09f, 0.12f, 0.15f, 0.16f, 0.15f, 0.12f, 0.09f, 0.05f
	};
	for (int i = -blurRadius; i <= blurRadius; i++) {
		for (int j = -blurRadius; j <= blurRadius; j++) {
			int x = j, y = i;
			if (DTid.x + j < 0)
				x = 0;
			if (DTid.x + j > cb.textureDim.x)
				x = 0;
			if (DTid.y + i < 0)
				y = 0;
			if (DTid.y + i > cb.textureDim.y)
				y = 0;

			if (abs(g_inNormalDepth[DTid.xy].x - g_inNormalDepth[DTid.xy + uint2(x, y)].x) > 0.01f) {
				x = 0;
				y = 0;
			}

			reflection += weights[j + blurRadius] * weights[i + blurRadius] * float4(g_reflectionBuffer[DTid.xy + uint2(x, y)], 0.0f);
			shadow += weights[j + blurRadius] * weights[i + blurRadius] * g_shadowBuffer[DTid.xy + uint2(x, y)].x;
		}
	}

	/*if (!matEqual(inverse(g_sceneCB.projectionToWorld), g_sceneCB.prevFrameViewProj)) {
		g_prevReflection[DTid.xy] = reflection;
		g_prevShadow[DTid.xy] = shadow;
		g_renderTarget[DTid.xy] += reflection;
		g_renderTarget[DTid.xy] *= (-shadow + 1);
		g_prevFrame[DTid.xy] = g_renderTarget[DTid.xy];
	}*/

	//reflection = (reflection + 7 * g_prevReflection[DTid.xy]) / 8;
	g_renderTarget[DTid.xy] += reflection;

	//shadow = (shadow + 7 * g_prevShadow[DTid.xy].x) / 8;
	g_renderTarget[DTid.xy] *= (-shadow + 1);
	float2 m = (motionBuffer[DTid.xy] - 0.5f) *cb.textureDim;
	if (m.x < 0.1f) m.x = 0.0f;
	if (m.y < 0.1f) m.y = 0.0f;

	g_renderTarget[DTid.xy] =(g_renderTarget[DTid.xy] + 7 * g_prevFrame[int2(m  + float2(DTid.xy))]) / 8;

	g_prevFrame[DTid.xy] = g_renderTarget[DTid.xy];
	g_prevReflection[DTid.xy] = reflection;
	g_prevShadow[DTid.xy] = shadow;
	//if (m.x >= 0.1f)
	//{
	//	g_renderTarget[DTid.xy] = 1.0f;
	//}
	//else if (m.x < -0.1f)
	//{
	//	g_renderTarget[DTid.xy] = 0.0f;
	//}
	//else
	//{
	//	g_renderTarget[DTid.xy] = 0.5f;
	//}
	//g_renderTarget[DTid.xy] = float4(motionBuffer[DTid.xy], 0.0f, 1.0f);
}
