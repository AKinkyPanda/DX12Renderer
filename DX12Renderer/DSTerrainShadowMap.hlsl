struct Mat
{
    matrix ModelMatrix;
    matrix ModelViewMatrix;
    matrix InverseTransposeModelViewMatrix;
    matrix ModelViewProjectionMatrix;
};

cbuffer MatCB : register(b2)
{
    Mat Matrices;
}

cbuffer cbLightViewProj : register(b0)
{
    matrix gLightViewProj;
};

cbuffer ChunkOffset : register(b3)
{
    float4 chunkOffset;
}

Texture2D<float4> heightmap : register(t2);

SamplerState hmsampler : register(s0);

struct DS_OUTPUT
{
    float4 pos : SV_POSITION;
    float4 WorldPos : POSITION1;
    float4 ShadowPos : POSITION2;
    float4 norm : NORMAL;
    float2 UV : TEXCOORD;
    float3 FragPos : COLOR0;
};
 
// Output control point
struct HS_CONTROL_POINT_OUTPUT
{
    float4 pos : POSITION0;
    float4 norm : NORMAL;
    float2 UV : TEXCOORD;
    float3 FragPos : COLOR0;
};
 
// Output patch constant data.
struct HS_CONSTANT_DATA_OUTPUT
{
    float EdgeTessFactor[4]         : SV_TessFactor; // e.g. would be [4] for a quad domain
    float InsideTessFactor[2]          : SV_InsideTessFactor; // e.g. would be Inside[2] for a quad domain
};
 
#define NUM_CONTROL_POINTS 4
 
[domain("quad")]
DS_OUTPUT main(
    HS_CONSTANT_DATA_OUTPUT input,
    float2 domain : SV_DomainLocation,
    const OutputPatch<HS_CONTROL_POINT_OUTPUT, NUM_CONTROL_POINTS> patch)
{
    DS_OUTPUT output;

    float2 bottomUV = lerp(patch[0].UV, patch[1].UV, domain.x);
    float2 topUV    = lerp(patch[2].UV, patch[3].UV, domain.x);
    float2 interpolatedUV = lerp(bottomUV, topUV, domain.y);

    // ***changed from barycentric to (u,v) coordinates. Now need to use bilinear interpolation to find the new points.
    float4 bottom = lerp(patch[0].pos, patch[1].pos, domain.x);
    float4 top    = lerp(patch[2].pos, patch[3].pos, domain.x);
    float4 interpolatedWorldPos = lerp(bottom, top, domain.y);

    // Remove hardcoded values
    float scale = 1024.0f / 4.0f;

    float2 worldUV = (interpolatedWorldPos.xz - (chunkOffset.xy * chunkOffset.zw)) / 1024;
    //float2 worldUV = (interpolatedWorldPos.xz - chunkOffset.xy) / chunkOffset.zw;

    // Sample from the chunk�s local heightmap texture
    float displacement = heightmap.SampleLevel(hmsampler, worldUV, 0.0).r;

    // Apply height
    interpolatedWorldPos.y += displacement * scale;
    
    output.WorldPos = interpolatedWorldPos;
     
    output.pos = output.WorldPos;
    output.pos = mul(Matrices.ModelViewProjectionMatrix, output.pos);

    float4 shadowPos = mul(output.WorldPos, Matrices.ModelMatrix);
    output.ShadowPos = float4(shadowPos.xyz, 1);

    output.FragPos = mul( Matrices.ModelViewMatrix, interpolatedWorldPos);

    output.norm = lerp(lerp(patch[0].norm, patch[1].norm, domain.x), lerp(patch[2].norm, patch[3].norm, domain.x), domain.y);
    output.UV = worldUV;

    output.pos = mul(interpolatedWorldPos, gLightViewProj);

    return output;
}