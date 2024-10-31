struct PixelShaderInput
{
	float4 Position : SV_Position;
    float4 Normal   : NORMAL;
    float2 UV : TEXCOORD0;  // Matches TEXCOORD0 from the vertex shader
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

struct LightProperties
{
    uint NumPointLights;
    uint NumSpotLights;
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


cbuffer LightPropertiesCB : register(b1)
{
    LightProperties LightPropertiesDelta;
}
StructuredBuffer<PointLight> PointLights : register( t0 );
StructuredBuffer<SpotLight> SpotLights : register( t1 );
Texture2D Diffuse : register( t2 );

SamplerState Sampler : register(s0);

// Helper function for Blinn-Phong lighting
float4 CalculateBlinnPhong(float3 normal, float3 lightDir, float3 viewDir, float3 lightColor, float intensity)
{
    float3 AmbientColor = (0.1, 0.1, 0.1);
    // Ambient
    float3 ambient = AmbientColor * lightColor * intensity;

    float3 DiffuseColor = (0.1, 0.1, 0.1);
    // Diffuse
    float3 diffuse = max(dot(normal, lightDir), 0.0) * DiffuseColor * lightColor * intensity;

    float3 SpecularColor = (0.1, 0.1, 0.1);
    // Specular
    float3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 0.3f);
    float3 specular = spec * SpecularColor * lightColor * intensity;

    return float4(ambient + diffuse + specular, 1.0);
}

float3 LinearToSRGB( float3 x )
{
    // This is exactly the sRGB curve
    //return x < 0.0031308 ? 12.92 * x : 1.055 * pow(abs(x), 1.0 / 2.4) - 0.055;

    // This is cheaper but nearly equivalent
    return x < 0.0031308 ? 12.92 * x : 1.13005 * sqrt( abs( x - 0.00228 ) ) - 0.13448 * x + 0.005719;
}

float DoDiffuse( float3 N, float3 L )
{
    return max( 0, dot( N, L ) );
}

float DoSpecular( float3 V, float3 N, float3 L )
{
    float3 R = normalize( reflect( -L, N ) );
    float RdotV = max( 0, dot( R, V ) );

    return pow( RdotV, 10 );
}

float DoAttenuation( float attenuation, float distance )
{
    return 1.0f / ( 1.0f + attenuation * distance * distance );
}

float DoSpotCone( float3 spotDir, float3 L, float spotAngle )
{
    float minCos = cos( spotAngle );
    float maxCos = ( minCos + 1.0f ) / 2.0f;
    float cosAngle = dot( spotDir, -L );
    return smoothstep( minCos, maxCos, cosAngle );
}

LightResult DoPointLight( PointLight light, float3 V, float3 P, float3 N )
{
    LightResult result;
    float3 L = ( light.PositionVS.xyz - P );
    float d = length( L );
    L = L / d;

    float attenuation = DoAttenuation( light.Attenuation, d );

    result.Diffuse = DoDiffuse( N, L ) * attenuation * light.Color * light.Intensity;
    result.Specular = DoSpecular( V, N, L ) * attenuation * light.Color * light.Intensity;

    return result;
}

LightResult DoSpotLight( SpotLight light, float3 V, float3 P, float3 N )
{
    LightResult result;
    float3 L = ( light.PositionVS.xyz - P );
    float d = length( L );
    L = L / d;

    float attenuation = DoAttenuation( light.Attenuation, d );

    float spotIntensity = DoSpotCone( light.DirectionVS.xyz, L, light.SpotAngle );

    result.Diffuse = DoDiffuse( N, L ) * attenuation * spotIntensity * light.Color * light.Intensity;
    result.Specular = DoSpecular( V, N, L ) * attenuation * spotIntensity * light.Color * light.Intensity;

    return result;
}

LightResult DoLighting( float3 P, float3 N )
{
    uint i;

    // Lighting is performed in view space.
    float3 V = normalize( -P );

    LightResult totalResult = (LightResult)0;

    for ( i = 0; i < LightPropertiesDelta.NumPointLights; ++i )
    {
        LightResult result = DoPointLight( PointLights[i], V, P, N );

        totalResult.Diffuse += result.Diffuse;
        totalResult.Specular += result.Specular;
    }

    for ( i = 0; i < LightPropertiesDelta.NumSpotLights; ++i )
    {
        LightResult result = DoSpotLight( SpotLights[i], V, P, N );

        totalResult.Diffuse += result.Diffuse;
        totalResult.Specular += result.Specular;
    }

    //totalResult.Diffuse = saturate( totalResult.Diffuse );
    //totalResult.Specular = saturate( totalResult.Specular );

    return totalResult;
}

PixelOutput main(PixelShaderInput IN) : SV_Target0
{
    PixelOutput OUT;

    LightResult lit = DoLighting( IN.Position.xyz, normalize( IN.Normal ) );

    float4 emissive = float4(0, 0, 0, 1);
    float4 ambient = float4(0.1, 0.1, 0.1, 1);
    float4 diffuse = float4(0.58, 0.58, 0.58, 1) * lit.Diffuse;
    float4 specular = float4(1, 1, 1, 1) * lit.Specular;
    float4 texColor = Diffuse.Sample( Sampler, IN.UV );

    OUT.Color = ( emissive + ambient + diffuse + specular ) * texColor;
    return OUT;
}