#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "input_structures.glsl"

layout (location = 0) out vec3 v_Normal;
layout (location = 1) out vec3 v_Color;
layout (location = 2) out vec2 v_UV;

struct Vertex
{
	vec3 position;
	float uvX;
	vec3 normal;
	float uvY;
	vec4 color;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer
{
	Vertex vertices[];
};

layout(push_constant) uniform constants
{
	mat4 renderMatrix;
	VertexBuffer vertexBuffer;
} PushConstants;

void main() 
{
	Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];
	
	vec4 position = vec4(v.position, 1.0f);

	gl_Position =  a_SceneData.viewProj * PushConstants.renderMatrix * position;

	v_Normal = (PushConstants.renderMatrix * vec4(v.normal, 0.f)).xyz;
	v_Color = v.color.xyz * a_MaterialData.colorFactors.xyz;	
	v_UV = vec2(v.uvX, v.uvY);
}