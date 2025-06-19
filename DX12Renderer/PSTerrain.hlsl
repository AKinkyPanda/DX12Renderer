struct PixelShaderInput
{
	float4 PosH : SV_Position;
    float4 WorldPos : POSITION1;
    float4 ShadowPos : POSITION2;
	float4 Normal : NORMAL;
    float2 UV : TEXCOORD;
    float3 FragPos : COLOR0;
};

struct PointLight
{
    float4 PositionWS; // Light position in world space.
    //----------------------------------- (16 byte boundary)
    float4 PositionVS; // Light position in view space.
    //----------------------------------- (16 byte boundary)
    float4 Color;
    //----------------------------------- (16 byte boundary)
    float       Intensity;
    float       Attenuation;
    float2      Padding;                // Pad to 16 bytes
    //----------------------------------- (16 byte boundary)
    // Total:                              16 * 4 = 64 bytes
};

struct SpotLight
{
    float4 PositionWS; // Light position in world space.
    //----------------------------------- (16 byte boundary)
    float4 PositionVS; // Light position in view space.
    //----------------------------------- (16 byte boundary)
    float4 DirectionWS; // Light direction in world space.
    //----------------------------------- (16 byte boundary)
    float4 DirectionVS; // Light direction in view space.
    //----------------------------------- (16 byte boundary)
    float4 Color;
    //----------------------------------- (16 byte boundary)
    float       Intensity;
    float       SpotAngle;
    float       Attenuation;
    float       Padding;                // Pad to 16 bytes.
    //----------------------------------- (16 byte boundary)
    // Total:                              16 * 6 = 96 bytes
};

struct DirectionalLight
{
    float4 DirectionWS; // Light direction in world space.
    //----------------------------------- (16 byte boundary)
    float4 DirectionVS; // Light direction in view space.
    //----------------------------------- (16 byte boundary)
    float4 Color;       // Light color.
    //----------------------------------- (16 byte boundary)
    float  Intensity;   // Light intensity.
    float3 Padding;     // Pad to 16 bytes.
    //----------------------------------- (16 byte boundary)
    // Total: 16 * 4 = 64 bytes
};

struct Material
{
    float4 DiffuseAlbedo;
    float3 FresnelR0;
    float Shininess;
};

struct LightProperties
{
    uint NumPointLights;
    uint NumSpotLights;
    uint NumDirectionalLights;
};

#define PI 3.14159265359

cbuffer LightPropertiesCB : register(b1)
{
    LightProperties LightPropertiesDelta;
}
cbuffer CameraPosition : register(b2)
{
    float4 camPos;
}
cbuffer cbLightSpace : register(b3)
{
    matrix gLightViewProj;
};

StructuredBuffer<PointLight> PointLights : register( t1 );
StructuredBuffer<SpotLight> SpotLights : register( t2 );
StructuredBuffer<DirectionalLight> DirectionalLights : register( t3 );

Texture2D<float4> heightmap : register(t0);
Texture2D<float4> ShadowMap : register(t4);
Texture2D<float4> GrassTex : register(t5);
Texture2D<float4> BlendTex : register(t6);
Texture2D<float4> RockTex : register(t7);

SamplerState hmsampler : register(s0);
SamplerState colorSampler : register(s1);
SamplerComparisonState ShadowSampler : register(s2);

float4 slope_based_color(float slope, float4 colorSteep, float4 colorFlat) {
    if (slope < 0.25f) {
        return colorFlat;
    }
 
    if (slope < 0.5f) {
        float blend = (slope - 0.25f) * (1.0f / (0.5f - 0.25f));
 
        return lerp(colorFlat, colorSteep, blend);
    } else {
        return colorSteep;
    }
}
// ----------------------------------------------------------------------------
float4 height_and_slope_based_color(float height, float slope, float2 UV) {
    //float4 grass = float4(0.22f, 0.52f, 0.11f, 1.0f);
    float4 grass = float4(GrassTex.Sample(colorSampler, UV).rgb, 1);
    //float4 dirt = float4(0.35f, 0.20f, 0.0f, 1.0f);
    float4 dirt = float4(BlendTex.Sample(colorSampler, UV).rgb, 1);
    float4 rock = float4(0.42f, 0.42f, 0.52f, 1.0f);
    float4 snow = float4(0.8f, 0.8f, 0.8f, 1.0f);
 
    float scale = 1024 / 4;
    float bounds = scale * 0.02f;
    float transition = scale * 0.6f;
    float greenBlendEnd = transition + bounds;
    float greenBlendStart = transition - bounds;
    float snowBlendEnd = greenBlendEnd + 2 * bounds;
 
    if (height < greenBlendStart) {
        // get grass/dirt values
        return slope_based_color(slope, dirt, grass);
    }
 
    if (height < greenBlendEnd) {
        // get both grass/dirt values and rock values and blend
        float4 c1 = slope_based_color(slope, dirt, grass);
        float4 c2 = rock;
     
        float blend = (height - greenBlendStart) * (1.0f / (greenBlendEnd - greenBlendStart));
         
        return lerp(c1, c2, blend);
    }
 
    if (height < snowBlendEnd) {
        // get rock values and rock/snow values and blend
        float4 c1 = rock;
        float4 c2 = slope_based_color(slope, rock, snow);
         
        float blend = (height - greenBlendEnd) * (1.0f / (snowBlendEnd - greenBlendEnd));
 
        return lerp(c1, c2, blend);
    }
 
    // get rock/snow values
    return slope_based_color(slope, rock, snow);
}
// ----------------------------------------------------------------------------
float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}
// ----------------------------------------------------------------------------
float3 fresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
// ----------------------------------------------------------------------------
float ShadowCalculation(float4 posWorld, float3 normal)
{
    float4 shadowCoord = mul(posWorld, gLightViewProj);
    shadowCoord /= shadowCoord.w;
    shadowCoord.xy = shadowCoord.xy * 0.5f + 0.5f;

    float depth = shadowCoord.z;
    float shadowDepth = ShadowMap.SampleCmpLevelZero(ShadowSampler, shadowCoord.xy, depth).r;

    return (depth <= shadowDepth + 0.001) ? 1.0 : 0.5; // Shadow Bias
}
// ----------------------------------------------------------------------------
float CalcShadowFactor(float4 shadowPosH, float3 normal, float3 lightDir)
{
    // Complete projection by doing division by w.
    shadowPosH.xyz /= shadowPosH.w;

    shadowPosH.xy = shadowPosH.xy * 0.5f + 0.5f;
    shadowPosH.y = 1 - shadowPosH.y;

    // Depth in NDC space.
    float depth = shadowPosH.z;

    // Apply depth bias to fix shadow acne / peter-panning.
    float bias = max(0.001f * (1.0f - dot(normal, lightDir)), 0.0005f); 
    depth -= bias;

    uint width, height, numMips;
    ShadowMap.GetDimensions(0, width, height, numMips);

    // Texel size.
    float dx = 1.0f / (float)width;

    float percentLit = 0.0f;
    const float2 offsets[9] =
    {
        float2(-dx,  -dx), float2(0.0f,  -dx), float2(dx,  -dx),
        float2(-dx, 0.0f),  float2(0.0f,  0.0f),  float2(dx, 0.0f),
        float2(-dx,  +dx), float2(0.0f,  +dx), float2(dx,  +dx)
    };

    [unroll]
    for(int i = 0; i < 9; ++i)
    {
        percentLit += ShadowMap.SampleCmpLevelZero(ShadowSampler,
            shadowPosH.xy + offsets[i], depth).r;
    }
    
    return percentLit / 9.0f;
}
// ----------------------------------------------------------------------------
// Schlick gives an approximation to Fresnel reflectance (see pg. 233 "Real-Time Rendering 3rd Ed.").
// R0 = ( (n-1)/(n+1) )^2, where n is the index of refraction.
float3 SchlickFresnel(float3 R0, float3 normal, float3 lightVec)
{
    float cosIncidentAngle = saturate(dot(normal, lightVec));

    float f0 = 1.0f - cosIncidentAngle;
    float3 reflectPercent = R0 + (1.0f - R0)*(f0*f0*f0*f0*f0);

    return reflectPercent;
}
float3 BlinnPhong(float3 lightStrength, float3 lightVec, float3 normal, float3 toEye, Material mat)
{
    const float m = mat.Shininess * 256.0f;
    float3 halfVec = normalize(toEye + lightVec);

    float roughnessFactor = (m + 8.0f)*pow(max(dot(halfVec, normal), 0.0f), m) / 8.0f;
    float3 fresnelFactor = SchlickFresnel(mat.FresnelR0, halfVec, lightVec);

    float3 specAlbedo = fresnelFactor*roughnessFactor;

    // Our spec formula goes outside [0,1] range, but we are 
    // doing LDR rendering.  So scale it down a bit.
    specAlbedo = specAlbedo / (specAlbedo + 1.0f);

    return (mat.DiffuseAlbedo.rgb + specAlbedo) * lightStrength;
}

float3 ComputeDirectionalLight(DirectionalLight L, Material mat, float3 normal, float3 toEye)
{
    // The light vector aims opposite the direction the light rays travel.
    float3 lightVec = normalize(L.DirectionWS.xyz); //L.DirectionVS;

    // Scale light down by Lambert's cosine law.
    float ndotl = max(dot(lightVec, normal), 0.0f);
    float3 lightStrength = L.Intensity * ndotl;

    return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}
// ------------------------------------------------------------------------------------------------------------------------------------
float4 main(PixelShaderInput input) : SV_TARGET
{
    float4 light = normalize(float4(1.0f, 1.0f, -1.0f, 1.0f));
    float diffuse = saturate(dot(input.Normal, light));
    float ambient = 0.1f;
    float3 color = float3(0.32f, 0.82f, 0.41f);

    // Color presets
    float4 grass = float4(0.22f, 0.52f, 0.11f, 1.0f);
    float4 dirt = float4(0.35f, 0.20f, 0.0f, 1.0f);
    float4 rock = float4(0.42f, 0.42f, 0.52f, 1.0f);
    float4 snow = float4(0.8f, 0.8f, 0.8f, 1.0f);

    // Calculate the slope of this point.
    //float slope = 1.0f - input.Normal.y;
    float slope = acos(input.Normal.y);
    float4 textureColor;

    float2 scaledUV = input.UV * 32.0f; //input.WorldPos.xz * 0.05f; - Adjust if necessary to 16, 64, etc. in the future if this looks off.
    textureColor = height_and_slope_based_color(input.WorldPos.y - 2000, slope - 0.2, scaledUV);

    //return float4(input.UV, 0, 1);

    float3 N = normalize( input.Normal );
    float3 V = normalize(camPos - input.FragPos);
    float3 F0 = float3(0.0, 0.0, 0.0); //Probably change this to PBR and stuff

    float shadowFactor = 0;
    //shadowFactor = ShadowCalculation(float4(IN.WorldPos, 1.0f), N);
    float4 shadowPosH = mul(float4(input.ShadowPos.xyz, 1.0f), gLightViewProj);
    shadowFactor = CalcShadowFactor(shadowPosH, N, DirectionalLights[0].DirectionWS.xyz);

    float3 result = 0.0f;
    Material mat = { float4(textureColor.xyz, 1), F0, 0.0f };

    result += ComputeDirectionalLight(DirectionalLights[0], mat, N, V) * shadowFactor;

    //float4 camPos = float4(-4300, 2000, 1500, 1);
    //float d = distance(input.WorldPos.xyz, camPos);
    //return float4(saturate(d / 2000.0f), saturate(d / 2000.0f), saturate(d / 2000.0f), 1.0);

    //return float4(input.WorldPos.y * 0.001, input.WorldPos.y * 0.001, input.WorldPos.y * 0.001, 1.0);

    //float4 debugColor = float4(input.WorldPos.xyz * 0.001, 1.0);  // Scale down for visibility
    //return debugColor;
     
    return float4(saturate((result * textureColor.rgb) + (result * ambient)), 1.0f);
}