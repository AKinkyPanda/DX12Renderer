struct PixelShaderInput
{
	float4 Position : SV_Position;
    float4 Normal   : NORMAL;
    float2 UV : TEXCOORD0;  // Matches TEXCOORD0 from the vertex shader
    float3 FragPos : COLOR0;
};

struct PixelOutput
{
    float4 Color : SV_Target0;
};

TextureCube gCubeMap : register(t0);

SamplerState Sampler : register(s0);

PixelOutput main(PixelShaderInput IN) : SV_Target0
{
    PixelOutput OUT;

    OUT.Color = gCubeMap.Sample(Sampler, IN.FragPos);

    return OUT;
}