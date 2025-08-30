#version 450

#extension GL_GOOGLE_include_directive : require
#include "input_structures.glsl"

layout (location = 0) in vec3 v_Normal;
layout (location = 1) in vec3 v_Color;
layout (location = 2) in vec2 v_UV;
layout (location = 3) in vec4 v_WorldPos;
layout (location = 4) in vec4 v_MetalRoughFactor;
layout (location = 5) in vec4 v_DirShadowCoord;
layout (location = 6) in vec4 v_SpotShadowCoord;

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
float distributionGGX(vec3 N, vec3 H, float roughness)
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
float geometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = geometrySchlickGGX(NdotV, roughness);
    float ggx1 = geometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}
// ----------------------------------------------------------------------------
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 calculateLightContribution(vec3 N, vec3 V, vec3 L, vec3 F0, vec3 radiance, vec3 albedo, float roughness, float metallic)
{
    vec3 H = normalize(V + L);
    float NDF = distributionGGX(N, H, roughness);
    // Cook-Torrance BRDF
    float G   = geometrySmith(N, V, L, roughness);
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

float shadowProj(sampler2D shadowMap, vec4 shadowCoord, vec2 off)
{
	float shadow = 1.0;
    if (shadowCoord.x < 0.0 || shadowCoord.x > 1.0 || shadowCoord.y < 0.0 || shadowCoord.y > 1.0 || shadowCoord.z > 1.0)
        return shadow; // Outside shadow map = always lit. Will cause issues if we add multiple shadowmap 
                       // outputs tho later on so maybe fix the clamp to edge thingy later on

	if (shadowCoord.z > -1.0 && shadowCoord.z < 1.0) 
	{
		float dist = texture(shadowMap, shadowCoord.st + off).r;
        dist -= 0.01f;  // Bias

        // Shouldnt the dist < z be the check in reverse z buffer setup?
		if (shadowCoord.w > 0.0 && dist > shadowCoord.z)
		{
			shadow = 0.0f;
		}
	}
	return shadow;
}

float filterPCF(sampler2D shadowMap, vec4 shadowCoord)
{
	ivec2 texDim = textureSize(shadowMap, 0);
	float scale = 1.5;
	float dx = scale * 1.0 / float(texDim.x);
	float dy = scale * 1.0 / float(texDim.y);

	float shadowFactor = 0.0;
	int count = 0;
	int range = 1;
	
	for (int x = -range; x <= range; x++)
	{
		for (int y = -range; y <= range; y++)
		{
			shadowFactor += shadowProj(shadowMap, shadowCoord, vec2(dx*x, dy*y));
			count++;
		}
	
	}
	return shadowFactor / count;
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
    //float directionalShadow = shadowProj(u_DirectionalShadowMap, v_DirShadowCoord / v_DirShadowCoord.w, vec2(0.0f));
    //float spotlightShadow = shadowProj(u_SpotLightShadowMap, v_SpotShadowCoord / v_SpotShadowCoord.w, vec2(0.0f));
    float directionalShadow = filterPCF(u_DirectionalShadowMap, v_DirShadowCoord / v_DirShadowCoord.w);
    float spotlightShadow = filterPCF(u_SpotLightShadowMap, v_SpotShadowCoord / v_SpotShadowCoord.w);

    // Point lights
    for (int i = 0; i < u_Light.TotalPointLights; ++i)
    {
        Light pointLight = u_Light.Lights[i];
        float intensity = u_Light.Lights[i].Intensity;

        if (intensity > 0.01f)
        {
            vec3 fragToLight = pointLight.Position - v_WorldPos.xyz;
            vec3 L = normalize(fragToLight);
            float distance = length(fragToLight);
            float attenuation = 1.0 / (distance * distance);
            vec3 radiance = pointLight.Color.xyz * intensity * attenuation;
    
            Lo += calculateLightContribution(N, V, L, F0, radiance, albedo, roughness, metallic);
        }
    }
    // Directional Light
    if (u_Light.TotalDirectionalLights != 0)
    {
        Light dirLight = u_Light.Lights[u_Light.Count-1];
        float intensity = u_Light.Lights[u_Light.Count-1].Intensity;

        if (intensity > 0.01f)
        {
            vec3 L = -dirLight.Direction; // Normalize it before passing
            vec3 radiance = dirLight.Color.xyz * intensity;
    
            Lo += calculateLightContribution(N, V, L, F0, radiance, albedo, roughness, metallic) * directionalShadow;
        }
    }
    // Spotlight
    for(int i = 0; i < u_Light.TotalSpotLights; ++i)
    {
        Light spotLight = u_Light.Lights[u_Light.TotalPointLights+i];
        //float intensity = u_Light.Lights[u_Light.TotalPointLights+i].Intensity;

        float Constant = 1.0f;
	    float Linear = 0.09f;
	    float Quadratic = 0.032f;
        float Cutoff = cos(radians(25.0f));
        float OuterCutoff = cos(radians(35.0f));

        if (Constant + Linear + Quadratic > 0.01f)
        {
            vec3 fragToLight = spotLight.Position - v_WorldPos.xyz;
            vec3 L = normalize(fragToLight);
            float distance = length(fragToLight);
            float attenuation = 1.0f / (Constant + Linear * distance + 
                                      Quadratic * (distance * distance));
        
            // Spotlight intensity (smoothstep between inner and outer cone)
            float theta = dot(L, -spotLight.Direction); // Normalize spotlight direction before passing
            float epsilon = Cutoff - OuterCutoff;
            float intensity = clamp((theta - OuterCutoff) / epsilon, 0.0, 1.0);
            vec3 radiance = spotLight.Color.xyz * attenuation * intensity;
        
            Lo += calculateLightContribution(N, V, L, F0, radiance, albedo, roughness, metallic) * spotlightShadow;
        }
    }
    
    // ambient lighting (note that the next IBL tutorial will replace 
    // this ambient lighting with environment lighting).
    vec3 ambient = u_SceneData.AmbientColor.rgb * albedo * ao;
    
    vec3 color = ambient + Lo;
    
    // HDR tonemapping
    color = color / (color + vec3(1.0));
    // gamma correct
    color = pow(color, vec3(1.0/2.2));
    
    FragColor = vec4(color, 1.0);
}