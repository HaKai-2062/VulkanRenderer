#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "input_structures.glsl"

layout (location = 0) out vec3 v_Normal;
layout (location = 1) out vec3 v_Color;
layout (location = 2) out vec2 v_UV;
layout (location = 3) out vec4 v_WorldPos;
layout (location = 4) out vec4 v_MetalRoughFactor;
layout (location = 5) out vec4 v_DirShadowCoord;
layout (location = 6) out vec4 v_SpotShadowCoord;

struct Vertex
{
	vec3 Position;
	float UVX;
	vec3 Normal;
	float UVY;
	vec4 Color;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer
{
	Vertex Vertices[];
};

layout(push_constant) uniform constants
{
	mat4 RenderMatrix;
	VertexBuffer VertexBuffer;
	vec2 Padding;
	vec4 OverrideColor;
} PushConstants;

// Scale by 0.5 and translate 0.5 to trasform -1:1 to 0:1
const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 );

void main()
{
	Vertex v = PushConstants.VertexBuffer.Vertices[gl_VertexIndex];
	v_WorldPos = PushConstants.RenderMatrix * vec4(v.Position, 1.0f);

	// This is proj * view * model * pos
	gl_Position =  u_SceneData.ViewProj * v_WorldPos;

	v_UV = vec2(v.UVX, v.UVY);
	v_Normal = transpose(inverse(mat3(PushConstants.RenderMatrix))) * v.Normal;
	if (PushConstants.OverrideColor.w > 0.5f)
	{
		v_Color = PushConstants.OverrideColor.rgb;
	}
	else
	{
		v_Color = v.Color.rgb;
	}
	v_Color *= u_MaterialData.ColorFactors.rgb;
	v_MetalRoughFactor = u_MaterialData.MetalRoughFactors;
	v_DirShadowCoord = biasMat * u_Light.Lights[u_Light.Count-1].LightProj * v_WorldPos;
	v_SpotShadowCoord = biasMat * u_Light.Lights[u_Light.TotalPointLights].LightProj * v_WorldPos;
}