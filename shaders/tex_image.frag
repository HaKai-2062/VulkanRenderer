//glsl version 4.5
#version 450

//shader input
layout (location = 0) in vec3 v_Color;
layout (location = 1) in vec2 v_UV;

//output write
layout (location = 0) out vec4 FragColor;

//texture to access
layout(set =0, binding = 0) uniform sampler2D displayTexture;

void main() 
{
	FragColor = texture(displayTexture, v_UV);
}