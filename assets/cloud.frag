#version 150

uniform sampler2D mTexRgb;
uniform samplerCube mTexCube;
uniform vec3 ViewDirection;
uniform vec3 LightPosition;
uniform float SpecPow;
in vec2 UV;
in vec3 ObjectNormal;
in vec4 WorldPosition;

out vec4 oColor;

void main()
{
	vec3 cNormal = normalize(ObjectNormal);
	vec3 cLightDir = normalize(LightPosition-vec3(WorldPosition.xyz));
	vec3 cEyeDir = normalize(ViewDirection);
	vec3 cRefl = reflect(cLightDir, cNormal);
	
	float cDiffTerm = max(dot(cNormal,cLightDir), 0.0);
	float cSpecTerm = pow( max( dot(cEyeDir, cRefl), 0), SpecPow);

	vec4 cTexContrib = texture2D(mTexRgb, vec2(UV.x,1.0-UV.y));
	vec4 cSpecContrib = cTexContrib*cSpecTerm;
	oColor = cTexContrib*cDiffTerm+cSpecContrib;
}