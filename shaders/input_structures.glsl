#define MAX_POINT_LIGHTS 8
#define MAX_SPOT_LIGHTS 8

struct PointLight
{
	vec3 Position;
	float Intensity;
	vec3 Color;
	int Padding;
	mat4 LightProj;
};

struct Directional
{
	vec3 Direction;
	float Intensity;
	vec3 Color;
	int Padding;
	mat4 LightProj;
};

struct SpotLight
{
	vec3 Position;
	float Cutoff;
	vec3 Direction;
	float OuterCutoff;
	vec3 Color;
	float Constant;
	float Linear;
	float Quadratic;
	vec2 Padding;
	mat4 LightProj;
};

layout(set = 0, binding = 0) uniform  SceneData{   
	mat4 View;
	mat4 Proj;
	mat4 ViewProj;
	vec4 AmbientColor;
	vec4 CameraPos;
	float Time;
	vec3 Padding;
} u_SceneData;

layout(set = 0, binding = 1) uniform LightData{   
	Directional DirectionalLight;
	PointLight PointLights[MAX_POINT_LIGHTS];
	SpotLight SpotLights[MAX_SPOT_LIGHTS];

	int TotalPointLights;
	int TotalSpotLights;
	vec2 Padding;
} u_Light;

layout(set = 0, binding = 2) uniform sampler2D u_CubeMap;
layout(set = 0, binding = 3) uniform sampler2D u_SpotLightShadowMap;
layout(set = 0, binding = 4) uniform sampler2D u_DirectionalShadowMap;

layout(set = 1, binding = 0) uniform GLTFMaterialData{
	vec4 ColorFactors;
	vec4 MetalRoughFactors;
} u_MaterialData;

layout(set = 1, binding = 1) uniform sampler2D u_ColorTex;
layout(set = 1, binding = 2) uniform sampler2D u_MetalRoughTex;
layout(set = 1, binding = 3) uniform sampler2D u_AOTex;
layout(set = 1, binding = 4) uniform sampler2D u_NormalTex;