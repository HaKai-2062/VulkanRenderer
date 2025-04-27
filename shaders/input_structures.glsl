#define MAX_POINT_LIGHTS 16

struct PointLight
{
	vec3 Position;
	float Radius;
	vec3 Color;
	float Intensity;
};

layout(set = 0, binding = 0) uniform  SceneData{   
	mat4 View;
	mat4 Proj;
	mat4 ViewProj;
	vec4 AmbientColor;
	vec4 SunlightDirection; //w for sun power
	vec4 SunlightColor;
	vec4 CameraPos;
	float Time;
} u_SceneData;

layout(set = 0, binding = 1) uniform  LightData{   
	PointLight PointLights[MAX_POINT_LIGHTS];
	int TotalPointLights;
} u_Light;

layout(set = 1, binding = 0) uniform GLTFMaterialData{
	vec4 ColorFactors;
	vec4 MetalRoughFactors;
} u_MaterialData;

layout(set = 1, binding = 1) uniform sampler2D u_ColorTex;
layout(set = 1, binding = 2) uniform sampler2D u_MetalRoughTex;
layout(set = 1, binding = 3) uniform sampler2D u_AOTex;
layout(set = 1, binding = 4) uniform sampler2D u_NormalTex;