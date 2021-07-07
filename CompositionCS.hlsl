RWTexture2D<float4> g_renderTarget : register(u0);
Texture2D<float3> g_reflectionBuffer : register(t0);
Texture2D<float3> g_shadowBuffer : register(t1);

[numthreads(8, 8, 1)]
void main(int3 groupThreadID : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID)
{
	float4 color = float4(0,0,0, 0);
	int blurRadius = 5;
	float weight = 1 / ((2.0f * blurRadius + 1.0f) * (2.0f * blurRadius + 1.0f));
	[unroll]
	for (int i = -blurRadius; i <= blurRadius; i++) {
		for (int j = -blurRadius; j <= blurRadius; j++) {
			color += weight * float4(g_reflectionBuffer[DTid.xy + uint2(j, i)], 0.0f);
		}
	}

	float shadow = 0.0f;
	int shadowblurRadius = 7;
	float shadowWeight = 1 / ((2.0f * shadowblurRadius + 1.0f) * (2.0f * shadowblurRadius + 1.0f));
	[unroll]
	for (int i = -shadowblurRadius; i <= shadowblurRadius; i++) {
		for (int j = -shadowblurRadius; j <= shadowblurRadius; j++) {
			shadow += shadowWeight * g_shadowBuffer[DTid.xy + uint2(j, i)].x;
		}
	}

	g_renderTarget[DTid.xy] += color;
	g_renderTarget[DTid.xy] *= (-shadow + 1);
	//g_renderTarget[DTid.xy] = g_shadowBuffer[DTid.xy];
}