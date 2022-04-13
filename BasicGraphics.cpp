#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <vector>
#include <GL/glew.h>					
#include <GLFW/glfw3.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/transform.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/string_cast.hpp"
#include "glm/gtc/type_ptr.hpp"
using namespace std;

// Global Variable for rotation Angle
float rotAngle = 0.0;

//Global Variable for camera position:
glm::vec3 eye = glm::vec3(0, 0, 1);

//Global Variable for Camera Look At point:
glm::vec3 lookAt = glm::vec3(0, 0, 0);

//Global Variable for Mouse Position:
glm::vec2 mousePos;

// Struct for holding vertex data
struct Vertex {
	glm::vec3 position;
	glm::vec4 color;
	glm::vec3 normal;
};

//Struct for holding Pointlight Data
struct PointLight {
	glm::vec4 pos;
	glm::vec4 color;
};

//Global light variable
PointLight light;

// Struct for holding mesh data
struct Mesh {
	vector<Vertex> vertices;
	vector<unsigned int> indices;
};

// Struct for holding OpenGL mesh
struct MeshGL {
	GLuint VBO = 0;
	GLuint EBO = 0;
	GLuint VAO = 0;
	int indexCnt = 0;
};

// Read from file and dump in string
string readFileToString(string filename) {
	// Open file
	ifstream file(filename);
	// Could we open file?
	if(!file || file.fail()) {
		cerr << "ERROR: Could not open file: " << filename << endl;
		const char *m = ("ERROR: Could not open file: " + filename).c_str();
		throw runtime_error(m);
	}

	// Create output stream to receive file data
	ostringstream outS;
	outS << file.rdbuf();
	// Get actual string of file contents
	string allS = outS.str();
	// Close file
	file.close();
	// Return string
	return allS;
}

// Print out shader code
void printShaderCode(string &vertexCode, string &fragCode) {
	cout << "***********************" << endl;
	cout <<"** VERTEX SHADER CODE **" << endl;
	cout << "***********************" << endl;
	cout << vertexCode << endl;
	cout << "*************************" << endl;
	cout <<"** FRAGMENT SHADER CODE **" << endl;
	cout << "*************************" << endl;
	cout << fragCode << endl;
	cout << "*************************" << endl;
}

//Convert aiMatrix4x4 to glm::mat4
void aiMatToGLM4(aiMatrix4x4 &a, glm::mat4 &m) {
	for(int i = 0; i < 4; i++) {
		for(int j = 0; j < 4; j++) {
			m[j][i] = a[i][j];
		}
	}
}

//Print number of tabs given level in tree
void printTab(int cnt) {
	for(int i = 0; i < cnt; i++) {
		cout << "\t";
	}
}

//Print information about a particular aiNode and current Matrix
void printNodeInfo(aiNode *node, glm::mat4 &nodeT, glm::mat4 &parentMat, 
	glm::mat4 &currentMat, int level) {
	printTab(level);
	cout << "NAME: " << node->mName.C_Str() << endl;
	printTab(level);
	cout << "NUM MESHES: " << node->mNumMeshes << endl;
	printTab(level);
	cout << "NUM CHILDREN: " << node->mNumChildren << endl;
	printTab(level);
	cout << "Parent Model Matrix: " << glm::to_string(parentMat) << endl;
	printTab(level);
	cout << "Current Model Matrix: " << glm::to_string(currentMat) << endl;
	cout << endl;
}

// GLFW error callback
static void error_callback(int error, const char* description) {
	cerr << "ERROR " << error << ": " << description << endl;
}

// GLFW setup
GLFWwindow* setupGLFW(int major, int minor, int windowWidth, int windowHeight, bool debugging) {
	
	// Set GLFW error callback
	glfwSetErrorCallback(error_callback);

	// (Try to) initialize GLFW
	if (!glfwInit()) {
		exit(EXIT_FAILURE);
	}

	// Force specific OpenGL version
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, major);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, minor);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

	// Request debugging context?
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, debugging);
	
	// Create our GLFW window
	GLFWwindow* window = glfwCreateWindow(	windowWidth, windowHeight, 
											"I Can See the Code Morpheus", 
											NULL, NULL);

	// Were we able to make it?
	if (!window) {
		// Kill GLFW and exit program
		glfwTerminate();
		exit(EXIT_FAILURE);
	}

	// We want to draw to this window, so make the OpenGL context associated with this window our current context.
	glfwMakeContextCurrent(window);

	// Basically, turning VSync on (so we will wait until the screen is updated once before swapping the back and front buffers
	glfwSwapInterval(1);

	// Return window
	return window;
}

// Cleanup GLFW
void cleanupGLFW(GLFWwindow* window) {
	glfwDestroyWindow(window);
	glfwTerminate();
}

// GLEW setup
void setupGLEW(GLFWwindow* window) {
	
	// MAC-SPECIFIC: Some issues occur with using OpenGL core and GLEW; so, we'll use the experimental version of GLEW
	glewExperimental = true;

	// (Try to) initalize GLEW
	GLenum err = glewInit();

	if (GLEW_OK != err) {
		// We couldn't start GLEW, so we've got to go.
		// Kill GLFW and get out of here
		cout << "ERROR: GLEW could not start: " << glewGetErrorString(err) << endl;
		cleanupGLFW(window);
		exit(EXIT_FAILURE);
	}

	cout << "GLEW initialized; version ";
	cout << glewGetString(GLEW_VERSION) << endl;
}

// Check OpenGL version
void checkOpenGLVersion() {
	GLint glMajor, glMinor;
	glGetIntegerv(GL_MAJOR_VERSION, &glMajor);
	glGetIntegerv(GL_MINOR_VERSION, &glMinor);
	cout << "OpenGL context version: ";
	cout << glMajor << "." << glMinor << endl;
	cout << "Supported GLSL version is ";
	cout << glGetString(GL_SHADING_LANGUAGE_VERSION) << endl;
}

// OpenGL debugging callback
// Taken from: https://learnopengl.com/In-Practice/Debugging
void APIENTRY openGLDebugCallback(	GLenum source, 
									GLenum type, 
									unsigned int id, 
									GLenum severity, 
									GLsizei length, 
									const char *message, 
									const void *userParam) {
    // ignore non-significant error/warning codes
    if(id == 131169 || id == 131185 || id == 131218 || id == 131204) return; 

    cout << "---------------" << endl;
    cout << "Debug message (" << id << "): " <<  message << endl;

    switch (source)
    {
        case GL_DEBUG_SOURCE_API:             cout << "Source: API"; break;
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   cout << "Source: Window System"; break;
        case GL_DEBUG_SOURCE_SHADER_COMPILER: cout << "Source: Shader Compiler"; break;
        case GL_DEBUG_SOURCE_THIRD_PARTY:     cout << "Source: Third Party"; break;
        case GL_DEBUG_SOURCE_APPLICATION:     cout << "Source: Application"; break;
        case GL_DEBUG_SOURCE_OTHER:           cout << "Source: Other"; break;
    } std::cout << std::endl;

    switch (type)
    {
        case GL_DEBUG_TYPE_ERROR:               cout << "Type: Error"; break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: cout << "Type: Deprecated Behaviour"; break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  cout << "Type: Undefined Behaviour"; break; 
        case GL_DEBUG_TYPE_PORTABILITY:         cout << "Type: Portability"; break;
        case GL_DEBUG_TYPE_PERFORMANCE:         cout << "Type: Performance"; break;
        case GL_DEBUG_TYPE_MARKER:              cout << "Type: Marker"; break;
        case GL_DEBUG_TYPE_PUSH_GROUP:          cout << "Type: Push Group"; break;
        case GL_DEBUG_TYPE_POP_GROUP:           cout << "Type: Pop Group"; break;
        case GL_DEBUG_TYPE_OTHER:               cout << "Type: Other"; break;
    } std::cout << std::endl;
    
    switch (severity)
    {
        case GL_DEBUG_SEVERITY_HIGH:         cout << "Severity: high"; break;
        case GL_DEBUG_SEVERITY_MEDIUM:       cout << "Severity: medium"; break;
        case GL_DEBUG_SEVERITY_LOW:          cout << "Severity: low"; break;
        case GL_DEBUG_SEVERITY_NOTIFICATION: cout << "Severity: notification"; break;
    } cout << endl;
    cout << endl;
}

// Check and setup debugging
void checkAndSetupOpenGLDebugging() {
	// If we have a debug context, we can connect a callback function for OpenGL errors...
	int flags; 
	glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
	if(flags & GL_CONTEXT_FLAG_DEBUG_BIT) {
		// Enable debug output
		glEnable(GL_DEBUG_OUTPUT);
		// Call debug output function when error occurs
    	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS); 
		// Attach error callback
    	glDebugMessageCallback(openGLDebugCallback, nullptr);
		// Control output
		// * ALL messages...
    	glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
		// * Only high severity errors from the OpenGL API...
		// glDebugMessageControl(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_ERROR, GL_DEBUG_SEVERITY_HIGH, 0, nullptr, GL_TRUE); 
	}
}

// GLSL Compiling/Linking Error Check
// Returns GL_TRUE if compile was successful; GL_FALSE otherwise.
GLint checkGLSLError(GLuint ID, bool isCompile) {

	GLint resultGL = GL_FALSE;
	int infoLogLength;
	char *errorMessage = nullptr;

	if(isCompile) {
		// Get the compilation status and message length
		glGetShaderiv(ID, GL_COMPILE_STATUS, &resultGL);	
		glGetShaderiv(ID, GL_INFO_LOG_LENGTH, &infoLogLength);
	}
	else {
		// Get linking status and message length
		glGetProgramiv(ID, GL_LINK_STATUS, &resultGL);
		glGetProgramiv(ID, GL_INFO_LOG_LENGTH, &infoLogLength);
	}

	// Make sure length is at least one and allocate space for message	
	infoLogLength = (infoLogLength > 1) ? infoLogLength : 1;
	errorMessage = new char[infoLogLength];	

	// Get actual message
	if(isCompile)
		glGetShaderInfoLog(ID, infoLogLength, NULL, errorMessage);		
	else	
		glGetProgramInfoLog(ID, infoLogLength, NULL, errorMessage);

	// Print error message
	if(infoLogLength > 1)
		cout << errorMessage << endl;

	// Cleanup
	if(errorMessage) delete [] errorMessage;

	// Return OpenGL error
	return resultGL;
}

// Creates and compiles GLSL shader from code string; returns shader ID
GLuint createAndCompileShader(const char *shaderCode, GLenum shaderType) {
	// Create the shader ID
	GLuint shaderID = glCreateShader(shaderType);

	// Compile the vertex shader...
	cout << "Compiling shader..." << endl;
	glShaderSource(shaderID, 1, &shaderCode, NULL);
	glCompileShader(shaderID);

	// Checking result of compilation...
	GLint compileOK = checkGLSLError(shaderID, true);
	if (!compileOK || shaderID == 0) {
		glDeleteShader(shaderID);		
		cout << "Error compiling shader." << endl;
		throw runtime_error("Error compiling shader.");
	}

	// Return shader ID
	return shaderID;
}

// Given a list of compiled shaders, create and link a shader program (ID returned).
GLuint createAndLinkShaderProgram(std::vector<GLuint> allShaderIDs) {

	// Create program ID and attach shaders
	cout << "Linking program..." << endl;
	GLuint programID = glCreateProgram();
	for (GLuint &shaderID : allShaderIDs) {
		glAttachShader(programID, shaderID);
	}

	// Actually link the program
	glLinkProgram(programID);

	// Detach shaders (program already linked, successful or not)
	for (GLuint &shaderID : allShaderIDs) {
		glDetachShader(programID, shaderID);		
	}

	// Check linking
	GLint linkOK = checkGLSLError(programID, false);
	if (!linkOK || programID == 0) {		
		glDeleteProgram(programID);		
		cout << "Error linking shaders." << endl;
		throw runtime_error("Error linking shaders.");
	}

	// Return program ID
	return programID;
}

// Does the following:
// - Creates and compiles vertex and fragment shaders (from provided code strings)
// - Creates and links shader program
// - Deletes vertex and fragment shaders
GLuint initShaderProgramFromSource(string vertexShaderCode, string fragmentShaderCode) {
	GLuint vertID = 0;
	GLuint fragID = 0;
	GLuint programID = 0;

	try {
		// Create and compile shaders
		cout << "Vertex shader: ";
		vertID = createAndCompileShader(vertexShaderCode.c_str(), GL_VERTEX_SHADER);
		cout << "Fragment shader: ";
		fragID = createAndCompileShader(fragmentShaderCode.c_str(), GL_FRAGMENT_SHADER);

		// Create and link program
		programID = createAndLinkShaderProgram({ vertID, fragID });

		// Delete individual shaders
		glDeleteShader(vertID);
		glDeleteShader(fragID);

		// Success!
		cout << "Program successfully compiled and linked!" << endl;
	}
	catch (exception e) {
		// Cleanup shaders and shader program, just in case
		if (vertID) glDeleteShader(vertID);
		if (fragID) glDeleteShader(fragID);		
		// Rethrow exception
		throw e;
	}

	return programID;
}

void ExtractMeshData(aiMesh *mesh, Mesh &m) {
	m.vertices.clear();
	m.indices.clear();
	for(unsigned int i = 0; i < mesh->mNumVertices; i++){
		Vertex loopVert;
		loopVert.position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
		loopVert.color = glm::vec4(1.0, 1.0, 0.0, 1.0);
		loopVert.normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
		m.vertices.push_back(loopVert);

	}
	for(unsigned int j = 0; j < mesh->mNumFaces; j++) {
		aiFace face;
		face = mesh->mFaces[j];
		for(unsigned int k = 0; k < face.mNumIndices; k++) {
			m.indices.push_back(face.mIndices[k]);
		}

	}
}

//Generate Transformation to rotate around arbitrary point and axis:
 glm::mat4 makeLocalRotate(glm::vec3 offset, glm::vec3 axis, float angle) {
	 glm::mat4 translateNeg = glm::translate(-offset);
	 glm::mat4 translatePos = glm::translate(offset);
	 glm::mat4 rotate = glm::rotate(glm::radians(angle), glm::vec3(axis));
	 glm::mat4 compositeTransformArb = translatePos * rotate * translateNeg;

	 return compositeTransformArb;
 }

//Generate Transformation around local z axis
glm::mat4 makeRotateZ(glm::vec3 offset) {
	glm::mat4 translateNeg = glm::translate(-offset);
	glm::mat4 translatePos = glm::translate(offset);
	glm::mat4 rotate = glm::rotate(glm::radians(rotAngle), glm::vec3(0, 0, 1));
	glm::mat4 compositeTransformZ;
	compositeTransformZ = translatePos * rotate * translateNeg;

	return compositeTransformZ;
}

//GlFW Callback Function
static void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods) {
	glm::vec3 moveAxis;
    if(action == GLFW_PRESS || action == GLFW_REPEAT) {
        if(key == GLFW_KEY_ESCAPE) {
            glfwSetWindowShouldClose(window, true);
        }
        else if(key == GLFW_KEY_J) {
			rotAngle += 1.0;
        }
        else if(key == GLFW_KEY_K) {
			rotAngle -= 1.0;
        }
		else if(key == GLFW_KEY_W) {
			glm::vec3 direction = lookAt - eye;
			direction *= 0.1;
			lookAt += direction;
			eye += direction;
		}
		else if(key == GLFW_KEY_S) {
			glm::vec3 direction = lookAt - eye;
			direction *= 0.1;
			lookAt -= direction;
			eye -= direction;
		}
		else if(key == GLFW_KEY_D) {
			moveAxis = glm::cross(glm::vec3(0,1,0), -(lookAt - eye));
			moveAxis *= 0.1;
			lookAt +=moveAxis;
			eye += moveAxis;
		}
		else if(key == GLFW_KEY_A) {
			moveAxis = glm::cross(glm::vec3(0,1,0), -(lookAt - eye));
			moveAxis *= 0.1;
			lookAt -= moveAxis;
			eye -= moveAxis;
		}
		else if (key == GLFW_KEY_1) {
			light.color = glm::vec4(1, 1, 1, 1); //white
		}
		else if (key == GLFW_KEY_2) {
			light.color = glm::vec4(1, 0, 0, 1); //red
		}
		else if (key == GLFW_KEY_3) {
			light.color = glm::vec4(0, 1, 0, 1); //green
		}		
		else if (key == GLFW_KEY_4) {
			light.color = glm::vec4(0, 0, 1, 1); //blue
		}
    }
}

//Mouse Callback Function
static void mouse_position_callback(GLFWwindow* window, double xpos, double ypos) {
	int fWidth, fHeight;
	glm::mat4 xRot, yRot;
	glm::vec2 currMouse = glm::vec2(xpos, ypos);
	glm::vec4 look4 = glm::vec4(lookAt, 1.0);
	currMouse = mousePos - currMouse;
	glfwGetFramebufferSize(window, &fWidth, &fHeight);
	if(fWidth > 0 && fHeight > 0) {
		glm::vec2 relMotion = glm::vec2((float)currMouse.x / fWidth, (float)currMouse.y / fHeight);
		xRot = makeLocalRotate(eye, glm::vec3(0,1,0), 30.0f * relMotion.x);
		yRot = makeLocalRotate(eye, glm::cross(glm::vec3(0,-1,0), lookAt - eye), 30.0f * relMotion.y);
		look4 = yRot * xRot * look4;
		lookAt = glm::vec3(look4);
		
	}
	mousePos = glm::vec2(xpos, ypos);
	
}


// Create very simple mesh: a quad (4 vertices, 6 indices, 2 triangles)
void createSimpleQuad(Mesh &m) {
	// Clear out vertices and elements
	m.vertices.clear();
	m.indices.clear();

	// Create four corners
	Vertex upperLeft, upperRight;
	Vertex lowerLeft, lowerRight;
	Vertex vertex5; // Fifth Vertex created for assignment 2

	// Set positions of vertices
	// Note: glm::vec3(x, y, z)
	upperLeft.position = glm::vec3(-0.5, 0.5, 0.0);
	upperRight.position = glm::vec3(0.5, 0.5, 0.0);
	lowerLeft.position = glm::vec3(-0.5, -0.5, 0.0);
	lowerRight.position = glm::vec3(0.5, -0.5, 0.0);
	vertex5.position = glm::vec3(0.75, 0.0, 0.0);

	// Set vertex colors (red, green, blue, white)
	// Note: glm::vec4(red, green, blue, alpha)
	upperLeft.color = glm::vec4(1.0, 0.0, 0.0, 1.0);
	upperRight.color = glm::vec4(0.0, 1.0, 0.0, 1.0);
	lowerLeft.color = glm::vec4(0.0, 0.0, 1.0, 1.0);
	lowerRight.color = glm::vec4(1.0, 1.0, 1.0, 1.0);
	vertex5.color = glm::vec4(0.76, 0.65, 0.32, 1.0);

	// Add to mesh's list of vertices
	m.vertices.push_back(upperLeft);
	m.vertices.push_back(upperRight);	
	m.vertices.push_back(lowerLeft);
	m.vertices.push_back(lowerRight);
	m.vertices.push_back(vertex5);
	
	// Add indices for two triangles
	m.indices.push_back(0);
	m.indices.push_back(3);
	m.indices.push_back(1);

	m.indices.push_back(0);
	m.indices.push_back(2);
	m.indices.push_back(3);

	m.indices.push_back(1);
	m.indices.push_back(3);
	m.indices.push_back(4);
}

// Create OpenGL mesh (VAO) from mesh data
void createMeshGL(Mesh &m, MeshGL &mgl) {
	// Create Vertex Buffer Object (VBO)
	glGenBuffers(1, &(mgl.VBO));
	glBindBuffer(GL_ARRAY_BUFFER, mgl.VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex)*m.vertices.size(), m.vertices.data(), GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	
	// Create Vertex Array Object (VAO)
	glGenVertexArrays(1, &(mgl.VAO));

	// Enable VAO
	glBindVertexArray(mgl.VAO);

	// Enable the first three vertex attribute arrays
	glEnableVertexAttribArray(0);	// position
	glEnableVertexAttribArray(1);	// color
	glEnableVertexAttribArray(2);   // normal

	
	// Bind the VBO and set up data mappings so that VAO knows how to read it
	// 0 = pos (3 elements)
	// 1 = color (4 elements)
	glBindBuffer(GL_ARRAY_BUFFER, mgl.VBO);	

	// Attribute, # of components, type, normalized?, stride, array buffer offset
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

	
	// Create Element Buffer Object (EBO)
	glGenBuffers(1, &(mgl.EBO));
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mgl.EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER,
		m.indices.size() * sizeof(GLuint),
		m.indices.data(),
		GL_STATIC_DRAW);

	// Set index count
	mgl.indexCnt = (int)m.indices.size();

	// Unbind vertex array for now
	glBindVertexArray(0);
}

// Draw OpenGL mesh
void drawMesh(MeshGL &mgl) {
	glBindVertexArray(mgl.VAO);
	glDrawElements(GL_TRIANGLES, mgl.indexCnt, GL_UNSIGNED_INT, (void*)0);
	glBindVertexArray(0);		
}

//Render scene Recursively
void renderScene(vector<MeshGL> &allMeshes, aiNode *node, glm::mat4 parentMat,
		GLint modelMatLoc, GLint normMatLoc, glm::mat4 viewMat, int level) {
	aiMatrix4x4 nodeTrans = node->mTransformation;
	glm::mat4 nodeT;
	aiMatToGLM4(nodeTrans, nodeT);
	glm::mat4 modelMat;
	modelMat = parentMat * nodeT;
	glm::mat4 R = makeRotateZ(glm::vec3(modelMat[3]));
	glm::mat4 tmpModel = R * modelMat;
	glUniformMatrix4fv(modelMatLoc, 1, false, glm::value_ptr(tmpModel));
	glm::mat4 modelView = viewMat * tmpModel;
    glm::mat3 normMat = glm::transpose(glm::inverse(glm::mat3(modelView)));
	glUniformMatrix3fv(normMatLoc, 1, false, glm::value_ptr(normMat));
	for(unsigned int i = 0; i < node->mNumMeshes; i++) {
		int index = node->mMeshes[i];
		drawMesh(allMeshes.at(index));
	}
	for(unsigned int j = 0; j < node->mNumChildren; j++)
		renderScene(allMeshes, node->mChildren[j], modelMat, modelMatLoc, normMatLoc, viewMat, level+1);
}

// Cleanup OpenGL mesh
void cleanupMesh(MeshGL &mgl) {

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glDeleteBuffers(1, &(mgl.VBO));
	mgl.VBO = 0;

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glDeleteBuffers(1, &(mgl.EBO));
	mgl.EBO = 0;

	glBindVertexArray(0);
	glDeleteVertexArrays(1, &(mgl.VAO));
	mgl.VAO = 0;

	mgl.indexCnt = 0;
}

// Main 
int main(int argc, char **argv) {
	
	//check if model is loaded on command line
	if(argc <=1) {
		cout << "Error with Loading model. Now Exiting.";
		exit(1);
	}

	//Create the model importer
	Assimp::Importer importer;

	//Load model
	const aiScene *scene = importer.ReadFile(argv[1], aiProcess_Triangulate || aiProcess_FlipUVs ||
		 aiProcess_GenNormals || aiProcess_JoinIdenticalVertices);

	//Check Model loaded correctly
	if(!scene || scene->mFlags && AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
		cerr << "Error: " << importer.GetErrorString() << endl;
		exit(1);
	}
		
	vector<MeshGL> meshVector;

	// Are we in debugging mode?
	bool DEBUG_MODE = true;

	// GLFW setup
	GLFWwindow* window = setupGLFW(4, 3, 800, 800, DEBUG_MODE);

	//Set Key Callback
	glfwSetKeyCallback(window, keyCallback);

	// GLEW setup
	setupGLEW(window);

	// Check OpenGL version
	checkOpenGLVersion();

	// Set up debugging (if requested)
	if(DEBUG_MODE) checkAndSetupOpenGLDebugging();

	//Get Initial Mouse Position
	double mx, my;
	glfwGetCursorPos(window, &mx, &my);
	mousePos = glm::vec2(mx, my);

	//Set Mouse Cursor Motion Function
	glfwSetCursorPosCallback(window, mouse_position_callback);

	//Hide the mouse
	 glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	// Set the background color to a shade of blue
	glClearColor(0.64f, 0.93f, 0.4f, 1.0f);	

	// Create and load shader
	GLuint programID = 0;
	try {		
		// Load vertex shader code and fragment shader code
		string vertexCode = readFileToString("./Basic.vs");
		string fragCode = readFileToString("./Basic.fs");

		// Print out shader code, just to check
		if(DEBUG_MODE) printShaderCode(vertexCode, fragCode);

		// Create shader program from code
		programID = initShaderProgramFromSource(vertexCode, fragCode);
	}
	catch (exception e) {		
		// Close program
		cleanupGLFW(window);
		exit(EXIT_FAILURE);
	}

	//Do light stuff
	light.pos = glm::vec4(0.5, 0.5, 0.5, 1);
	light.color = glm::vec4(1, 1, 1, 1);
	

	//Get View Matrix Location
	GLint viewMatLoc = glGetUniformLocation(programID, "viewMat");

	//Get Projection Matrix Location
	GLint projMatLoc = glGetUniformLocation(programID, "projMat");

	//Get model matrix location
	GLint modelMatLoc = glGetUniformLocation(programID, "modelMat");

	//Get light info location
	GLint lightPosLoc = glGetUniformLocation(programID, "light.pos");
	GLint lightColorLoc = glGetUniformLocation(programID, "light.color");

	//Get normal matrix location
	GLint normMatLoc = glGetUniformLocation(programID, "normMat");

	cout << modelMatLoc << endl;
	cout << lightPosLoc << " " << lightColorLoc << " " << normMatLoc << endl;
	
	// Create simple quad
	Mesh m;
	createSimpleQuad(m);

	//Fill our new Vector
	for(unsigned int cnt = 0; cnt < scene->mNumMeshes; cnt++) {
		Mesh loopMesh;
		MeshGL loopMeshGl;
		ExtractMeshData(scene->mMeshes[cnt], loopMesh);
		createMeshGL(loopMesh, loopMeshGl);
		meshVector.push_back(loopMeshGl);
	}

	// Create OpenGL mesh (VAO) from data
	MeshGL mgl;
	createMeshGL(m, mgl);
	
	// Enable depth testing
	glEnable(GL_DEPTH_TEST);

	while (!glfwWindowShouldClose(window)) {
		// Set viewport size
		int fwidth, fheight;
		glfwGetFramebufferSize(window, &fwidth, &fheight);
		glViewport(0, 0, fwidth, fheight);

		// Clear the framebuffer
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Use shader program
		glUseProgram(programID);

		//Pass View matrix to shader
		glm::mat4 viewMat = glm::lookAt(eye, lookAt, glm::vec3(0,1,0));
		glUniformMatrix4fv(viewMatLoc, 1, false, glm::value_ptr(viewMat));

		//Get Framebuffer size
		int fBuffW, fBuffH;
		float aspectRatio;
		glfwGetFramebufferSize(window, &fBuffW, &fBuffH);
		if(fBuffW == 0 || fBuffH == 0) {
			aspectRatio = 1.0;
		}
		else {
			aspectRatio = (float)fBuffW / fBuffH;
		}

		glm::mat4 projMat = glm::perspective(glm::radians(90.0f), aspectRatio, 0.01f, 50.0f);
		glUniformMatrix4fv(projMatLoc, 1, false, glm::value_ptr(projMat));

		//calculate position of light in view space
        glm::vec4 curLightPos = viewMat * light.pos;
        glUniform4fv(lightPosLoc, 1, glm::value_ptr(curLightPos));
        glUniform4fv(lightColorLoc, 1, glm::value_ptr(light.color));

		//Draw our Models
		renderScene(meshVector, scene->mRootNode, glm::mat4(1.0), modelMatLoc, normMatLoc, viewMat, 0);

		// Swap buffers and poll for window events		
		glfwSwapBuffers(window);
		glfwPollEvents();

		// Sleep for 15 ms
		this_thread::sleep_for(chrono::milliseconds(15));
	}

	// Clean up mesh
	cleanupMesh(mgl);

	//Clean up meshes for model
	for(unsigned int g = 0; g <meshVector.size(); g++) {
		cleanupMesh(meshVector[g]);
	}

	// Clean up shader programs
	glUseProgram(0);
	glDeleteProgram(programID);
		
	// Destroy window and stop GLFW
	cleanupGLFW(window);

	return 0;
}
