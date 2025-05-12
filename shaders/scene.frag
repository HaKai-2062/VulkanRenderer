#version 450

#extension GL_GOOGLE_include_directive : require
#include "input_structures.glsl"

layout (location = 0) in vec3 v_Normal;
layout (location = 1) in vec3 v_Color;
layout (location = 2) in vec2 v_UV;
layout (location = 3) in vec4 v_WorldPos;
layout (location = 4) in vec4 v_MetalRoughFactor;

layout (location = 0) out vec4 FragColor;

const float PI = 3.14159265359;
// ----------------------------------------------------------------------------
// Easy trick to get tangent-normals to world-space to keep PBR code simplified.
// Don't worry if you don't get what's going on; you generally want to do normal 
// mapping the usual way for performance anyways; I do plan make a note of this 
// technique somewhere later in the normal mapping tutorial.
vec3 getNormalFromMap()
{
    vec3 tangentNormal = texture(u_NormalTex, v_UV).xyz * 2.0 - 1.0;

    vec3 Q1  = dFdx(v_WorldPos.xyz);
    vec3 Q2  = dFdy(v_WorldPos.xyz);
    vec2 st1 = dFdx(v_UV);
    vec2 st2 = dFdy(v_UV);

    vec3 N   = normalize(v_Normal);
    vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
    vec3 B  = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}
// ----------------------------------------------------------------------------
float DistributionGGX(vec3 N, vec3 H, float roughness)
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
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}
// ----------------------------------------------------------------------------
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 calculateLightContribution(vec3 N, vec3 H, vec3 V, vec3 L, vec3 F0, vec3 radiance, vec3 albedo, float roughness, float metallic)
{
    float NDF = DistributionGGX(N, H, roughness);
    // Cook-Torrance BRDF
    float G   = GeometrySmith(N, V, L, roughness);
    vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);
       
    vec3 numerator    = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; // + 0.0001 to prevent divide by zero
    vec3 specular = numerator / denominator;
    
    // kS is equal to Fresnel
    vec3 kS = F;
    // for energy conservation, the diffuse and specular light can't
    // be above 1.0 (unless the surface emits light); to preserve this
    // relationship the diffuse component (kD) should equal 1.0 - kS.
    vec3 kD = vec3(1.0) - kS;
    // multiply kD by the inverse metalness such that only non-metals 
    // have diffuse lighting, or a linear blend if partly metal (pure metals
    // have no diffuse light).
    kD *= 1.0 - metallic;

    // scale light by NdotL
    float NdotL = max(dot(N, L), 0.0);

    // add to outgoing radiance Lo
    return (kD * albedo / PI + specular) * radiance * NdotL;  // note that we already multiplied the BRDF by the Fresnel (kS) so we won't multiply by kS again
}


// ----------------------------------------------------------------------------
void main()
{
    vec3 albedo     = pow(v_Color * texture(u_ColorTex, v_UV).rgb, vec3(2.2));
    float metallic  = texture(u_MetalRoughTex, v_UV).r * v_MetalRoughFactor.r;
    float roughness = texture(u_MetalRoughTex, v_UV).g * v_MetalRoughFactor.g;
    float ao        = texture(u_AOTex, v_UV).r;

    vec3 N = getNormalFromMap();
    vec3 V = normalize(u_SceneData.CameraPos.xyz - v_WorldPos.xyz);

    // calculate reflectance at normal incidence; if dia-electric (like plastic) use F0 
    // of 0.04 and if it's a metal, use the albedo color as F0 (metallic workflow)    
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);

    // reflectance equation
    vec3 Lo = vec3(0.0);

    // Point lights
    for(int i = 0; i < u_Light.TotalPointLights; ++i)
    {
        PointLight light = u_Light.PointLights[i];

        vec3 fragToLight = light.Position - v_WorldPos.xyz;
        vec3 L = normalize(fragToLight);
        vec3 H = normalize(V + L);
        float distance = length(fragToLight);
        float attenuation = 1.0 / (distance * distance);
        vec3 radiance = light.Color * light.Intensity * attenuation;

        Lo += calculateLightContribution(N, H, V, L, F0, radiance, albedo, roughness, metallic);
    }
    // Directional Light
    {
        vec3 L = normalize(-vec3(0.0f, -1.0f, 0.2f));   // Light dir
        vec3 H = normalize(V + L);
        vec3 radiance = vec3(1.0f); // Light color
    
        Lo += calculateLightContribution(N, H, V, L, F0, radiance, albedo, roughness, metallic);
    }
    // Spotlight
    {
        vec3 spotLightPos = u_SceneData.CameraPos.xyz;
        vec3 spotLightDir = normalize(-vec3(u_SceneData.View[0][2], u_SceneData.View[1][2], u_SceneData.View[2][2]));
        vec3 spotLightColor = vec3(1.0f);
        float spotInnerCutoff = 0.86;   // (30 deg)
        float spotOuterCutoff = 0.5;    // (60 deg)
        float range = 10.0f;

        vec3 fragToLight = spotLightPos - v_WorldPos.xyz;
        vec3 L = normalize(fragToLight);
        vec3 H = normalize(V + L);
        float distance = length(fragToLight);
        float attenuation = range / (distance * distance);

        // Spotlight intensity (smoothstep between inner and outer cone)
        float theta = dot(L, normalize(-spotLightDir));
        float epsilon = spotInnerCutoff - spotOuterCutoff;
        float intensity = clamp((theta - spotOuterCutoff) / epsilon, 0.0, 1.0);
        vec3 radiance = spotLightColor * attenuation * intensity;

        Lo += calculateLightContribution(N, H, V, L, F0, radiance, albedo, roughness, metallic);
    }

    // ambient lighting (note that the next IBL tutorial will replace 
    // this ambient lighting with environment lighting).
    vec3 ambient = vec3(0.03) * albedo * ao;
    
    vec3 color = ambient + Lo;

    // HDR tonemapping
    color = color / (color + vec3(1.0));
    // gamma correct
    color = pow(color, vec3(1.0/2.2)); 

    FragColor = vec4(color, 1.0);
}