#include "glad/glad.h"
#include <GLFW/glfw3.h>
#include <errno.h>
#include <turbojpeg.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <boost/filesystem.hpp>
#include <boost/range/iterator_range.hpp>
#include "TinyEXIF.h" // https://github.com/cdcseacave/TinyEXIF
#include <imgui.h>
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"


namespace fs = boost::filesystem;


struct ImageData {
	// image size
	int width, height;

	// image orientation, see http://jpegclub.org/exif_orientation.html
	int orientation;

	// image data
	unsigned char *data;
};


const char *subsampName[TJ_NUMSAMP] = {
	"4:4:4", "4:2:2", "4:2:0", "Grayscale", "4:4:0", "4:1:1"
};

const char *colorspaceName[TJ_NUMCS] = {
	"RGB", "YCbCr", "GRAY", "CMYK", "YCCK"
};

class Picture {
public:

	Picture(fs::path path) {
		// determine jpeg size
		std::ifstream file(path.string(), std::ios::binary | std::ios::ate);
		int jpegSize = file.tellg();
		file.seekg(0);

		// allocate jpeg buffer
		unsigned char *jpegBuf = NULL;
		if ((jpegBuf = (unsigned char *)tjAlloc(jpegSize)) == NULL) {
			setError("allocating JPEG buffer");
			return;
		}

		// read jpeg into buffer
		file.read(reinterpret_cast<char *>(jpegBuf), jpegSize);
		file.close();

		// read exif
		TinyEXIF::EXIFInfo imageEXIF(jpegBuf, jpegSize);
		if (imageEXIF.Fields) {
			this->orientation = imageEXIF.Orientation;
		}

		// init decompressor
		tjhandle tjInstance = NULL;
		if ((tjInstance = tjInitDecompress()) == NULL) {
			setError("initializing decompressor", tjInstance);
			return;
		}
		
		// decompress header
		int inSubsamp, inColorspace;
		if (tjDecompressHeader3(tjInstance, jpegBuf, jpegSize, &this->width, &this->height, &inSubsamp, &inColorspace) < 0) {
			setError("reading JPEG header", tjInstance);
			return;
		}

		// allocate image
		int pixelFormat = TJPF_RGB;
		if ((this->imgBuf = (unsigned char *)tjAlloc(width * height * tjPixelSize[pixelFormat])) == NULL) {
			setError("allocating uncompressed image buffer");
			return;
		}

		// decompress image
		int flags = TJFLAG_FASTDCT | TJFLAG_FASTUPSAMPLE;
		if (tjDecompress2(tjInstance, jpegBuf, jpegSize, this->imgBuf, this->width, 0, this->height,
			pixelFormat, flags) < 0)
		{
			setError("decompressing JPEG image", tjInstance);
		}
		
		// free
		tjFree(jpegBuf);
		tjDestroy(tjInstance);
	}
	
	~Picture() {
		tjFree(this->imgBuf);
	}
	
	void setError(char const *action) {
		this->action = action;
		this->error = strerror(errno);
	}
	
	void setError(char const *action, tjhandle tjInstance) {
		this->action = action;
		this->error = tjGetErrorStr2(tjInstance);
	}

	// get image data
	ImageData getImage() {return {this->width, this->height, this->orientation, this->imgBuf};}

protected:
	char const *action;
	char const *error;
	int width, height;
	int orientation = 0;
	unsigned char *imgBuf = NULL;
};



std::vector<fs::path> files;
int fileIndex = 0;
int fileCount;
Picture* picture;

static void errorCallback(int error, const char* description) {
	fprintf(stderr, "Error: %s\n", description);
}

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if (action == GLFW_PRESS) {
		if (key == GLFW_KEY_ESCAPE)
			glfwSetWindowShouldClose(window, GLFW_TRUE);

		if (key == GLFW_KEY_UP || key == GLFW_KEY_DOWN) {
			if (key == GLFW_KEY_UP)
				fileIndex = std::max(fileIndex - 1, 0);
			else
				fileIndex = std::min(fileIndex + 1, fileCount - 1);

			delete picture;
			picture = new Picture(files[fileIndex]);
		}
	}
}

static void mouseCallback(GLFWwindow* window, int button, int action, int mods) {
    /*   if (button == GLFW_MOUSE_BUTTON_LEFT) {
   		if(GLFW_PRESS == action)
   			lbutton_down = true;
   		else if(GLFW_RELEASE == action)
   			lbutton_down = false;
   	}

   	if(lbutton_down) {
   		 // do your drag here
   	}*/
}



namespace shader {

GLuint compile(std::string const &name, GLenum type, std::string const &source) {
	unsigned int shader = glCreateShader(type);
	const char * src = source.data();
	glShaderSource(shader, 1, &src, nullptr);
	glCompileShader(shader);
	int isCompiled;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
	if (GL_FALSE == isCompiled) {
		int error = glGetError();
		int length;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
		std::string message(length, '\0');
		glGetShaderInfoLog(shader, length, nullptr, &message.front());
		throw std::runtime_error(
			"Failed to compile shader '" + name + "': " + message + " glError:" + std::to_string(error));
	}
	return shader;
}

GLuint create(std::string const &name, std::string const &vertex, std::string const &fragment) {
	// create shaders
	GLuint vs = compile(name + ".vertex", GL_VERTEX_SHADER, vertex);
	GLuint fs = compile(name + ".fragment", GL_FRAGMENT_SHADER, fragment);

	// create program
	GLuint program = glCreateProgram();
	glAttachShader(program, vs);
	glAttachShader(program, fs);
	glLinkProgram(program);
	glValidateProgram(program);

	// delete shaders (program still holds a reference to them)
	glDeleteShader(vs);
	glDeleteShader(fs);

	return program;
}

GLint getUniform(std::string const &programName, GLuint program, std::string const &uniformName) {
	GLint location = glGetUniformLocation(program, uniformName.c_str());
	if (location == -1) {
		throw std::runtime_error(
			"Uniform '" + uniformName + "' does not exist in program '" + programName + "'");
	}
	return location;
}

GLint getVertexInput(std::string const &programName, GLuint program, std::string const &inputName) {
	GLint location = glGetAttribLocation(program, inputName.c_str());
	if (location == -1) {
		throw std::runtime_error(
			"Vertex input '" + inputName + "' does not exist in program '" + programName + "'");
	}
	return location;
}

}




struct Vertex {
    float x;
    float y;
    float z;
};

struct Texcoord {
    float u;
    float v;
};


class Image {
public:

	Image() {
		// create shader
		std::string shaderName = "Map";
		this->shader = shader::create(shaderName, vertexShaderCode, fragmentShaderCode);
		this->matUniform = shader::getUniform(shaderName, shader, "mat");
		this->mapUniform = shader::getUniform(shaderName, this->shader, "map");
		this->vertexInput = shader::getVertexInput(shaderName, this->shader, "vertex");
		this->texcoordInput = shader::getVertexInput(shaderName, this->shader, "texcoord");

		// set texture index, not necessary because 0 is default
		//glUseProgram(this->shader);
		//glUniform1i(this->mapUniform, 0);
		//glUseProgram(0);

		// load texture
		glGenTextures(1, &this->texture);
		glBindTexture(GL_TEXTURE_2D, this->texture);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glBindTexture(GL_TEXTURE_2D, 0);

		// create vertex buffers
		this->vertexCount = std::size(vertices);
		this->indexCount = std::size(indices);
		glGenBuffers(1, &this->vertexBuffer);
		glGenBuffers(1, &this->texcoordBuffer);
		glGenBuffers(1, &this->indexBuffer);

		// create vertex array object
		glGenVertexArrays(1, &this->vao);
		glBindVertexArray(this->vao);

		glBindBuffer(GL_ARRAY_BUFFER, this->vertexBuffer);
		glBufferData(GL_ARRAY_BUFFER, this->vertexCount * sizeof(Vertex), this->vertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(this->vertexInput);
		glVertexAttribPointer(this->vertexInput, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);

		glBindBuffer(GL_ARRAY_BUFFER, this->texcoordBuffer);
		glBufferData(GL_ARRAY_BUFFER, this->vertexCount * sizeof(Texcoord), this->texcoords, GL_STATIC_DRAW);
		glEnableVertexAttribArray(this->texcoordInput);
		glVertexAttribPointer(this->texcoordInput, 2, GL_FLOAT, GL_FALSE, sizeof(Texcoord), nullptr);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->indexBuffer);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, this->indexCount * sizeof(uint32_t), this->indices, GL_STATIC_DRAW);

		glBindVertexArray(0);
	}

	void set(int width, int height, ImageData image) {
		float mat[4][4] = {};
		
		float m00 = 1;
		float m11 = 1;
		if (image.orientation <= 4) {
			// width and height are not exchanged
			if (width * image.height > height * image.width) {
				m00 = float(height * image.width) / float(width * image.height);
			} else {
				m11 = float(width * image.height) / float(height * image.width);
			}
	
			//   1       2       3       4
			// 888888  888888      88  88
			// 88          88      88  88
			// 8888      8888    8888  8888
			// 88          88      88  88
			// 88          88  888888  888888
			switch (image.orientation) {
			case 2:
				mat[0][0] = -m00;
				mat[1][1] = m11;
				break;
			case 3:
				mat[0][0] = -m00;
				mat[1][1] = -m11;
				break;
			case 4:
				mat[0][0] = m00;
				mat[1][1] = -m11;
				break;
			default:
				mat[0][0] = m00;
				mat[1][1] = m11;
			}

		} else {
			// width and height are exchanged
			if (width * image.width > height * image.height) {
				m00 = float(height * image.height) / float(width * image.width);
			} else {
				m11 = float(width * image.width) / float(height * image.height);
			}

			//     5           6           7           8
			// 8888888888  88                  88  8888888888
			// 88  88      88  88          88  88      88  88
			// 88          8888888888  8888888888          88
			switch (image.orientation) {
			case 5:
				mat[1][0] = -m00;
				mat[0][1] = -m11;
				break;
			case 6:
				mat[1][0] = m00;
				mat[0][1] = -m11;
				break;
			case 7:
				mat[1][0] = -m00;
				mat[0][1] = m11;
				break;
			default:
				mat[1][0] = m00;
				mat[0][1] = m11;
			}
		}
		mat[2][2] = 1;
		mat[3][3] = 1;

		// set matrix
		glUseProgram(this->shader);
		glUniformMatrix4fv(this->matUniform, 1, false, mat[0]);
		glUseProgram(0);

		// set texture data
		glBindTexture(GL_TEXTURE_2D, this->texture);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexImage2D(GL_TEXTURE_2D, 0,  GL_RGB8, image.width, image.height, 0, GL_RGB, GL_UNSIGNED_BYTE, image.data);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	void render() {
		// set program
		glUseProgram(this->shader);

		glBindTexture(GL_TEXTURE_2D, this->texture);

		// set vertex array object
		glBindVertexArray(this->vao);

		// draw
		//glDrawArrays(GL_TRIANGLES, 0, vertexCount);
		glDrawElements(GL_TRIANGLES, this->indexCount, GL_UNSIGNED_INT, nullptr);

		// reset
		glBindVertexArray(0);
		glBindTexture(GL_TEXTURE_2D, 0);
		glUseProgram(0);
	}

	GLuint shader;
	GLint matUniform;
	GLint mapUniform;
	GLint vertexInput;
	GLint texcoordInput;

	GLuint texture;

	int vertexCount;
	int indexCount;
	GLuint vertexBuffer;
	GLuint texcoordBuffer;
	GLuint indexBuffer;

	GLuint vao;

	static char const *vertexShaderCode;
	static char const *fragmentShaderCode;
	static Vertex const vertices[4];
	static Texcoord const texcoords[4];
	static uint32_t const indices[6];
};

char const *Image::vertexShaderCode = R"SHADER(#version 330
uniform mat4 mat;
in vec4 vertex;
in vec2 texcoord;
out vec2 uv;
void main() {
    gl_Position = mat * vertex;
    uv = texcoord;
})SHADER";

char const *Image::fragmentShaderCode = R"SHADER(#version 330
uniform sampler2D map;
in vec2 uv;
out vec4 pixel;
void main() {
    pixel = texture(map, uv);
})SHADER";



Vertex const Image::vertices[4] = {
    {-1, -1, 0},
    { 1, -1, 0},
    {-1,  1, 0},
    { 1,  1, 0}
};

Texcoord const Image::texcoords[4] = {
    {0, 1},
    {1, 1},
    {0, 0},
    {1, 0}
};

uint32_t const Image::indices[6] = {
    0, 1, 2,
    3, 2, 1
};


std::vector<fs::path> getList(fs::path const &dir) {
	std::vector<fs::path> list;
	for (auto &entry : boost::make_iterator_range(fs::directory_iterator(dir), {})) {
		fs::path const &path = entry.path();

		boost::system::error_code ec;
		if (fs::is_directory(path, ec))
			list.push_back(path.filename());
	}
	std::sort(list.begin(), list.end());
	return list;
}


int main(int argc, const char **argv) {
	fs::path dir = ".";
	fs::path targetDir = fs::canonical(dir);
	std::vector<fs::path> targetList = getList(targetDir);
	
	// read current directory
	for (auto &entry : boost::make_iterator_range(fs::directory_iterator(fs::path(dir)), {})) {
		//std::cout << entry << "\n";

		fs::path const &path = entry.path();
		
		// collect images
		if (path.extension() == ".jpg" || path.extension() == ".JPG")
			files.push_back(path);
	}
	std::sort(files.begin(), files.end());
	fileCount = files.size();
	if (fileCount == 0) {
		std::cerr << "No input files";
		return 0;
	}

	// init GLFW
	glfwSetErrorCallback(errorCallback);
	if (!glfwInit())
	exit(EXIT_FAILURE);

	// create GLFW window and OpenGL 3.3 Core context
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	//glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GL_TRUE);
	GLFWwindow *window = glfwCreateWindow(800, 800, "PicSorter", NULL, NULL);
	if (!window) {
		glfwTerminate();
		exit(EXIT_FAILURE);
	}
	glfwSetKeyCallback(window, keyCallback);
	glfwSetMouseButtonCallback(window, mouseCallback);

	// make OpenGL context current
	glfwMakeContextCurrent(window);

	// load OpenGL functions
	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

	// v-sync
	glfwSwapInterval(1);


	// Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init();


	// create picture from first file in list
	picture = new Picture(dir / files[0]);
	

	// class for rendering a picture onto the screen
	Image image;


	// main loop
	int frameCount = 0;
	auto start = std::chrono::steady_clock::now();
	while (!glfwWindowShouldClose(window)) {
		auto frameStart = std::chrono::steady_clock::now();

		// process events
		glfwPollEvents();
		//glfwWaitEvents();
		//glfwWaitEventsTimeout(0.03);
		
		// exit if all files sorted
		if (fileCount == 0)
			break;

		// Start the Dear ImGui frame
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

		// target directory selector
		{
			std::vector<fs::path> newTargetList;
			bool selected = false;
			std::string current = targetDir.filename().string() + "###target";
            if (ImGui::Begin(current.c_str(), nullptr, 0)) {
				// list box containing subdirectories
				ImGui::PushItemWidth(-1);
				if (ImGui::ListBoxHeader("", targetList.size(), 10)) {
					// parent directory
					if (ImGui::Selectable("..", false)) {
						targetDir = targetDir.parent_path();
						newTargetList = getList(targetDir);
						selected = true;
					}
					
					// subdirectories
					for (int i = 0; i < targetList.size(); ++i) {
						if (ImGui::Selectable(targetList[i].c_str(), false)) {
							targetDir /= targetList[i];
							newTargetList = getList(targetDir);
							selected = true;
						}
					}
					ImGui::ListBoxFooter();
				}
				ImGui::PopItemWidth();
			}
			ImGui::End();

			if (selected)
				targetList.swap(newTargetList);
        }

		// set viewport
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);
		glViewport(0, 0, width, height);

		// clear screen
		glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		// render image
		image.set(width, height, picture->getImage());
		image.render();

		// render gui
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		// swap render buffer to screen
		glfwSwapBuffers(window);


		// show frames per second
		auto now = std::chrono::steady_clock::now();
		++frameCount;
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
		if (duration.count() > 1000) {
			//std::cout << frameCount * 1000 / duration.count() << "fps" << std::endl;
			frameCount = 0;
			start = std::chrono::steady_clock::now();
		}

	}

	// cleanup
	ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
