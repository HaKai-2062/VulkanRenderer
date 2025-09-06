#define MAX_POINT_LIGHTS 8
#define MAX_SPOT_LIGHTS 8

layout(std140, set = 0, binding = 0) uniform SceneData{   
	mat4 View;
	mat4 Proj;
	mat4 ViewProj;
	vec4 AmbientColor;
	vec4 CameraPos;
	float Time;
	vec3 Padding;
} u_SceneData;

struct Light
{
	vec3 Position;
	// 0 is PointLight, 1 is SpotLight, 2 is DirectionalLight
	int Type;
	vec3 Direction;
	float Intensity;
	vec4 Color;
	mat4 LightProj;
};

layout(std430, set = 0, binding = 1) readonly restrict buffer LightData{   
	int Count;
	int TotalPointLights;
	int TotalSpotLights;
	int TotalDirectionalLights;
	Light Lights[];
} u_Light;

layout(set = 0, binding = 2) uniform sampler2D u_CubeMap;
layout(set = 0, binding = 3) uniform sampler2D u_SpotLightShadowMap;
layout(set = 0, binding = 4) uniform sampler2D u_DirectionalShadowMap;

layout(std140, set = 1, binding = 0) uniform GLTFMaterialData{
	vec4 ColorFactors;
	vec4 MetalRoughFactors;
} u_MaterialData;

layout(set = 1, binding = 1) uniform sampler2D u_ColorTex;
layout(set = 1, binding = 2) uniform sampler2D u_MetalRoughTex;
layout(set = 1, binding = 3) uniform sampler2D u_AOTex;
layout(set = 1, binding = 4) uniform sampler2D u_NormalTex;