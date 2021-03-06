#include <GL/glew.h>	// should be included at the beginning!
#include <GLFW/glfw3.h>
#include <iostream>
#include <cstdlib>
#include <string>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include "tiny_obj_loader.h"

#define GLM_FORCE_RADIANS
#define PI 3.1415
#define e 2.71828

// Pixel should be double in OSX
// So we will check if this is run on OSX
#ifdef __APPLE__
	#define PIXELMULTI 2.0
#else
	#define PIXELMULTI 1.0
#endif

struct object_struct{
	unsigned int program;
	unsigned int vao;
	unsigned int vbo[4];
	unsigned int texture;
	glm::mat4 model;
	glm::vec3 ambient;
	object_struct(): model(glm::mat4(1.0f)){}
};

// Vertex attributes for a quad that fills the entire screen in Normalized Device Coordinates.
GLfloat screenVertices[] = {
        // Positions   // TexCoords
        -1.0f,  1.0f,  0.0f, 1.0f,	//left-top
        -1.0f, -1.0f,  0.0f, 0.0f,	//left-bottom
         1.0f, -1.0f,  1.0f, 0.0f,	//right-bottom

        -1.0f,  1.0f,  0.0f, 1.0f,	//left-top
         1.0f, -1.0f,  1.0f, 0.0f,	//right-bottom
         1.0f,  1.0f,  1.0f, 1.0f	//right-top
};
GLuint frameBuffer;
GLuint texColorBuffer;
GLuint screenVAO, screenVBO;
GLfloat Zoom = 1.5;
double xpos,ypos;	// Mouse Pos
double circleArea = 5000.0;
double stdDev = 0.84089642;		// stdDev for Gaussian Blur
bool playing = true;

std::vector<object_struct> objects;	// VAO: vertex array object,vertex buffer object and texture(color) for objs
unsigned int FlatProgram, GouraudProgram, PhongProgram, BlinnProgram, ScreenProgram;	// Five shader program
int sun, earth;						// index in objects
int ProgramIndex = 2;				// To indicate which program is used now
std::vector<int> indicesCount;		// Number of indices of objs


static void error_callback(int error, const char* description)
{
	fputs(description, stderr);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	// Press Esc to exit this program
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GL_TRUE);

	// Use ↑ ↓ to adjust zoom depth
	else if (key == GLFW_KEY_UP && (action == GLFW_REPEAT || action == GLFW_PRESS))
		Zoom = (Zoom+0.1<=2.1) ? Zoom+0.1 : Zoom;
	else if (key == GLFW_KEY_DOWN && (action == GLFW_REPEAT || action == GLFW_PRESS))
		Zoom = (Zoom-0.1>=0.9) ? Zoom-0.1 : Zoom;

	// Use ← → to adjust stdDev
	else if (key == GLFW_KEY_LEFT && (action == GLFW_REPEAT || action == GLFW_PRESS))
		stdDev = (stdDev-0.4>=0.0) ? stdDev-0.4 : stdDev;
	else if (key == GLFW_KEY_RIGHT && (action == GLFW_REPEAT || action == GLFW_PRESS))
		stdDev = (stdDev+0.4<=16.0) ? stdDev+0.4 : stdDev;

	else if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
		playing = (playing == true) ? false : true;
}

static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	// Use mouse scrolling to adjust circle area
	if (circleArea+1000*yoffset >= 100 && circleArea+1000*yoffset <= 50000)
		circleArea = circleArea + 1000*yoffset;
}

// Setup shader program here
static unsigned int setup_shader(const char *vertex_shader, const char *fragment_shader)
{
	GLuint vs=glCreateShader(GL_VERTEX_SHADER);

	// bind the source code to VS object
	glShaderSource(vs, 1, (const GLchar**)&vertex_shader, nullptr);

	// compile the source code of VS object
	glCompileShader(vs);

	/***** To check if compiling is success or not *****/
	int status, maxLength;
	char *infoLog=nullptr;
	glGetShaderiv(vs, GL_COMPILE_STATUS, &status);	// get status
	if(status==GL_FALSE)
	{
		glGetShaderiv(vs, GL_INFO_LOG_LENGTH, &maxLength);	//get info length

		/* The maxLength includes the NULL character */
		infoLog = new char[maxLength];

		glGetShaderInfoLog(vs, maxLength, &maxLength, infoLog);	//get info

		fprintf(stderr, "Vertex Shader Error: %s\n", infoLog);

		/* Handle the error in an appropriate way such as displaying a message or writing to a log file. */
		/* In this simple program, we'll just leave */
		delete [] infoLog;
		return 0;
	}
	/***************************************************/

	//create a shader object and set shader type to run on a programmable fragment processor
	GLuint fs=glCreateShader(GL_FRAGMENT_SHADER);

	// bind the source code to FS object
	glShaderSource(fs, 1, (const GLchar**)&fragment_shader, nullptr);

	// compile the source code of FS object
	glCompileShader(fs);

	glGetShaderiv(fs, GL_COMPILE_STATUS, &status);	// get compile status
	if(status==GL_FALSE)
	{
		glGetShaderiv(fs, GL_INFO_LOG_LENGTH, &maxLength);	//get info length

		/* The maxLength includes the NULL character */
		infoLog = new char[maxLength];

		glGetShaderInfoLog(fs, maxLength, &maxLength, infoLog);	//get info

		fprintf(stderr, "Fragment Shader Error: %s\n", infoLog);

		/* Handle the error in an appropriate way such as displaying a message or writing to a log file. */
		/* In this simple program, we'll just leave */
		delete [] infoLog;
		return 0;
	}

	// Create a program object
	unsigned int program=glCreateProgram();

	// Attach our shaders to our program
	glAttachShader(program, vs);
	glAttachShader(program, fs);

	// Link the shader in the program
	glLinkProgram(program);

	// To check if linking is success or not
	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if(status==GL_FALSE)
	{
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);

		/* The maxLength includes the NULL character */
		infoLog = new char[maxLength];
		glGetProgramInfoLog(program, maxLength, NULL, infoLog);
		glGetProgramInfoLog(program, maxLength, &maxLength, infoLog);

		fprintf(stderr, "Link Error: %s\n", infoLog);

		/* Handle the error in an appropriate way such as displaying a message or writing to a log file. */
		/* In this simple program, we'll just leave */
		delete [] infoLog;
		return 0;
	}
	// No more need to be used, delete it
	glDeleteShader(vs);
	glDeleteShader(fs);
	return program;
}

static std::string readfile(const char *filename)
{
	std::ifstream ifs(filename);
	if(!ifs)
		exit(EXIT_FAILURE);
	return std::string( (std::istreambuf_iterator<char>(ifs)),
			(std::istreambuf_iterator<char>()));
}

// mini bmp loader written by HSU YOU-LUN
static unsigned char *load_bmp(const char *bmp, unsigned int *width, unsigned int *height, unsigned short int *bits)
{
	unsigned char *result=nullptr;
	FILE *fp = fopen(bmp, "rb");
	if(!fp)
		return nullptr;
	char type[2];
	unsigned int size, offset;
    // check for magic signature
	fread(type, sizeof(type), 1, fp);
	if(type[0]==0x42 || type[1]==0x4d){
		fread(&size, sizeof(size), 1, fp);

		// ignore 2 two-byte reversed fields
		fseek(fp, 4, SEEK_CUR);
		fread(&offset, sizeof(offset), 1, fp);

		// ignore size of bmpinfoheader field
		fseek(fp, 4, SEEK_CUR);
		fread(width, sizeof(*width), 1, fp);
		fread(height, sizeof(*height), 1, fp);

		// ignore planes field
		fseek(fp, 2, SEEK_CUR);
		fread(bits, sizeof(*bits), 1, fp);

		unsigned char *pos = result = new unsigned char[size-offset];
		fseek(fp, offset, SEEK_SET);
		// Read the real content
		while(size-ftell(fp)>0)
			pos+=fread(pos, 1, size-ftell(fp), fp);
	}
	fclose(fp);
	return result;
}

// Add vertex data and bmp into VBO and setup VAO, then put them into objects array
static int add_obj(unsigned int program, const char *filename,const char *texbmp)
{
	object_struct new_node;

	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;

	// Load .obj file
	std::string err = tinyobj::LoadObj(shapes, materials, filename);
	if (!err.empty()||shapes.size()==0)
	{
		std::cerr<<err<<std::endl;
		exit(1);
	}

	// Generate Vertex Array Objects
	glGenVertexArrays(1, &new_node.vao);
	glGenBuffers(4, new_node.vbo);

	// Generate texture objects
	glGenTextures(1, &new_node.texture);

	// Start VAO recording
	glBindVertexArray(new_node.vao);

	// Upload postion array
	glBindBuffer(GL_ARRAY_BUFFER, new_node.vbo[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat)*shapes[0].mesh.positions.size(),
			shapes[0].mesh.positions.data(), GL_STATIC_DRAW);

	// Put into vs's input (location=0)
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

	if(shapes[0].mesh.texcoords.size()>0)
	{
		// Upload texCoord array
		glBindBuffer(GL_ARRAY_BUFFER, new_node.vbo[1]);		// bind buffer type
		// copy data into buffers
		glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat)*shapes[0].mesh.texcoords.size(),
				shapes[0].mesh.texcoords.data(), GL_STATIC_DRAW);

		// Put into vs's input (location=1)
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);

		glBindTexture( GL_TEXTURE_2D, new_node.texture);
		unsigned int width, height;
		unsigned short int bits;
		unsigned char *bgr=load_bmp(texbmp, &width, &height, &bits);
		GLenum format = (bits == 24? GL_BGR: GL_BGRA);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, format, GL_UNSIGNED_BYTE, bgr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
		glGenerateMipmap(GL_TEXTURE_2D);
		delete [] bgr;
	}

	if(shapes[0].mesh.normals.size()>0)
	{
		// Upload normal array
		glBindBuffer(GL_ARRAY_BUFFER, new_node.vbo[2]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat)*shapes[0].mesh.normals.size(),
				shapes[0].mesh.normals.data(), GL_STATIC_DRAW);

		// Put into vs's input (location=2)
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, 0);
	}

	// Setup index buffer for glDrawElements
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, new_node.vbo[3]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint)*shapes[0].mesh.indices.size(),
			shapes[0].mesh.indices.data(), GL_STATIC_DRAW);

	indicesCount.push_back(shapes[0].mesh.indices.size());

	// End of VAO recording
	glBindVertexArray(0);

	new_node.program = program;

	objects.push_back(new_node);
	return objects.size()-1;
}

// This function will initialize all works before using framebuffer
static void frameBuffer_init()
{
	glGenVertexArrays(1, &screenVAO);
	glGenBuffers(1, &screenVBO);
	glBindVertexArray(screenVAO);
	glBindBuffer(GL_ARRAY_BUFFER, screenVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(screenVertices), &screenVertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(GLfloat), (GLvoid*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(GLfloat), (GLvoid*)(2*sizeof(GLfloat)));
	glBindVertexArray(0);

	glGenFramebuffers(1, &frameBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);

	// Generate texture buffer and bind it to framebuffer
	glGenTextures(1, &texColorBuffer);
	glBindTexture(GL_TEXTURE_2D, texColorBuffer);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 800*PIXELMULTI, 600*PIXELMULTI, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texColorBuffer, 0);

	// Generate render buffer and bind it to framebuffer
	GLuint rbo;
	glGenRenderbuffers(1, &rbo);
	glBindRenderbuffer(GL_RENDERBUFFER, rbo);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 800*PIXELMULTI, 600*PIXELMULTI);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "FrameBuffer is not complete!!!" << std::endl;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// Free all objects and memory space
static void releaseObjects()
{
	for(int i=0;i<objects.size();i++){
		glDeleteVertexArrays(1, &objects[i].vao);
		glDeleteTextures(1, &objects[i].texture);
		glDeleteBuffers(4, objects[i].vbo);
	}
	// Delete all the program
	glDeleteProgram(FlatProgram);
	glDeleteProgram(GouraudProgram);
	glDeleteProgram(PhongProgram);
	glDeleteProgram(BlinnProgram);
	glDeleteProgram(ScreenProgram);
}

// This function will return mat3 sample gauss matrix with given sigma
static glm::mat3 buildGaussianMat3()
{
	double element[9] = {0};
	double sumArg = 0.0;
	for (int i=-1;i<=1;i++)
	{
		for (int j=-1;j<=1;j++)
		{
			double arg = 1/(2*PI*stdDev*stdDev)*pow(e, -(i*i+j*j)/(2*stdDev*stdDev));
			element[3*i+j+4] = arg;
			sumArg = sumArg + arg;
		}
	}
	// Normalize
	for (int k=0;k<9;k++)
		element[k] = element[k] / sumArg;

	return glm::make_mat3(element);
}

// The key function of HW2, you can change Uniform variable here
static void setUniformMat4(unsigned int program, const std::string &name, const glm::mat4 &mat)
{
	// This line can be ignore. But, if you have multiple shader program
	// You must check if currect binding is the one you want
	glUseProgram(program);
	GLint loc=glGetUniformLocation(program, name.c_str());
	if(loc==-1) return;

	// mat4 of glm is column major, same as opengl
	// we don't need to transpose it. so..GL_FALSE
	// Put `mat` into uniform variable in vs.txt
	glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(mat));
}

static void setUniformMat3(unsigned int program, const std::string &name, const glm::mat3 &mat)
{
	// This line can be ignore. But, if you have multiple shader program
	// You must check if currect binding is the one you want
	glUseProgram(program);
	GLint loc=glGetUniformLocation(program, name.c_str());
	if(loc==-1) return;

	// mat4 of glm is column major, same as opengl
	// we don't need to transpose it. so..GL_FALSE
	// Put `mat` into uniform variable in vs.txt
	glUniformMatrix3fv(loc, 1, GL_FALSE, glm::value_ptr(mat));
}

// A function which is similar to setUniformMat4, but this time it is for vec3
static void setUniformVec3(unsigned int program, const std::string &name, const glm::vec3 &vector)
{
	// This line can be ignore. But, if you have multiple shader program
	// You must check if currect binding is the one you want
	glUseProgram(program);
	GLint loc=glGetUniformLocation(program, name.c_str());
	if(loc==-1) return;

	glUniform3f(loc, vector.x, vector.y, vector.z);
}

static void setUniformVec2(unsigned int program, const std::string &name, const glm::vec2 &vector)
{
	// This line can be ignore. But, if you have multiple shader program
	// You must check if currect binding is the one you want
	glUseProgram(program);
	GLint loc=glGetUniformLocation(program, name.c_str());
	if(loc==-1) return;

	glUniform2f(loc, vector.x, vector.y);
}

// A function which is similar to setUniformMat4, but this time it is for vec3
static void setUniformFloat(unsigned int program, const std::string &name, const GLfloat &value)
{
	// This line can be ignore. But, if you have multiple shader program
	// You must check if currect binding is the one you want
	glUseProgram(program);
	GLint loc=glGetUniformLocation(program, name.c_str());
	if(loc==-1) return;

	glUniform1f(loc, value);
}

// Draw Object on window
static void render()
{
	/********* 1. Switch to framebuffer first and draw *********/
	glViewport(0.0, 0.0, 800*PIXELMULTI, 600*PIXELMULTI);
	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	for(int i=0;i<objects.size();i++){		// draw every object
		glUseProgram(objects[i].program);
		glBindVertexArray(objects[i].vao);
		glBindTexture(GL_TEXTURE_2D, objects[i].texture);

        // I change the model matrix and ambient strength here
		setUniformMat4(objects[i].program, "model", objects[i].model);
		setUniformVec3(objects[i].program, "ambientLight", objects[i].ambient);
		glDrawElements(GL_TRIANGLES, indicesCount[i], GL_UNSIGNED_INT, nullptr);
	}
	glBindVertexArray(0);
	/**********************************************************/

	/********* 2. Switch back to default and clear buffer *********/
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	/**************************************************************/

	/********* 3. Use screen shader to do post processing *********/
	glUseProgram(ScreenProgram);
	glBindVertexArray(screenVAO);
	glDisable(GL_DEPTH_TEST);
	glBindTexture(GL_TEXTURE_2D, texColorBuffer);
	setUniformFloat(ScreenProgram, "Zoom", Zoom);
	setUniformFloat(ScreenProgram, "pixelMulti", PIXELMULTI);
	setUniformFloat(ScreenProgram, "circleArea", circleArea);
	setUniformMat3(ScreenProgram, "gaussMat", buildGaussianMat3());
	setUniformVec2(ScreenProgram, "mouseLoc", glm::vec2(xpos,std::abs(600-ypos)));
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);
	/**************************************************************/

}

// This function can change the shading program one after another
static void changeProgram()
{
	switch(ProgramIndex) {
		case 1:	// change to Gouraud
			objects[sun].program = GouraudProgram;
			objects[earth].program = GouraudProgram;
			std::cout << "Gouraud Shading!!!" << std::endl;
			ProgramIndex = 2;
			break;
		case 2:	// change to Phong
			objects[sun].program = PhongProgram;
			objects[earth].program = PhongProgram;
			std::cout << "Phong Shading!!!" << std::endl;
			ProgramIndex = 3;
			break;
		case 3:	// change to Blinn
			objects[sun].program = BlinnProgram;
			objects[earth].program = BlinnProgram;
			std::cout << "Blinn-Phong Shading!!!" << std::endl;
			ProgramIndex = 4;
			break;
		case 4:	// change to Flat
			objects[sun].program = FlatProgram;
			objects[earth].program = FlatProgram;
			std::cout << "Flat Shading!!!" << std::endl;
			ProgramIndex = 1;
			break;
	}
	// Since we change the program, we have to set uniform again
	// Setup the MVP matrix for Sun
	setUniformMat4(objects[sun].program, "vp", glm::perspective(glm::radians(35.0f), 800.0f/600, 1.0f, 100.f)*
			glm::lookAt(glm::vec3(40.0f, 15.0f, 40.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f))*glm::mat4(1.0f));

	// Setup VP matrix for Earth
	setUniformMat4(objects[earth].program, "vp", glm::perspective(glm::radians(24.0f), 800.0f/600, 1.0f, 100.f)*
			glm::lookAt(glm::vec3(40.0f, 15.0f, 40.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f))*glm::mat4(1.0f));

	// setup lightPos
	setUniformVec3(objects[earth].program, "lightPos", glm::vec3(0.0f));
	setUniformVec3(objects[sun].program, "lightPos", glm::vec3(0.0f));

	// setup viewPos
	setUniformVec3(objects[earth].program, "viewPos", glm::vec3(40.0f, 15.0f, 40.0f));
	setUniformVec3(objects[sun].program, "viewPos", glm::vec3(40.0f, 15.0f, 40.0f));
}

int main(int argc, char *argv[])
{
	GLFWwindow* window;
	glfwSetErrorCallback(error_callback);
	if (!glfwInit())
		exit(EXIT_FAILURE);
	// OpenGL 3.3, Mac OS X is reported to have some problem. However I don't have Mac to test
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
	// For Mac OS X
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);	//to make OpenGL forward compatible
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	window = glfwCreateWindow(800, 600, "Solar System", NULL, NULL);	//create a window object
	if (!window)
	{
		glfwTerminate();
		return EXIT_FAILURE;
	}

	// Make our window context the main context on current thread
	glfwMakeContextCurrent(window);

	// This line MUST put below glfwMakeContextCurrent or it will crash
	// Set GL_TRUE so that glew will use modern techniques to manage OpenGL functionality
	glewExperimental = GL_TRUE;
	glewInit();

	// Enable vsync
	glfwSwapInterval(1);

	// Setup input callback
	glfwSetKeyCallback(window, key_callback);
	glfwSetScrollCallback(window, scroll_callback);

	// setup all shader program
	FlatProgram = setup_shader(readfile("vsFlat.txt").c_str(), readfile("fsFlat.txt").c_str());
	GouraudProgram = setup_shader(readfile("vsGouraud.txt").c_str(), readfile("fsGouraud.txt").c_str());
	PhongProgram = setup_shader(readfile("vs.txt").c_str(), readfile("fs.txt").c_str());
	BlinnProgram = setup_shader(readfile("vsBlinn.txt").c_str(), readfile("fsBlinn.txt").c_str());
	ScreenProgram = setup_shader(readfile("vsScreen.txt").c_str(), readfile("fsScreen.txt").c_str());

	// Build obj and return the index in objects array
	sun = add_obj(PhongProgram, "sun.obj","sun.bmp");
	earth = add_obj(PhongProgram, "earth.obj","earth.bmp");

	//glCullFace(GL_BACK);
	// Enable blend mode for billboard
	glEnable(GL_BLEND);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glProvokingVertex(GL_FIRST_VERTEX_CONVENTION);

	// Initialize framebuffers
	frameBuffer_init();

	// change program first in order to give uniforms value
	changeProgram();

	// setup ambient strength
	objects[earth].ambient = glm::vec3(0.5f);
	objects[sun].ambient = glm::vec3(1.0f);

	float last, start;
	last = start = glfwGetTime();
	int fps=0;

	float angle = 5.0f;
	float rev = 5.0f;
	float sunAngle = 5.0f;
	int changeCount = 3;	// the interval to change a shader(in sec)
	while (!glfwWindowShouldClose(window))
	{ //program will keep drawing here until you close the window
		float delta = glfwGetTime() - start;
		glfwGetCursorPos(window, &xpos, &ypos);
		render();
		glfwSwapBuffers(window);	// To swap the color buffer in this game loop
		glfwPollEvents();			// To check if any events are triggered

		// Do the next step for sun rotation, earth rotation and revolution
		if (playing == true) {
			angle = angle + 0.1f;
			sunAngle = sunAngle + 0.003f;
			rev = rev + 0.01f;
			glm::mat4 tl=glm::translate(glm::mat4(),glm::vec3(8.0*sin(rev),3.0*sin(rev),16.0*cos(rev)));
			glm::mat4 rotateM = glm::rotate(glm::mat4(), angle, glm::vec3(0.1f, 1.0f, 0.0f));
			glm::mat4 sunRotate = glm::rotate(glm::mat4(), sunAngle, glm::vec3(0.0f, 1.0f, 0.0f));
			objects[earth].model = tl * rotateM;
			objects[sun].model = sunRotate;
		}

		fps++;
		if(glfwGetTime() - last > 1.0)
		{
			if (playing == true) {
				if (changeCount == 0) {
					changeProgram();	// time to change program!
					changeCount = 3;
				}
				else
					changeCount--;
			}
			std::cout<<(double)fps/(glfwGetTime()-last)<<std::endl;
			fps = 0;
			last = glfwGetTime();
		}
	}

	// End of the program
	releaseObjects();
	glfwDestroyWindow(window);
	glfwTerminate();
	return EXIT_SUCCESS;
}
