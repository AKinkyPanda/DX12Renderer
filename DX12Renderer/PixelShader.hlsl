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
    float  ConstantAttenuation;
    float  LinearAttenuation;
    float  QuadraticAttenuation;
    float  Padding;
    //----------------------------------- (16 byte boundary)
    // Total:                              16 * 4 = 64 bytes
};

struct PointLightDos
{
    float4    PositionWS; // Light position in world space.
    //----------------------------------- (16 byte boundary)
    float4    PositionVS; // Light position in view space.
    //----------------------------------- (16 byte boundary)
    float4    Color;
    //----------------------------------- (16 byte boundary)
    float       Intensity;
    float       Attenuation;
    float       Padding[2];             // Pad to 16 bytes.
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
    float  SpotAngle;
    float  ConstantAttenuation;
    float  LinearAttenuation;
    float  QuadraticAttenuation;
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

    //return pow( RdotV, MaterialCB.SpecularPower );
    return pow( RdotV, 10.f );
}

float DoAttenuation( float c, float l, float q, float d )
{
    return 1.0f / ( c + l * d + q * d * d );
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

    float attenuation = DoAttenuation( 1,
                                       0.08f,
                                       0,
                                       d );

    result.Diffuse = DoDiffuse( N, L ) * attenuation * light.Color;
    result.Specular = DoSpecular( V, N, L ) * attenuation * light.Color;

    return result;
}

LightResult DoSpotLight( SpotLight light, float3 V, float3 P, float3 N )
{
    LightResult result;
    float3 L = ( light.PositionVS.xyz - P );
    float d = length( L );
    L = L / d;

    float attenuation = DoAttenuation( light.ConstantAttenuation,
                                       light.LinearAttenuation,
                                       light.QuadraticAttenuation,
                                       d );

    float spotIntensity = DoSpotCone( light.DirectionVS.xyz, L, light.SpotAngle );

    result.Diffuse = DoDiffuse( N, L ) * attenuation * spotIntensity * light.Color;
    result.Specular = DoSpecular( V, N, L ) * attenuation * spotIntensity * light.Color;

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

    totalResult.Diffuse = saturate( totalResult.Diffuse );
    totalResult.Specular = saturate( totalResult.Specular );

    return totalResult;
}

PixelOutput main(PixelShaderInput IN) : SV_Target0
{
    PixelOutput OUT;
    float4 color = Diffuse.Sample(Sampler, IN.UV);
    float4 ambient = 0.05 * color;

    // diffuse
    float3 lightDir = normalize(float4(15, 135, -10, 1) - IN.FragPos);
    float3 normal = normalize(IN.Normal);
    float diff = max(dot(lightDir, normal), 0.0);
    float3 diffuse = diff * color;
    // specular
    float3 viewDir = normalize(IN.Position.xyz - IN.FragPos);
    float spec = 0.0;

    float3 reflectDir = reflect(-lightDir, normal);
    spec = pow(max(dot(viewDir, reflectDir), 0.0), 8.0);

    float3 specular = float3(0.3, 0.3, 0.3) * spec; // assuming bright white light color
    OUT.Color = float4(ambient + diffuse + specular, 1.0);
    return OUT;






    //float4 albedo = Diffuse.Sample(Sampler, IN.UV);
    //OUT.Color = albedo;

	//return OUT;

    //LightResult lit = DoLighting( IN.Position.xyz, normalize( IN.Normal ) );

    //float4 diffuse = lit.Diffuse;
    //float4 specular = lit.Specular;
    //float4 texColor = Diffuse.Sample( Sampler, IN.UV );

    //OUT.Color = float4( IN.UV, 0, 0 );
    //OUT.Color = texColor;
    //OUT.Color = ( diffuse + specular ) * texColor;
    //return OUT;

    // Sample the texture for the base color
    float4 baseColor = Diffuse.Sample(Sampler, IN.UV);

    // Normalize normal and view direction
    //float3 normal = normalize(IN.Normal);
    //float3 viewDir = normalize(-IN.Position.xyz); // Assuming camera at origin

    // Accumulate light results
    float4 finalColor = baseColor;
    float3 lighting = float3(0, 0, 0);

    // Process Point Lights
    for (uint i = 0; i < LightPropertiesDelta.NumPointLights; ++i)
    {
        PointLight light = PointLights[i];
        float3 lightDir = normalize(light.PositionWS - IN.FragPos);
        //finalColor += CalculateBlinnPhong(normal, lightDir, viewDir, light.Color.rgb, light.Intensity);
        lighting += CalculateBlinnPhong(normal, lightDir, viewDir, light.Color.rgb, 1).rgb;
    }

    // Process Spot Lights (similar structure to point lights, if applicable)
    for (uint i = 0; i < LightPropertiesDelta.NumSpotLights; ++i)
    {
        SpotLight light = SpotLights[i];
        float3 lightDir = normalize(light.PositionWS - IN.FragPos);

        // Spot angle calculation (if needed)
        float theta = dot(lightDir, normalize(light.DirectionWS));
        if (theta > light.SpotAngle)  // only apply within cone
        {
            finalColor += CalculateBlinnPhong(normal, lightDir, viewDir, light.Color.rgb, 1);
            lighting += CalculateBlinnPhong(normal, lightDir, viewDir, light.Color.rgb, 1).rgb;
        }
    }

    // Apply base color and return
    finalColor *= baseColor;
    finalColor = float4(baseColor.rgb * lighting, 1.0f);
    OUT.Color = finalColor;
    return OUT;
}