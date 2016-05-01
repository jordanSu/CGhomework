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

struct object_struct{
	unsigned int program;
	unsigned int vao;
	unsigned int vbo[4];
	unsigned int texture;
	glm::mat4 model;
	object_struct(): model(glm::mat4(1.0f)){}
};

std::vector<object_struct> objects;	// VAO: vertex array object,vertex buffer object and texture(color) for objs
unsigned int program, program2;		// Two shader program
//unsigned int lightProgram;			// Shader program for light source
std::vector<int> indicesCount;		//Number of indice of objs

static void error_callback(int error, const char* description)
{
	fputs(description, stderr);
}
static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GL_TRUE);		//close the window when Esc is pressed
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

// Free all objects and memory space
static void releaseObjects()
{
	for(int i=0;i<objects.size();i++){
		glDeleteVertexArrays(1, &objects[i].vao);
		glDeleteTextures(1, &objects[i].texture);
		glDeleteBuffers(4, objects[i].vbo);
	}
	glDeleteProgram(program);
	glDeleteProgram(program2);
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

static void setUniformVec3(unsigned int program, const std::string &name, const glm::vec3 &vector)
{
	// This line can be ignore. But, if you have multiple shader program
	// You must check if currect binding is the one you want
	glUseProgram(program);
	GLint loc=glGetUniformLocation(program, name.c_str());
	if(loc==-1) return;

	glUniform3f(loc, vector.x, vector.y, vector.z);
}

// Draw Object on window
static void render()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	// draw every object
	for(int i=0;i<objects.size();i++){
		glUseProgram(objects[i].program);
		glBindVertexArray(objects[i].vao);
		glBindTexture(GL_TEXTURE_2D, objects[i].texture);
		// I will change the model matrix here
		setUniformMat4(objects[i].program, "model", objects[i].model);
		glDrawElements(GL_TRIANGLES, indicesCount[i], GL_UNSIGNED_INT, nullptr);
	}
	glBindVertexArray(0);
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

	// setup and load shader program
	program = setup_shader(readfile("vs.txt").c_str(), readfile("fs.txt").c_str());
	program2 = setup_shader(readfile("vs.txt").c_str(), readfile("fs.txt").c_str());
	//lightProgram = setup_shader(readfile("vsLight.txt").c_str(), readfile("fsLight.txt").c_str());

	// Build obj and return the index in objects array
	int sun = add_obj(program, "sun.obj","sun.bmp");
	int earth = add_obj(program2, "earth.obj","earth.bmp");

	glEnable(GL_DEPTH_TEST);
	glCullFace(GL_BACK);
	// Enable blend mode for billboard
	glEnable(GL_BLEND);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Setup the MVP matrix for Sun
	setUniformMat4(objects[sun].program, "vp", glm::perspective(glm::radians(32.0f), 800.0f/600, 1.0f, 100.f)*
			glm::lookAt(glm::vec3(40.0f, 15.0f, 40.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f))*glm::mat4(1.0f));
	objects[sun].model = glm::mat4(1.0f);

	// Setup VP matrix for Earth
	setUniformMat4(objects[earth].program, "vp", glm::perspective(glm::radians(24.0f), 800.0f/600, 1.0f, 100.f)*
			glm::lookAt(glm::vec3(40.0f, 15.0f, 40.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f))*glm::mat4(1.0f));
	objects[earth].model = glm::mat4(1.0f);

	// setup lightPos
	setUniformVec3(objects[earth].program, "lightPos", glm::vec3(0.0f));
	setUniformVec3(objects[sun].program, "lightPos", glm::vec3(0.0f));

	// setup viewPos
	setUniformVec3(objects[earth].program, "viewPos", glm::vec3(40.0f, 15.0f, 40.0f));
	setUniformVec3(objects[sun].program, "viewPos", glm::vec3(40.0f, 15.0f, 40.0f));

	// setup ambient strength
	setUniformVec3(objects[earth].program, "ambientLight", glm::vec3(0.5f));
	setUniformVec3(objects[sun].program, "ambientLight", glm::vec3(1.0f));

	float last, start;
	last = start = glfwGetTime();
	int fps=0;
	//objects[sun].model = glm::scale(glm::mat4(1.0f), glm::vec3(0.85f));

	float angle = 5.0f;
	float rev = 5.0f;
	float sunAngle = 5.0f;
	while (!glfwWindowShouldClose(window))
	{ //program will keep drawing here until you close the window
		float delta = glfwGetTime() - start;
		render();
		glfwSwapBuffers(window);	// To swap the color buffer in this game loop
		glfwPollEvents();			// To check if any events are triggered

		// Do the next step for sun rotation, earth rotation and revolution
		angle = angle + 0.1f;
		sunAngle = sunAngle + 0.003f;
		rev = rev + 0.01f;
		glm::mat4 tl=glm::translate(glm::mat4(),glm::vec3(8.0*sin(rev),3.0*sin(rev),16.0*cos(rev)));
		glm::mat4 rotateM = glm::rotate(glm::mat4(), angle, glm::vec3(0.1f, 1.0f, 0.0f));
		glm::mat4 sunRotate = glm::rotate(glm::mat4(), sunAngle, glm::vec3(0.0f, 1.0f, 0.0f));
		objects[earth].model = tl * rotateM;
		objects[sun].model = sunRotate;

		fps++;
		if(glfwGetTime() - last > 1.0)
		{
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
