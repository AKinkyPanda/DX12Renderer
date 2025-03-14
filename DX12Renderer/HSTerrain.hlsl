// Input control point
struct VS_OUTPUT
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
    float EdgeTessFactor[4]         : SV_TessFactor; // e.g. would be [4] for a quad domain
    float InsideTessFactor[2]          : SV_InsideTessFactor; // e.g. would be Inside[2] for a quad domain
};
 
#define NUM_CONTROL_POINTS 4
 
// Patch Constant Function
HS_CONSTANT_DATA_OUTPUT CalcHSPatchConstants(
    InputPatch<VS_OUTPUT, NUM_CONTROL_POINTS> ip,
    uint PatchID : SV_PrimitiveID)
{
    HS_CONSTANT_DATA_OUTPUT output;
 
    // Insert code to compute output here
    output.EdgeTessFactor[0] = 4;
    output.EdgeTessFactor[1] = 4;
    output.EdgeTessFactor[2] = 4;
    output.EdgeTessFactor[3] = 4;
    output.InsideTessFactor[0] = 4; 
    output.InsideTessFactor[1] = 4; 
 
    return output;
}
 
[domain("quad")]
[partitioning("fractional_even")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(4)]
[patchconstantfunc("CalcHSPatchConstants")]
HS_CONTROL_POINT_OUTPUT main( 
    InputPatch<VS_OUTPUT, NUM_CONTROL_POINTS> ip, 
    uint i : SV_OutputControlPointID,
    uint PatchID : SV_PrimitiveID )
{
    HS_CONTROL_POINT_OUTPUT output;
 
    // Insert code to compute Output here
    output.pos = ip[i].pos;
    output.norm = ip[i].norm;
    output.UV = ip[i].UV;
 
    return output;
}