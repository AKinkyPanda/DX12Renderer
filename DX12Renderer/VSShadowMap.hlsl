cbuffer cbLightViewProj : register(b0)
{
    matrix gLightViewProj;
};

cbuffer cbWorld  : register(b1)
{
    matrix gWorld;
}

struct VertexInput
{
    float3 PosL : POSITION;
};

struct VertexOutput
{
    float4 PosH : SV_POSITION;
};

VertexOutput main(VertexInput IN)
{
    VertexOutput OUT;

    float4 worldPos = mul(float4(IN.PosL, 1.0f), gWorld);
    OUT.PosH = mul(worldPos, gLightViewProj);

    //OUT.PosH = mul(float4(IN.PosL, 1.0f), gLightViewProj);

    return OUT;
}