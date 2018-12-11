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
	float coords[4];
	float colors[4];
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

static enum buffer { TERRAIN_VERTICES };
static enum object { TERRAIN };

// Globals
static Vertex terrainVertices[MAP_SIZE*MAP_SIZE] = {};

const int numStripsRequired = MAP_SIZE - 1;
const int verticesPerStrip = 2 * MAP_SIZE;

unsigned int terrainIndexData[numStripsRequired][verticesPerStrip];

static unsigned int
programId,
vertexShaderId,
fragmentShaderId,
modelViewMatLoc,
projMatLoc,
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

void diamond_step(float map[MAP_SIZE][MAP_SIZE], int sideLength, int halfSide, int size, int yRange, int x, int y) {
	double avg = map[(x - halfSide + size - 1) % (size - 1)][y] +
		map[(x + halfSide) % (size - 1)][y] +
		map[x][(y + halfSide) % (size - 1)] +
		map[x][(y - halfSide + size - 1) % (size - 1)];
	avg /= 4.0 + randomFloat(-1.0f, 1.0f);
	map[x][y] = avg;

	if (x == 0) map[size - 1][y] = avg;
	if (y == 0) map[x][size - 1] = avg;
}

void square_step(float map[MAP_SIZE][MAP_SIZE], int sideLength, int halfSide, int size, int yRange, int x, int y) {
	double avg = map[x][y] + map[x + sideLength][y] + map[x][y + sideLength] + map[x + sideLength][y + sideLength];
	avg /= 4.0;
	map[x + halfSide][y + halfSide] = avg + randomFloat(-1.0f, 1.0f);
}

inline static float random(int range) {
	return (rand() % (range * 2)) - range;
}

void diamondStep(float map[MAP_SIZE][MAP_SIZE], int x, int z, int reach) {
	int count = 0;
	float avg = 0.0f;
	if (x - reach >= 0) {
		avg += map[x - reach][z];
		count++;
	}

	if (x + reach < MAP_SIZE) {
		avg += map[x + reach][z];
		count++;
	}

	if (z - reach >= 0) {
		avg += map[x][z - reach];
		count++;
	}

	if (z + reach < MAP_SIZE) {
		avg += map[x][z + reach];
		count++;
	}

	avg += random(reach);
	avg /= count;
	map[x][z] = (int)avg;
}

void squareStep(float map[MAP_SIZE][MAP_SIZE], int x, int z, int reach) {
	int count = 0;
	float avg = 0.0f;

	if (x - reach >= 0 && z - reach >= 0) {
		avg += map[x - reach][z - reach];
		count++;
	}

	if (x - reach >= 0 && z + reach < MAP_SIZE) {
		avg += map[x - reach][z + reach];
		count++;
	}

	if (x + reach < MAP_SIZE && z - reach >= 0) {
		avg += map[x + reach][z - reach];
		count++;
	}

	if (x + reach < MAP_SIZE && z + reach < MAP_SIZE) {
		avg += map[x + reach][z + reach];
		count++;
	}

	avg += random(reach);
	avg /= count;
	map[x][z] = round(avg);
}

void diamondSquare(float map[MAP_SIZE][MAP_SIZE], int size) {
	int half = size / 2;
	if (half < 1)
		return;

	for (int z = half; z < MAP_SIZE; z += size) {
		for (int x = half; x < MAP_SIZE; x += size) {
			squareStep(map, x % MAP_SIZE, z % MAP_SIZE, half);
		}
	}

	int col = 0;
	for (int x = 0; x < MAP_SIZE; x += half) {
		col++;
		if (col % 2 == 1) {
			for (int z = half; z < MAP_SIZE; z += size) {
				diamondStep(map, x % MAP_SIZE, z % MAP_SIZE, half);
			} 
		}
		else {
			for (int z = 0; z < MAP_SIZE; z += size) {
				diamondStep(map, x % MAP_SIZE, z % MAP_SIZE, half); 
			}
		}
		diamondSquare(map, size/2);
	}
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

	diamondSquare(terrain, MAP_SIZE);

/*	while (step_size >= 1) {

		for (int x = 0; x < MAP_SIZE - 1; x += step_size) {
			for (int y = 0; y < MAP_SIZE - 1; y += step_size) {
				square_step(terrain, step_size, step_size / 2, MAP_SIZE - 1, H, x, y);
			}
		}

		for (int x = 0; x < MAP_SIZE - 1; x += step_size) {
			for (int y = 0; y < MAP_SIZE - 1; y += step_size) {
				diamond_step(terrain, step_size, step_size / 2, MAP_SIZE - 1, H, x, y);
			}
		}



		rand_max = rand_max * pow(2, -H);
		step_size /= 2;
	} */

	// Intialise vertex array
	int i = 0;

	for (int z = 0; z < MAP_SIZE; z++)
	{
		for (int x = 0; x < MAP_SIZE; x++)
		{
			// Set the coords (1st 4 elements) and a default colour of black (2nd 4 elements) 
			terrainVertices[i] = { { (float)x, terrain[x][z], (float)z, 1.0 }, { 0.0, 0.0, 0.0, 1.0 } };
			i++;
		}
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


	static const Material terrainFandB = {
		vec4(1.0, 1.0, 1.0, 1.0),
		vec4(1.0, 1.0, 1.0, 1.0),
		vec4(1.0, 1.0, 1.0, 1.0),
		vec4(1.0, 1.0, 1.0, 1.0),
		50.0f
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
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(terrainVertices[0]), (GLvoid*)sizeof(terrainVertices[0].coords));
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

	///////////////////////////////////////

	// Obtain modelview matrix uniform location and set value.
	mat4 modelViewMat = mat4(1.0);
	// Move terrain into view - glm::translate replaces glTranslatef
	modelViewMat = translate(modelViewMat, vec3(-2.5f, -2.5f, -10.0f)); // 5x5 grid
	modelViewMatLoc = glGetUniformLocation(programId, "modelViewMat");
	glUniformMatrix4fv(modelViewMatLoc, 1, GL_FALSE, value_ptr(modelViewMat));

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