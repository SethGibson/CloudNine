#version 150

uniform mat4 ciModelViewProjection;
uniform mat4 ciModelMatrix;

in vec3 ciNormal;
in vec4 ciPosition;
in vec3 iPosition;
in float iSize;
in vec4 iColor;

out vec4 Color;
out vec3 ObjectNormal;
out vec4 WorldPosition;

void main()
{
	vec4 mPosition = ciPosition;
	mPosition.x *= iSize;
	mPosition.y *= iSize;
	mPosition.z *= iSize;

	Color = iColor;
	ObjectNormal = vec3((vec4(ciNormal,0)*ciModelMatrix).xyz);
	//ObjectNormal = ciNormal;
	WorldPosition = ciModelMatrix*(4.0*mPosition + vec4(iPosition,0));

	gl_Position = ciModelViewProjection * (4.0*mPosition + vec4(iPosition,0));
}