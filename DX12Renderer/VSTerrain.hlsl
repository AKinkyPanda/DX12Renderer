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
    float4 WorldPos : POSITION1;
    //float4 ShadowPos : POSITION2;
    float4 Normal : NORMAL;
    float2 UV : TEXCOORD;
};

VertexOutput main(float3 input : POSITION)
{
    VertexOutput output;
 
    float scale = heightWidth.x / 4;
    float4 worldPos = float4(input.x, 0, input.z, 1.0f);
    output.WorldPos = worldPos;
    float2 uv = float2(input.x / heightWidth.x, input.z / heightWidth.y);
    float4 sample = heightmap.Load(int3(uv * float2(heightWidth.x, heightWidth.y), 0));
    worldPos = float4(input.x, sample.r * scale, input.z, 1.0f);
    output.WorldPos = mul(Matrices.ModelViewProjectionMatrix, worldPos);
    output.UV = float2(input.x / heightWidth.x, input.z / heightWidth.y);
    //output.PosH = mul(Matrices.ModelViewProjectionMatrix, worldPos);
    output.PosH = float4(input.x, 0, input.z, 1.0f);
 
    // calculate vertex normal from heightmap
    float zb = heightmap.Load(int3(input.xz + int2(0, -1), 0)).r * scale;
    float zc = heightmap.Load(int3(input.xz + int2(1, 0), 0)).r * scale;
    float zd = heightmap.Load(int3(input.xz + int2(1, 1), 0)).r * scale;
    float ze = heightmap.Load(int3(input.xz + int2(0, 1), 0)).r * scale;
    float zf = heightmap.Load(int3(input.xz + int2(-1, 0), 0)).r * scale;
    float zg = heightmap.Load(int3(input.xz + int2(-1, -1), 0)).r * scale;
 
    float x = 2 * zf + zc + zg - zb - 2 * zc - zd;
    float y = 6.0f;
    float z = 2 * zb + zc + zg - zd - 2 * ze - zf;
 
    output.Normal = float4(normalize(float3(x, y, z)), 1.0f);
     
    return output;
}