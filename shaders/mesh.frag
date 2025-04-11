#version 450

#extension GL_GOOGLE_include_directive : require
#include "input_structures.glsl"

layout (location = 0) in vec3 v_Normal;
layout (location = 1) in vec3 v_Color;
layout (location = 2) in vec2 v_UV;

layout (location = 0) out vec4 FragColor;

void main()
{
	float lightValue = max(dot(v_Normal, a_SceneData.SunlightDirection.xyz), 0.1f);

	vec3 color = v_Color * texture(a_ColorTex,v_UV).xyz;
	vec3 ambient = color *  a_SceneData.AmbientColor.xyz;

	FragColor = vec4(color * lightValue *  a_SceneData.SunlightColor.w + ambient, 1.0f);
}