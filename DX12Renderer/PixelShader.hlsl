struct PixelShaderInput
{
	float4 Position : SV_Position;
    float4 Normal   : NORMAL;
    float2 UV : TEXCOORD0;  // Matches TEXCOORD0 from the vertex shader
    float3 FragPos : COLOR0;
    float3 WorldPos : COLOR1;
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

struct LightResult
{
    float4 Diffuse;
    float4 Specular;
};

struct PixelOutput
{
    float4 Color : SV_Target0;
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
StructuredBuffer<PointLight> PointLights : register( t0 );
StructuredBuffer<SpotLight> SpotLights : register( t1 );
StructuredBuffer<DirectionalLight> DirectionalLights : register( t2 );
Texture2D Diffuse : register( t3 );
Texture2D Normal : register( t4 );
Texture2D Metallic : register( t5 );
Texture2D Roughness : register( t6 );
Texture2D AO : register( t7 );
Texture2D Opacity : register( t8 );
Texture2D<float> ShadowMap : register( t9 );


SamplerState Sampler : register(s0);
SamplerComparisonState ShadowSampler : register(s1);

float3 getNormalFromMap(PixelShaderInput IN)
{
    float3 tangentNormal = Normal.Sample(Sampler, IN.UV).xyz * 2.0 - 1.0;

    float3 Q1  = ddx(IN.FragPos);
    float3 Q2  = ddy(IN.FragPos);
    float2 st1 = ddx(IN.UV);
    float2 st2 = ddy(IN.UV);

    float3 N   = normalize(IN.Normal.xyz);
    float3 T  = normalize(Q1*st2.x - Q2*st1.x);
    float3 B  = -normalize(cross(N, T));

    // Transform the tangent-space normal to world space using direct multiplication
    float3 worldNormal = normalize(
        tangentNormal.x * T +
        tangentNormal.y * B +
        tangentNormal.z * N
    );

    // Return the normalized world-space normal
    return worldNormal;
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
float CalcShadowFactor(float4 shadowPosH)
{
    // Complete projection by doing division by w.
    shadowPosH.xyz /= shadowPosH.w;

    shadowPosH.xy = shadowPosH.xy * 0.5f + 0.5f;
    shadowPosH.y = 1 - shadowPosH.y;

    // Depth in NDC space.
    float depth = shadowPosH.z;

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
    float3 lightVec = L.DirectionVS;

    // Scale light down by Lambert's cosine law.
    float ndotl = max(dot(lightVec, normal), 0.0f);
    float3 lightStrength = L.Intensity * ndotl;

    return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}
// ------------------------------------------------------------------------------------------------------------------------------------
PixelOutput main(PixelShaderInput IN) : SV_Target0
{
    PixelOutput OUT;

    float3 albedo   = pow(Diffuse.Sample(Sampler, IN.UV).rgb, 2.2);
    float metallic  = Metallic.Sample(Sampler, IN.UV).r;
    float roughness = Roughness.Sample(Sampler, IN.UV).r;
    float ao        = AO.Sample(Sampler, IN.UV).r;
    float opacity = Opacity.Sample(Sampler, IN.UV).r;

    float3 N = getNormalFromMap(IN); //normalize( IN.Normal );
    float3 V = normalize(camPos - IN.FragPos);

    // calculate reflectance at normal incidence; if dia-electric (like plastic) use F0 
    // of 0.04 and if it's a metal, use the albedo color as F0 (metallic workflow)    
    float3 F0 = float3(0.04, 0.04, 0.04); 
    F0 = lerp(albedo, albedo, metallic);

    // reflectance equation
    float3 Lo = float3(0.0, 0.0, 0.0);
    for(int i = 0; i < LightPropertiesDelta.NumPointLights; ++i) 
    {
        // calculate per-light radiance
        float3 L = normalize( PointLights[i].PositionVS - IN.FragPos );
        float3 H = normalize(V + L);
        float distance = length( PointLights[i].PositionVS - IN.FragPos );
        float attenuation = 1.0 / (distance * distance);
        float3 radiance = PointLights[i].Color * PointLights[i].Intensity * attenuation * 1000;

        // Cook-Torrance BRDF
        float NDF = DistributionGGX(N, H, roughness);   
        float G   = GeometrySmith(N, V, L, roughness);      
        float3 F    = fresnelSchlick(clamp(dot(H, V), 0.0, 1.0), F0);
           
        float3 numerator    = NDF * G * F; 
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; // + 0.0001 to prevent divide by zero
        float3 specular = numerator / denominator;
        
        // kS is equal to Fresnel
        float3 kS = F;
        float3 kD = float3(1.0, 1.0, 1.0) - kS;
        kD *= 1.0 - metallic;	  

        // scale light by NdotL
        float NdotL = max(dot(N, L), 0.0);        

        // add to outgoing radiance Lo
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    float shadowFactor = 0;
    //shadowFactor = ShadowCalculation(float4(IN.WorldPos, 1.0f), N);
    float4 shadowPosH = mul(float4(IN.WorldPos, 1.0f), gLightViewProj);
    shadowFactor = CalcShadowFactor(shadowPosH);

    float3 result = 0.0f;
    Material mat = { float4(albedo, 1), F0, 0.0f };

    for(int i = 0; i < LightPropertiesDelta.NumDirectionalLights; ++i)
    {
        //result += ComputeDirectionalLight(DirectionalLights[i], mat, N, V) * shadowFactor;

        // The light vector aims opposite the direction the light rays travel.
        float3 L = DirectionalLights[i].DirectionVS;
        float3 H = normalize(V + L);

        // Scale light down by Lambert's cosine law.
        float ndotl = max(dot(L, N), 0.0f);
        float3 radiance = DirectionalLights[i].Color * DirectionalLights[i].Intensity * ndotl;

        // Cook-Torrance BRDF
        float NDF = DistributionGGX(N, H, roughness);   
        float G   = GeometrySmith(N, V, L, roughness);      
        float3 F    = fresnelSchlick(clamp(dot(H, V), 0.0, 1.0), F0);
           
        float3 numerator    = NDF * G * F; 
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; // + 0.0001 to prevent divide by zero
        float3 specular = numerator / denominator;
        
        // kS is equal to Fresnel
        float3 kS = F;
        float3 kD = float3(1.0, 1.0, 1.0) - kS;
        kD *= 1.0 - metallic;	  

        // scale light by NdotL
        float NdotL = max(dot(N, L), 0.0);        

        // add to outgoing radiance Lo
        Lo += (kD * albedo / PI + specular) * radiance * NdotL * shadowFactor;
    }

    float4 FinalResult = float4(result, 0);

    // ambient lighting (note that the next IBL tutorial will replace 
    // this ambient lighting with environment lighting).
    float3 ambient = float3(0.0003, 0.0003, 0.0003) * albedo * ao;

    float3 color = ambient + (Lo * shadowFactor);
    //float3 color = ambient + FinalResult.xyz;

    // HDR tonemapping
    color = color / (color + float3(1.0, 1.0, 1.0));
    // gamma correct
    color = pow(color, float3(1.0/2.2, 1.0/2.2, 1.0/2.2)); 

    OUT.Color = float4(color, 1.0);
    return OUT;
}