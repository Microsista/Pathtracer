#define HLSL
#include "RaytracingShaderHelper.hlsli"
#include "RayTracingHlslCompat.h"

RWTexture2D<float4> g_renderTarget : register(u0);
Texture2D<float3> g_reflectionBuffer : register(t0);
Texture2D<float3> g_shadowBuffer : register(t1);
Texture2D<float3> g_inNormalDepth : register(t2);
ConstantBuffer<AtrousWaveletTransformFilterConstantBuffer> cb: register(b0);

static float e = 2.71828182846f;

bool IsWithinBounds(in int2 index, in int2 dimensions) {
	return index.x >= 0 && index.y >= 0 && index.x < dimensions.x < dimensions.x&& index.y < dimensions.y;
}

float DepthThreshold(float depth, float2 ddxy, float2 pixelOffset) {
	float depthThreshold;

	/*if (cb.perspectiveCorrectDepthInterpolation) {
		float2
	}*/
	depthThreshold = dot(1, abs(pixelOffset * ddxy));

	return depthThreshold;
}

[numthreads(8, 8, 1)]
void main(int3 groupThreadID : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID)
{
	float4 color = float4(0, 0, 0, 0);
	float depth = g_inNormalDepth[DTid.xy].x;
	//float prevDepth = g

	//float depthThreshold = DepthThreshold(depth, ddxy, pixelOffsetForDepth);

	int blurRadius = 4;
	float weights[] = {
		0.05f, 0.09f, 0.12f, 0.15f, 0.16f, 0.15f, 0.12f, 0.09f, 0.05f
	};
	float weight = 1 / ((2.0f * blurRadius + 1.0f) * (2.0f * blurRadius + 1.0f));
	[unroll]
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

			color += weights[j + blurRadius] * weights[i + blurRadius] * float4(g_reflectionBuffer[DTid.xy + uint2(x, y)], 0.0f);
		}
	}
	//color += float4(g_reflectionBuffer[DTid.xy].x, g_reflectionBuffer[DTid.xy].y, g_reflectionBuffer[DTid.xy].z, 1.0f);

	float shadow = 0.0f;
	int shadowBlurRadius = 6;
	float shadowWeight = 1 / ((2.0f * shadowBlurRadius + 1.0f) * (2.0f * shadowBlurRadius + 1.0f));
	float shadowWeights[] = {
		0.05f, 0.05f, 0.05f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.01f, 0.05f, 0.05f, 0.05f
	};
	[unroll]
	for (int i = -shadowBlurRadius; i <= shadowBlurRadius; i++) {
		for (int j = -shadowBlurRadius; j <= shadowBlurRadius; j++) {
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

			shadow += shadowWeights[j + shadowBlurRadius] * shadowWeights[i + shadowBlurRadius] * g_shadowBuffer[DTid.xy + uint2(x, y)].x;
		}
	}

	g_renderTarget[DTid.xy] += color;
	g_renderTarget[DTid.xy] *= (-shadow + 1);


	//g_renderTarget[DTid.xy] = float4(g_inNormalDepth[DTid.xy].x, g_inNormalDepth[DTid.xy].y, g_inNormalDepth[DTid.xy].z, 1.0f);
	/*float dd = abs(g_inNormalDepth[DTid.xy].x - g_inNormalDepth[DTid.xy + uint2 (1, 0)].x);
	g_renderTarget[DTid.xy] = float4(dd, dd, dd, 0.0f);*/
}
