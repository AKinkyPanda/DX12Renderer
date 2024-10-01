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
    float3 Color    : COLOR;
};

struct VertexShaderOutput
{
    float4 Color    : COLOR;
    float4 Position : SV_Position;
};

VertexShaderOutput main(VertexPosColor IN)
{
    VertexShaderOutput OUT;

    OUT.Position = mul(MVP, float4(IN.Position, 1.0f));
    OUT.Color = float4(IN.Color, 1.0f);

    return OUT;
}