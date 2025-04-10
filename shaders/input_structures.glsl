layout(set = 0, binding = 0) uniform  SceneData{   
	mat4 view;
	mat4 proj;
	mat4 viewProj;
	vec4 ambientColor;
	vec4 sunlightDirection; //w for sun power
	vec4 sunlightColor;
} a_SceneData;

layout(set = 1, binding = 0) uniform GLTFMaterialData{   
	vec4 colorFactors;
	vec4 metalRoughFactors;
} a_MaterialData;

layout(set = 1, binding = 1) uniform sampler2D a_colorTex;
layout(set = 1, binding = 2) uniform sampler2D a_metalRoughTex;