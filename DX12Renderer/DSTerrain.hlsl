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
 
struct DS_OUTPUT
{
    float4 pos : SV_POSITION;
    float4 WorldPos : POSITION1;
    float4 norm : NORMAL;
    float2 UV : TEXCOORD;
};
 
// Output control point
struct HS_CONTROL_POINT_OUTPUT
{
    float4 pos : POSITION0;
    float4 norm : NORMAL;
    float2 UV : TEXCOORD;
};
 
// Output patch constant data.
struct HS_CONSTANT_DATA_OUTPUT
{
    float EdgeTessFactor[3]         : SV_TessFactor; // e.g. would be [4] for a quad domain
    float InsideTessFactor          : SV_InsideTessFactor; // e.g. would be Inside[2] for a quad domain
};
 
#define NUM_CONTROL_POINTS 3
 
[domain("tri")]
DS_OUTPUT main(
    HS_CONSTANT_DATA_OUTPUT input,
    float3 domain : SV_DomainLocation,
    const OutputPatch<HS_CONTROL_POINT_OUTPUT, NUM_CONTROL_POINTS> patch)
{
    DS_OUTPUT output;
 
    output.pos = float4(patch[0].pos.xyz*domain.x+patch[1].pos.xyz*domain.y+patch[2].pos.xyz*domain.z,1);
    //output.pos = mul(output.pos, Matrices.ModelViewProjectionMatrix);
    output.pos = mul(Matrices.ModelViewProjectionMatrix, output.pos);

    //float4 worldPos = float4(patch[0].WorldPos.xyz * domain.x +
    //                      patch[1].WorldPos.xyz * domain.y +
    //                      patch[2].WorldPos.xyz * domain.z, 1.0);
    //output.pos = mul(Matrices.ModelViewProjectionMatrix, worldPos);

        // Interpolate world positions from the control points
    float4 interpolatedWorldPos = patch[0].pos * domain.x +
                                  patch[1].pos * domain.y +
                                  patch[2].pos * domain.z;
    
    // Apply the MVP matrix once to get the final clip-space position
    output.pos = mul(Matrices.ModelViewProjectionMatrix, interpolatedWorldPos);

    output.norm = float4(patch[0].norm.xyz*domain.x + patch[1].norm.xyz*domain.y + patch[2].norm.xyz*domain.z, 1);
 
    return output;
}