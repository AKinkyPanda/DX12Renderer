// Structure to hold the Model-View-Projection matrix
struct ModelViewProjection
{
    float4x4 MVP;  // Use float4x4 for the MVP matrix
};

// Declare the constant buffer
cbuffer ModelViewProjectionCB : register(b0)
{
    float4x4 MVP;  // Actual MVP matrix stored in the constant buffer
};

//ConstantBuffer<ModelViewProjection> ModelViewProjectionCB : register(b0);

struct VertexPosColor
{
    float3 Position : POSITION;
    float3 Normal    : NORMAL;
    float2 TexCoord : TEXCOORD;
};

struct VertexShaderOutput
{
    float4 Position : SV_Position;
    float4 Normal    : NORMAL;
    float2 UV : TEXCOORD0;   // Match this to the input in pixel shader
    float3 FragPos : COLOR0;
};

VertexShaderOutput main(VertexPosColor IN)
{
    VertexShaderOutput OUT;

    OUT.Position = mul(MVP, float4(IN.Position, 1.0f));
    OUT.Normal = float4(IN.Normal, 1.0f);
    OUT.UV = IN.TexCoord;  // Forward texture coordinates
    float3 frag = mul(MVP, float4(IN.Position, 0.0f));
	OUT.FragPos = frag;

    return OUT;
}