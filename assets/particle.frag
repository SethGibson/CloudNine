#version 150

uniform samplerCube mTexCube;
uniform vec3 ViewDirection;
uniform vec3 ViewPosition;
uniform vec3 LightPosition;
uniform float SpecPow;
uniform float SpecStr;
uniform float ReflStr;

in vec3 ObjectNormal;
in vec4 WorldPosition;
in vec4 Color;

out vec4 oColor;

void main()
{
	vec3 cNormal = normalize(ObjectNormal);
	vec3 cLightDir = normalize(LightPosition-vec3(WorldPosition.xyz));
	vec3 cEyeDir = normalize(ViewDirection);
	vec3 cHalfV = normalize(cLightDir+cEyeDir);

	vec3 cRefl = reflect(cLightDir, cNormal);
	vec3 cReflView = reflect(cEyeDir, cNormal);

	float cSpecTerm = pow( max( dot(cEyeDir, cRefl), 0), SpecPow);
	float cViewTerm = 1.0 - max(dot(-cEyeDir, cNormal), 0.0);
	//cViewTerm = pow(cViewTerm, 16.0);
	vec4 cReflContrib = texture(mTexCube, cReflView);

	oColor = Color+(cReflContrib*cViewTerm*ReflStr)+cSpecTerm;
	//oColor = vec4(vec3(cViewTerm),1.0);
}