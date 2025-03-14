struct PixelShaderInput
{
	float4 PosH : SV_Position;
    float4 WorldPos : POSITION1;
	float4 Normal : NORMAL;
    float2 UV : TEXCOORD;
};

Texture2D<float4> heightmap : register(t0);
Texture2D<float4> shadowMap : register(t1);

SamplerState hmsampler : register(s0);
SamplerComparisonState ShadowSampler : register(s1);

float4 main(PixelShaderInput input) : SV_TARGET
{
    float4 light = normalize(float4(1.0f, 1.0f, -1.0f, 1.0f));
    float diffuse = saturate(dot(input.Normal, light));
    float ambient = 0.1f;
    float3 color = float3(0.32f, 0.82f, 0.41f);
     
    return float4(saturate((color * diffuse) + (color * ambient)), 1.0f);
}