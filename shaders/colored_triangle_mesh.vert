#version 450
#extension GL_EXT_buffer_reference : require

layout (location = 0) out vec3 v_Color;
layout (location = 1) out vec2 v_UV;

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

layout (push_constant) uniform constants
{
	mat4 renderMatrix;
	VertexBuffer vertexBuffer;
} PushConstants;

void main()
{
	Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];
	gl_Position = PushConstants.renderMatrix * vec4(v.position, 1.0f);
	v_Color = v.color.xyz;
	v_UV = vec2(v.uvX, v.uvY);
}