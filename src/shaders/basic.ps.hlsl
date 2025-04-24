Texture2D<float4> Texture : register(t0, space2);
SamplerState Sampler : register(s0, space2);

struct Input
{
	float2 UV : TEXCOORD0;
	float4 Color : TEXCOORD1;
	float4 Position : SV_Position;
};

float4 main(Input input) : SV_Target0
{
	float4 color = Texture.Sample(Sampler, input.UV);
	color = color * input.Color;
	
	return color;
}