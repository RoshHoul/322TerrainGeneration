#include <iostream>
#include <fstream>
#include <time.h>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <Windows.h>

#include <GL/glew.h>
#include <GL/freeglut.h>
//#include <GL/glext.h>
#pragma comment(lib, "glew32.lib") 

using namespace std;
using namespace glm;

// Size of the terrain
const int MAP_SIZE = 33;

const int SCREEN_WIDTH = 500;
const int SCREEN_HEIGHT = 500;


struct Vertex
{
	vec4 coords;
	vec3 normals;
};

struct Matrix4x4
{
	float entries[16];
};


static mat4 projMat = mat4(1.0);

static const Matrix4x4 IDENTITY_MATRIX4x4 =
{
	{
		1.0, 0.0, 0.0, 0.0,
		0.0, 1.0, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		0.0, 0.0, 0.0, 1.0
	}
};

struct Material {
	vec4 ambRefl;
	vec4 difRefl;
	vec4 specRefl;
	vec4 emitCols;
	float shininess;
};

struct Light {
	vec4 ambCols;
	vec4 difCols;
	vec4 specCols;
	vec4 coords;
};

static enum buffer { TERRAIN_VERTICES };
static enum object { TERRAIN };

// Globals
static Vertex terrainVertices[MAP_SIZE*MAP_SIZE] = {};

const int numStripsRequired = MAP_SIZE - 1;
const int verticesPerStrip = 2 * MAP_SIZE;

unsigned int terrainIndexData[numStripsRequired][verticesPerStrip];

static mat3 normalMat = mat3(1.0);

static unsigned int
programId,
vertexShaderId,
fragmentShaderId,
modelViewMatLoc,
projMatLoc,
normalMatLoc,
buffer[1],
vao[1];

// Function to read text file, used to read shader files
char* readTextFile(char* aTextFile)
{
	FILE* filePointer = fopen(aTextFile, "rb");
	char* content = NULL;
	long numVal = 0;

	fseek(filePointer, 0L, SEEK_END);
	numVal = ftell(filePointer);
	fseek(filePointer, 0L, SEEK_SET);
	content = (char*)malloc((numVal + 1) * sizeof(char));
	fread(content, 1, numVal, filePointer);
	content[numVal] = '\0';
	fclose(filePointer);
	return content;
}

void shaderCompileTest(GLuint shader) {
	GLint result = GL_FALSE;
	int logLength;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
	std::vector<GLchar> vertShaderError((logLength > 1) ? logLength : 1);
	glGetShaderInfoLog(shader, logLength, NULL, &vertShaderError[0]);
	std::cout << &vertShaderError[0] << std::endl;
}

float randomFloat(float min, float max)
{
	float random = ((float)rand()) / (float)RAND_MAX;
	float diff = max - min;
	float r = random * diff;
	return min + r;
}

inline static float random(int range) {
	return (rand() % (range * 2)) - range;
}

bool inDimensions(int x, int z) {
	return x >= 0 && x < MAP_SIZE && z >= 0 && z < MAP_SIZE;
}

float getAverage(float map[MAP_SIZE][MAP_SIZE], int x, int z, int step) {

	int halfStep = step  / 2;
	int xValue, zValue;
	int avg = 0;
	int count = 0;

	xValue = x + halfStep;
	zValue = z;
	cout << xValue << " " << zValue;
	if (inDimensions(xValue, zValue)) {
		avg += map[xValue][zValue];
		count++;
	}
	xValue = x - halfStep;
	zValue = z;

	if (inDimensions(xValue, zValue)) {
		avg += map[xValue][zValue];
		count++;
	}
	xValue = x;
	zValue = z - halfStep;

	if (inDimensions(xValue, zValue)) {
		avg += map[xValue][zValue];
		count++;
	}
	xValue = x;
	zValue = z + halfStep;

	if (inDimensions(xValue, zValue)) {
		avg += map[xValue][zValue];
		count++;
	}

	return avg / count;
}

void diamondStep(float map[MAP_SIZE][MAP_SIZE], int x, int z, int fullStep, float roughness) {
	
	
	int reach = fullStep/2;
	float avg = map[x][z] + map[x + fullStep][z] + map[x + fullStep][z + fullStep] + map[x][z + fullStep];

	avg /= 2.0f;
	map[x + reach][z + reach] = avg + random(1) * roughness;

}

void squareStep(float map[MAP_SIZE][MAP_SIZE], int x, int z, int fullStep, float roughness) {
	int reach = fullStep/ 2;
	float rAvg = getAverage(map,x + fullStep, z + reach, fullStep);
	float lAvg = getAverage(map, x, z + reach, fullStep);
	float upAvg = getAverage(map, x + reach, z, fullStep);
	float downAvg = getAverage(map, x + reach, z + fullStep, fullStep);

	map[x + fullStep][z + reach] = rAvg + random(1);
	map[x][z + reach] = lAvg + random(1) ;
	map[x + reach][z] = upAvg + random(1);
	map[x + reach][z + fullStep] = downAvg + random(1);

}

void diamondSquare(float map[MAP_SIZE][MAP_SIZE], int size, float startSmoothness, float roughness) {

	float smoothness = startSmoothness;
	map[0][0] = random(1);
	map[0][MAP_SIZE - 1] = random(1);
	map[MAP_SIZE - 1][0] = random(1);
	map[MAP_SIZE - 1][MAP_SIZE - 1] = random(1);

	for (int step = MAP_SIZE - 1; step > 1; step /= 2) {
		for (int x = 0; x < MAP_SIZE - 2; x += step) {
			for (int z = 0; z < MAP_SIZE - 2; z += step) {
				diamondStep(map, x, z, step, smoothness);
				squareStep(map, x, z, step, smoothness);
			}
			smoothness *= roughness;
			size /= 2;
		}
	}
}
	//int half = size / 2;
	//smoothness *= roughness;
	//if (half < 1)
	//	return;

	//int col = 0;
	//for (int x = 0; x < MAP_SIZE; x += half) {
	//	col++;
	//	if (col % 2 == 1) {
	//		for (int z = half; z < MAP_SIZE-2; z += size) {
	//			diamondStep(map, x % MAP_SIZE, z % MAP_SIZE, half, roughness);
	//		} 
	//	}
	//	else {
	//		for (int z = 0; z < MAP_SIZE-2; z += size) {
	//			diamondStep(map, x % MAP_SIZE, z % MAP_SIZE, half, roughness); 
	//		}
	//	}

	//	for (int z = half; z < MAP_SIZE-2; z += size) {
	//		for (int x = half; x < MAP_SIZE; x += size) {
	//			squareStep(map, x % MAP_SIZE, z % MAP_SIZE, half, roughness);
	//		}
	//	}

	//	diamondSquare(map, size/2, smoothness, roughness);
	//}


float height(vec3 point) {
	return point.y;
}

void normals(Vertex point) {
	//for (int i = 0; i < point.coords)
}

vector<vec3> ListOfNormals;
vector<vec3> CalculateNormals()
{
	vec3 v0, v1, v2, normal;

	for (int i = 0; i < MAP_SIZE; i += 3)
	{
		v0 = vec3(terrainVertices[i + 0].coords);
		v1 = vec3(terrainVertices[i + 1].coords);
		v2 = vec3(terrainVertices[i + 2].coords);

		vec3 e1 = v1 - v0;
		vec3 e2 = v2 - v0;
		vec3 e3 = v2 - v1;

		normal = cross(e1, e2);
		if (normal.z == 0) {
			cout << "e1: " << e1.x << " " << e1.y << " " << e1.z << " e2:" << e2.x << " " << e2.y << " " << e2.z << endl;
			terrainVertices[i + 0].normals = vec3(0,0,1);
		}
		cout << "preNorm: " << normal.z << endl;
		normal = normalize(normal);

		terrainVertices[i + 0].normals = normal;
	//	ListOfNormals.push_back(normalize(normal));
		
		cout << normal.z << endl;
	}
	return ListOfNormals;
}

// Initialization routine.
void setup(void)
{
	// Initialise terrain - set values in the height map to 0
	float terrain[MAP_SIZE][MAP_SIZE] = {};
	for (int x = 0; x < MAP_SIZE; x++)
	{
		for (int z = 0; z < MAP_SIZE; z++)
		{
			terrain[x][z] = 0;
		}
	}



	// TODO: Add your code here to calculate the height values of the terrain using the Diamond square algorithm
	float step_size = MAP_SIZE - 1;
	float rand_max = 1.0f;
	float H = 2.0f;

	diamondSquare(terrain, MAP_SIZE, 10, 0.3);

	// Intialise vertex array
	int i = 0;

	for (int z = 0; z < MAP_SIZE; z++)
	{
		for (int x = 0; x < MAP_SIZE; x++)
		{
			// Set the coords (1st 4 elements) and a default colour of black (2nd 4 elements) 
			terrainVertices[i] = { { vec4((float)x, terrain[x][z], (float)z, 1.0) }	};

			i++;
		}
	}

	for (int i = 0; i < MAP_SIZE; i++) {
	}

	// Now build the index data 
	i = 0;
	for (int z = 0; z < MAP_SIZE - 1; z++)
	{
		i = z * MAP_SIZE;
		for (int x = 0; x < MAP_SIZE * 2; x += 2)
		{
			terrainIndexData[z][x] = i;
			i++;
		}
		for (int x = 1; x < MAP_SIZE * 2 + 1; x += 2)
		{
			terrainIndexData[z][x] = i;
			i++;
		}
	}

	CalculateNormals();
	for (int k = 0; k < MAP_SIZE - 1; k++)
	{
	//	terrainVertices[k].normals = normalize(ListOfNormals[k]);

	}

	static const Material terrainFandB = {
		vec4(1.0, 1.0, 1.0, 1.0),
		vec4(1.0, 1.0, 1.0, 1.0),
		vec4(1.0, 1.0, 1.0, 1.0),
		vec4(1.0, 1.0, 1.0, 1.0),
		50.0f
	};

	static const Light light0 = {
		vec4(0.0f, 0.0f, 0.0f, 1.0f),
		vec4(1.0f, 1.0f, 1.0f, 1.0f),
		vec4(1.0f, 1.0f, 1.0f, 1.0f),
		vec4(1.0f, 1.0f, 0.0f, 0.0f),
	};

	static const vec4 globAmb = vec4(0.2f, 0.2f, 0.2f, 1.0f);

	glClearColor(1.0, 1.0, 1.0, 0.0);

	// Create shader program executable - read, compile and link shaders
	char* vertexShader = readTextFile("vertexShader.glsl");
	vertexShaderId = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShaderId, 1, (const char**)&vertexShader, NULL);
	glCompileShader(vertexShaderId);
	std::cout << "VERTEX: " << std::endl;
	shaderCompileTest(vertexShaderId);

	GLenum err;
	char* fragmentShader = readTextFile("fragmentShader.glsl");
	fragmentShaderId = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShaderId, 1, (const char**)&fragmentShader, NULL);
	glCompileShader(fragmentShaderId);
	std::cout << "FRAGMENT: " << std::endl;
	shaderCompileTest(fragmentShaderId);

	programId = glCreateProgram();
	glAttachShader(programId, vertexShaderId);
	glAttachShader(programId, fragmentShaderId);
	glLinkProgram(programId);
	glUseProgram(programId);
	///////////////////////////////////////

	// Create vertex array object (VAO) and vertex buffer object (VBO) and associate data with vertex shader.
	glGenVertexArrays(1, vao);
	glGenBuffers(1, buffer);
	glBindVertexArray(vao[TERRAIN]);
	glBindBuffer(GL_ARRAY_BUFFER, buffer[TERRAIN_VERTICES]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(terrainVertices), terrainVertices, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(terrainVertices[0]), 0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(terrainVertices[1]), (GLvoid*)sizeof(terrainVertices[1].coords));
	glEnableVertexAttribArray(1);
	///////////////////////////////////////

	// Obtain projection matrix uniform location and set value.
	projMatLoc = glGetUniformLocation(programId, "projMat");
	projMat = perspective(radians(60.0), (double)SCREEN_WIDTH / (double)SCREEN_HEIGHT, 0.1, 100.0);
	glUniformMatrix4fv(projMatLoc, 1, GL_FALSE, value_ptr(projMat));
	

	glUniform4fv(glGetUniformLocation(programId, "terrainFandB.ambRefl"), 1, &terrainFandB.ambRefl[0]);
	glUniform4fv(glGetUniformLocation(programId, "terrainFandB.difRefl"), 1, &terrainFandB.difRefl[0]);
	glUniform4fv(glGetUniformLocation(programId, "terrainFandB.specRefl"), 1, &terrainFandB.specRefl[0]);
	glUniform4fv(glGetUniformLocation(programId, "terrainFandB.emitCols"), 1, &terrainFandB.emitCols[0]);
	glUniform4fv(glGetUniformLocation(programId, "terrainFandB.emitCols"), 1, &terrainFandB.emitCols[0]);

	glUniform4fv(glGetUniformLocation(programId, "globAmb"), 1, &globAmb[0]);

	glUniform4fv(glGetUniformLocation(programId, "light0.ambCols"), 1, &light0.ambCols[0]);
	glUniform4fv(glGetUniformLocation(programId, "light0.difCols"), 1, &light0.difCols[0]);
	glUniform4fv(glGetUniformLocation(programId, "light0.specCols"), 1, &light0.specCols[0]);
	glUniform4fv(glGetUniformLocation(programId, "light0.coords"), 1, &light0.coords[0]);
	

	///////////////////////////////////////

	// Obtain modelview matrix uniform location and set value.
	mat4 modelViewMat = mat4(1.0);

	// Move terrain into view - glm::translate replaces glTranslatef
	modelViewMat = translate(modelViewMat, vec3(-16.0f, 0.0f, -30.0f)); // 5x5 grid
	modelViewMatLoc = glGetUniformLocation(programId, "modelViewMat");
	glUniformMatrix4fv(modelViewMatLoc, 1, GL_FALSE, value_ptr(modelViewMat));

	normalMatLoc = glGetUniformLocation(programId, "normalMat");

	normalMat = transpose(inverse(mat3(modelViewMat)));
	glUniformMatrix3fv(normalMatLoc, 1, GL_FALSE, value_ptr(normalMat));

	///////////////////////////////////////
}

// Drawing routine.
void drawScene(void)
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);



	// For each row - draw the triangle strip
	for (int i = 0; i < MAP_SIZE - 1; i++)
	{
		glDrawElements(GL_TRIANGLE_STRIP, verticesPerStrip, GL_UNSIGNED_INT, terrainIndexData[i]);
	}

	glFlush();
}

// OpenGL window reshape routine.
void resize(int w, int h)
{
	glViewport(0, 0, w, h);
}

// Keyboard input processing routine.
void keyInput(unsigned char key, int x, int y)
{
	switch (key)
	{
	case 27:
		exit(0);
		break;
	default:
		break;
	}
}

// Main routine.
int main(int argc, char* argv[])
{


	static const Material terrainFandB = {
		vec4(1.0, 1.0, 1.0, 1.0),
		vec4(1.0, 1.0, 1.0, 1.0),
		vec4(1.0, 1.0, 1.0, 1.0),
		vec4(1.0, 1.0, 1.0, 1.0),
		50.0f
	};

	glutInit(&argc, argv);

	// Set the version of OpenGL (4.2)
	glutInitContextVersion(4, 2);
	// The core profile excludes all discarded features
	glutInitContextProfile(GLUT_CORE_PROFILE);
	// Forward compatibility excludes features marked for deprecation ensuring compatability with future versions
	glutInitContextFlags(GLUT_FORWARD_COMPATIBLE);

	glutInitDisplayMode(GLUT_SINGLE | GLUT_RGBA);
	glutInitWindowSize(SCREEN_WIDTH, SCREEN_HEIGHT);
	glutInitWindowPosition(100, 100);
	glutCreateWindow("TerrainGeneration");

	// Set OpenGL to render in wireframe mode
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glEnable(GL_DEPTH_TEST);

	glutDisplayFunc(drawScene);
	glutReshapeFunc(resize);
	glutKeyboardFunc(keyInput);

	glewExperimental = GL_TRUE;
	glewInit();
	srand(time(NULL));
	setup();


	glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);

	glm::vec3 cameraTarget = glm::vec3(-2.5f, -2.5f, -10.0f); // 5x5 gri
	glm::vec3 cameraDirection = glm::normalize(cameraPos - cameraTarget);
	glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
	glm::vec3 cameraRight = glm::normalize(glm::cross(up, cameraDirection));
	glm::vec3 cameraUp = glm::cross(cameraDirection, cameraRight);
	


	glm::mat4 view;
	//?view = glm::lookAt(cameraPos, cameraTarget, glm::vec3(0.0f, 1.0f, 0.0f));
	


	//float rFloat = randomFloat(-1.0f, 1.0f);
	//cout << "Random number generated: " << rFloat << endl;

	glutMainLoop();
}