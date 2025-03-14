cbuffer CameraPosition : register(b3)
{
    float4 camPos;
}

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

// We add the following function to calculate what tessellation factor to use at
// what distance. Factors used are all factors of 2. ie 1, 2, 4, 8, 16, 32, 64 (64 being max)
// because we're using [partitioning("fractional_even")], the tessellator should automatically
// interpolate between these values, taking care of potential popping concerns.
float CalcTessFactor(float3 p) {
    float d = distance(p, camPos);
 
    float s = saturate((d - 16.0f) / (512.0f - 16.0f));
    return pow(2, (lerp(6, 0, s)));
}
 
// Patch Constant Function
HS_CONSTANT_DATA_OUTPUT CalcHSPatchConstants(
    InputPatch<VS_OUTPUT, NUM_CONTROL_POINTS> ip,
    uint PatchID : SV_PrimitiveID)
{
    HS_CONSTANT_DATA_OUTPUT output;
 
    // tessellate based on distance from the camera.
    // compute tess factor based on edges.
    // compute midpoint of edges.
    float3 e0 = 0.5f * (ip[0].WorldPos + ip[2].WorldPos);
    float3 e1 = 0.5f * (ip[0].WorldPos + ip[1].WorldPos);
    float3 e2 = 0.5f * (ip[1].WorldPos + ip[3].WorldPos);
    float3 e3 = 0.5f * (ip[2].WorldPos + ip[3].WorldPos);
    float3 c = 0.25f * (ip[0].WorldPos + ip[1].WorldPos + ip[2].WorldPos + ip[3].WorldPos);
 
    output.EdgeTessFactor[0] = CalcTessFactor(e0);
    output.EdgeTessFactor[1] = CalcTessFactor(e1);
    output.EdgeTessFactor[2] = CalcTessFactor(e2);
    output.EdgeTessFactor[3] = CalcTessFactor(e3);
    output.InsideTessFactor[0] = CalcTessFactor(c);
    output.InsideTessFactor[1] = output.InsideTessFactor[0];
 
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