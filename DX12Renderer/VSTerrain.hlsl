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

cbuffer HeightWidth : register(b1)
{
    float4 heightWidth;
}

Texture2D<float4> heightmap : register(t0);

struct VertexInput
{
    float3 PosL : POSITION;
};

struct VertexOutput
{
    float4 PosH : SV_POSITION;
    float4 Normal : NORMAL;
    float2 UV : TEXCOORD;
};

VertexOutput main(float3 input : POSITION)
{
    VertexOutput output;
 
    float scale = heightWidth.x / 4;
    //float4 mysample = heightmap.Load(int3(input));
    float xCoord = input.x / heightWidth.x;
    float zCoord = input.z / heightWidth.y;
    float4 mysample = heightmap.Load(int3(xCoord * heightWidth.x, zCoord * heightWidth.y, 0));
    float4 worldPos = float4(input.x, mysample.r * scale, input.z, 1.0f);
    output.UV = float2(input.x / heightWidth.x, input.z / heightWidth.y);
    output.PosH = mul(Matrices.ModelViewProjectionMatrix, worldPos);
 
    // calculate vertex normal from heightmap
    float zb = heightmap.Load(int3(input.xz + int2(0, -1), 0)).r * scale;
    float zc = heightmap.Load(int3(input.xz + int2(1, 0), 0)).r * scale;
    float zd = heightmap.Load(int3(input.xz + int2(0, 1), 0)).r * scale;
    float ze = heightmap.Load(int3(input.xz + int2(-1, 0), 0)).r * scale;
 
    output.Normal = float4(normalize(float3(ze - zc, 1.0f, zb - zd)), 1.0f);
    output.Normal = float4(mul((float3x3)Matrices.InverseTransposeModelViewMatrix, output.Normal.xyz), 1.0f);
     
    return output;
}