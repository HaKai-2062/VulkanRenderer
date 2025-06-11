#version 450

#extension GL_EXT_buffer_reference : require

layout (location = 0) out vec3 v_SampleDir;

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

void main() 
{
	Vertex v = PushConstants.VertexBuffer.Vertices[gl_VertexIndex];
	gl_Position = vec4(v.Position, 1.0f);
	v_SampleDir = vec3(PushConstants.RenderMatrix * vec4(v.Position, 1.0f));
	v_SampleDir *= -1.0f;
}