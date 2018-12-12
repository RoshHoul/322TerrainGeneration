#version 420 core

layout(location=0) in vec4 terrainCoords;
layout(location=1) in vec3 terrainNormals;

uniform mat4 projMat;
uniform mat4 modelViewMat;
uniform mat3 normalMat;
uniform vec4 globAmb;

struct Material
{
	vec4 ambRefl;
	vec4 difRefl;
	vec4 specRefl;
	vec4 emitCols;
	float shininess;
};

struct Light 
{
	vec4 ambCols;
	vec4 difCols;
	vec4 specCols;
	vec4 coords;
};

uniform Light light0;
uniform Material terrainFandB;
vec3 normal;
vec3 lightDirection;
flat out vec4 colorsExport;

void main(void)
{
   gl_Position = projMat * modelViewMat * terrainCoords;
   normal = normalize(normalMat * terrainNormals);
   lightDirection = normalize(vec3(light0.coords));
   colorsExport = max(dot(-normal,lightDirection), 0.0f) * (light0.difCols * terrainFandB.difRefl);
}
