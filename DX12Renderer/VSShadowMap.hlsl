cbuffer cbLightViewProj : register(b0)
{
    matrix gLightViewProj;
};

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
    OUT.PosH = mul(float4(IN.PosL, 1.0f), gLightViewProj);
    return OUT;
}