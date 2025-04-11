#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "input_structures.glsl"

layout (location = 0) out vec3 v_Normal;
layout (location = 1) out vec3 v_Color;
layout (location = 2) out vec2 v_UV;

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
} PushConstants;

void main()
{
	Vertex v = PushConstants.VertexBuffer.Vertices[gl_VertexIndex];
	
	vec4 position = vec4(v.Position, 1.0f);

	gl_Position =  a_SceneData.ViewProj * PushConstants.RenderMatrix * position;

	v_Normal = (PushConstants.RenderMatrix * vec4(v.Normal, 0.f)).xyz;
	v_Color = v.Color.xyz * a_MaterialData.ColorFactors.xyz;	
	v_UV = vec2(v.UVX, v.UVY);
}