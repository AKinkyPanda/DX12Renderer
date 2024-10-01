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

Texture2D TextureCoordinate : register(t0);
SamplerState Sampler : register(s0);

PixelOutput main(PixelShaderInput IN) : SV_Target0
{
    //float4 Output;
    //float4 albedo = Color.Sample(Sampler, IN.UV);

    //Output = albedo;

	//return Output;

    // Sample the texture using the interpolated texture coordinates
    //float4 albedo = Color.Sample(MySampler, IN.UV);

    //return albedo;

    PixelOutput OUT;

    float4 albedo = Color.Sample(Sampler, IN.UV);
    OUT.Color = albedo;

    OUT.Color = IN.Normal;
	return OUT;
}