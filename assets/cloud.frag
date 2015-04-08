#version 150

uniform samplerCube mTexCube;
uniform vec3 ViewDirection;
uniform vec3 LightPosition;
uniform vec4 CloudColor;
uniform float SpecPow;
uniform float SpecStr;
uniform float AmbStr;

in vec3 ObjectNormal;
in vec4 WorldPosition;

out vec4 oColor;

void main()
{
	vec3 cNormal = normalize(ObjectNormal);
	vec3 cLightDir = normalize(LightPosition-vec3(WorldPosition.xyz));
	vec3 cEyeDir = normalize(ViewDirection);
	vec3 cRefl = reflect(cLightDir, cNormal);
	vec3 cReflView = reflect(cEyeDir, cNormal);

	float cDiffTerm = max(dot(cNormal,cLightDir), 0.0);
	float cSpecTerm = pow( max( dot(cEyeDir, cRefl), 0), SpecPow);

	float cSpecContrib = cSpecTerm*SpecStr;
	vec4 cAmbContrib = CloudColor*AmbStr;
	vec4 cReflContrib = texture(mTexCube, cReflView);

	oColor = cReflContrib*cDiffTerm+cSpecContrib+cAmbContrib;

}