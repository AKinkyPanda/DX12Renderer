struct Mat
{
    matrix ModelMatrix;
    matrix ModelViewMatrix;
    matrix InverseTransposeModelViewMatrix;
    matrix ModelViewProjectionMatrix;
};

cbuffer MatCB : register(b0)
{
    Mat Matrices;
}

cbuffer CameraPosition : register(b1)
{
    float4 camPos;
}

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

    //OUT.Position = mul( Matrices.ModelViewProjectionMatrix, float4(IN.Position, 1.0f));
    OUT.Normal = float4(mul((float3x3)Matrices.InverseTransposeModelViewMatrix, IN.Normal), 1.0f);
    OUT.UV = IN.TexCoord;
    //OUT.FragPos = mul( Matrices.ModelViewMatrix, float4(IN.Position, 1.0f));

	// Use local vertex position as cubemap lookup vector.
	OUT.FragPos = IN.Position;

    OUT.Position = mul(Matrices.ModelViewProjectionMatrix, float4(IN.Position, 0.0f));
    OUT.Position.z = OUT.Position.w;

    return OUT;
}