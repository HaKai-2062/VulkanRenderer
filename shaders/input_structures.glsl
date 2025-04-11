layout(set = 0, binding = 0) uniform  SceneData{   
	mat4 View;
	mat4 Proj;
	mat4 ViewProj;
	vec4 AmbientColor;
	vec4 SunlightDirection; //w for sun power
	vec4 SunlightColor;
} a_SceneData;

layout(set = 1, binding = 0) uniform GLTFMaterialData{   
	vec4 ColorFactors;
	vec4 MetalRoughFactors;
} a_MaterialData;

layout(set = 1, binding = 1) uniform sampler2D a_ColorTex;
layout(set = 1, binding = 2) uniform sampler2D a_MetalRoughTex;