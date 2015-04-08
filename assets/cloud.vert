#version 150

uniform mat4 ciModelViewProjection;
uniform mat4 ciModelMatrix;
uniform mat4 ciViewMatrix;
uniform mat3 ciNormalMatrix;

in vec4 ciPosition;
in vec3 ciNormal;
in vec3 iPosition;
in vec2 iUV;
in float iSize;

out vec2 UV;
out vec3 ObjectNormal;
out vec4 WorldPosition;

void main()
{
	UV = iUV;
	vec4 mPosition = ciPosition;
	mPosition.x *= iSize;
	mPosition.y *= iSize;
	mPosition.z *= iSize;

	ObjectNormal = vec3((vec4(ciNormal,0)*ciModelMatrix).xyz);
	WorldPosition = ciModelMatrix*(4.0*mPosition + vec4(iPosition,0));
	gl_Position = ciModelViewProjection * (4.0*mPosition + vec4(iPosition,0));
}