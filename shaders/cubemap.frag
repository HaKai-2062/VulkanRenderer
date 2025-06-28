#version 450

layout (set = 0, binding = 0) uniform samplerCube u_CubeMapSampler;
layout (location = 0) in vec3 v_SampleDir;
layout (location = 0) out vec4 FragColor;

void main()
{
	FragColor = vec4(texture(u_CubeMapSampler, v_SampleDir).rgb, 1.0f);
}