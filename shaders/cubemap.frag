#version 450

layout (binding = 0) uniform samplerCube u_CubeMapSampler;
layout (location = 0) in vec3 v_UVW;
layout (location = 0) out vec4 FragColor;

void main() 
{
	FragColor = texture(u_CubeMapSampler, v_UVW);
}