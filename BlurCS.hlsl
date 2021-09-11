RWTexture2D<float4> g_renderTarget : register(u0);
RWTexture2D<float4> g_prevFrame : register(u1);

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
	g_prevFrame[DTid.xy] = g_renderTarget[DTid.xy];
}
