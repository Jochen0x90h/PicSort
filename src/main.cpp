#include "GuiWindow.hpp"
#include "glad/glad.h"
#include "TinyEXIF.h" // https://github.com/cdcseacave/TinyEXIF
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <turbojpeg.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include <filesystem>
#include <ranges>
#include <errno.h>


namespace fs = std::filesystem;


struct ImageData {
	// image size
	int width, height;

	// image orientation, see http://jpegclub.org/exif_orientation.html
	int orientation;

	// image data
	unsigned char *data;
};

/*
const char *subsampName[TJ_NUMSAMP] = {
	"4:4:4", "4:2:2", "4:2:0", "Grayscale", "4:4:0", "4:1:1"
};

const char *colorspaceName[TJ_NUMCS] = {
	"RGB", "YCbCr", "GRAY", "CMYK", "YCCK"
};
*/

class Picture {
public:

	Picture(fs::path path, GuiWindow &window) {
		// file name
		//this->name = path.stem().u8string();

		// get file date
		// https://omegaup.com/docs/cpp/en/cpp/chrono/format.html
		// %F = %Y-%m-%d
		// %R = %H:%M
		// %T = %H:%M:%S
		this->time = fs::last_write_time(path);
		this->date = std::format("{0:%F} {0:%R}", this->time);

		// determine jpeg size
		std::ifstream file(path, std::ios::binary | std::ios::ate);
		int jpegSize = int(file.tellg());
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
		TinyEXIF::EXIFInfo exif(jpegBuf, jpegSize);
		std::stringstream geo;
		if (exif.Fields) {
			// get image orientation
			this->orientation = exif.Orientation;

			// get date
			if (!exif.DateTime.empty()) {
				auto in = std::istringstream(exif.DateTime);
				std::chrono::time_point<std::chrono::file_clock> time;
				in >> std::chrono::parse("%Y:%m:%d %H:%M:%S", time);
				if (time.time_since_epoch().count() != 0) {
					this->time = time;
					this->date = std::format("{0:%F} {0:%R}", time);
				}
			}

			// copy GPS coordinates into clipboard
			if (exif.GeoLocation.hasLatLon()) {
				geo << exif.GeoLocation.Latitude << ", " << exif.GeoLocation.Longitude;
			}
		}
		window.setClipboard(geo.str());

		// init decompressor
		tjhandle tjInstance = NULL;int selectedTarget = -1;
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

	//std::u8string name;
	std::chrono::time_point<std::chrono::file_clock> time;
	std::string date;
	int width, height;

protected:
	char const *action;
	char const *error;
	int orientation = 0;
	unsigned char *imgBuf = NULL;
};


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
		this->vertexCount = int(std::size(vertices));
		this->indexCount = int(std::size(indices));
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

	void set(Size<float> size, ImageData image) {
		float mat[4][4] = {};

		float m00 = 1;
		float m11 = 1;
		if (image.orientation <= 4) {
			// width and height are not exchanged
			if (size.width * image.height > size.height * image.width) {
				m00 = float(size.height * image.width) / float(size.width * image.height);
			} else {
				m11 = float(size.width * image.height) / float(size.height * image.width);
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
			if (size.width * image.width > size.height * image.height) {
				m00 = float(size.height * image.height) / float(size.width * image.width);
			} else {
				m11 = float(size.width * image.width) / float(size.height * image.height);
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

	void draw() {
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


// get sorted file list
std::vector<fs::path> getList(fs::path const &dir) {
	std::vector<fs::path> list;
	//for (auto &entry : boost::make_iterator_range(fs::directory_iterator(dir), {})) {
	for (auto &entry : std::ranges::subrange(fs::directory_iterator(dir), {})) {
		fs::path const &path = entry.path();

		std::error_code ec;
		if (fs::is_directory(path, ec))
			list.push_back(path.filename());
	}
	std::sort(list.begin(), list.end());
	return list;
}


// MainWindow

class MainWindow : public GuiWindow {
public:

	MainWindow(int width, int height, char const *title)
		: GuiWindow(width, height, title)
	{
		fs::path dir = ".";

		// get list of directories in initial target directory
		this->targetDir = fs::canonical(dir);
		std::vector<fs::path> targetList = getList(this->targetDir);

		// read current directory to get list of imge files
		for (auto &entry : std::ranges::subrange(fs::directory_iterator(fs::path(dir)), {})) {
			fs::path const &path = entry.path();

			// collect images
			if (path.extension() == ".jpg" || path.extension() == ".JPG")
				this->files.push_back(path);
		}
		if (this->files.empty()) {
			std::cerr << "No input files";
			return;
		}
		std::sort(this->files.begin(), this->files.end());


		// create picture from first file in list
		if (!this->files.empty())
			this->picture = new Picture(dir / this->files[0], *this);


		// init temp variables
		this->newDirectoryBuffer[0] = 0;
	}

	~MainWindow() override {
		delete this->picture;
	}

	bool empty() {return this->files.empty();}

protected:
	bool onKey(int key, int scancode, int action, int modifiers, bool neededByGui) override {
		if (action == GLFW_PRESS) {
			// esc: exit
			if (key == GLFW_KEY_ESCAPE)
				close();

			// up/down: select next/pevious image
			if (key == GLFW_KEY_UP || key == GLFW_KEY_DOWN) {
				int count = int(this->files.size());
				if (key == GLFW_KEY_UP)
					this->fileIndex = (this->fileIndex + count - 1) % count;
				else
					this->fileIndex = (this->fileIndex + 1) % count;

				// show new picture
				delete this->picture;
				this->picture = new Picture(this->files[this->fileIndex], *this);
			}

			// shift-space: move image
			if (key == GLFW_KEY_SPACE && (modifiers & GLFW_MOD_SHIFT) != 0) {
				fs::path src = this->files[this->fileIndex];
				fs::path dst = this->targetDir / src.filename();
				fs::rename(src, dst);

				// set date
				fs::last_write_time(dst, this->picture->time);

				// erase from list
				this->files.erase(this->files.begin() + this->fileIndex);
				this->fileIndex = std::min(this->fileIndex, int(this->files.size()) - 1);

				// show next picture
				delete this->picture;
				if (!this->files.empty())
					this->picture = new Picture(this->files[this->fileIndex], *this);
				else
					this->picture = nullptr;
			}
		}
		return false;
	}

	void onDraw(State const &state) override {
		// target directory selector
		{
			std::u8string target = this->targetDir.filename().u8string() + u8"###target";
			if (ImGui::Begin((char *)target.c_str(), nullptr, 0)) {
				// input for new directory
				if (ImGui::InputText("New Directory", (char *)this->newDirectoryBuffer, std::size(this->newDirectoryBuffer),
					ImGuiInputTextFlags_EnterReturnsTrue))
				{
					// create and enter new subdirectory
					fs::path newDirectory = this->newDirectoryBuffer;
					fs::create_directory(this->targetDir / newDirectory);
					this->newDirectoryBuffer[0] = 0;
					this->targetDir /= newDirectory;
					this->targetList = getList(this->targetDir);
				}

				// list box containing subdirectories
				std::vector<fs::path> newTargetList;
				bool applyTargetList = false;
				int selectedTarget = -1;
				ImGui::PushItemWidth(-1);
				if (ImGui::BeginListBox("##list", ImVec2(-FLT_MIN, -FLT_MIN))) {
					// parent directory
					if (ImGui::Selectable("..", false)) {
						fs::path currentDirectory = this->targetDir.filename();

						// exit to parent directory
						this->targetDir = this->targetDir.parent_path();
						newTargetList = getList(this->targetDir);
						applyTargetList = true;

						// get index of the directory that we just exited
						for (int i = 0; i < newTargetList.size(); ++i) {
							auto const target = newTargetList[i];
							if (target == currentDirectory) {
								selectedTarget = i;
								break;
							}
						}
					}

					// subdirectories
					for (int i = 0; i < this->targetList.size(); ++i) {
						std::u8string path = this->targetList[i].u8string();
						if (ImGui::Selectable((char *)path.c_str(), false)) {
							// enter subdirectory
							this->targetDir /= this->targetList[i];
							newTargetList = getList(this->targetDir);
							applyTargetList = true;
						}

						// check if we exited a directory and we have to scroll to its location
						if (i == this->selectedTarget) {
							ImGui::SetScrollHereY();
							this->selectedTarget = -1;
						}
					}
					ImGui::EndListBox();
				}
				ImGui::PopItemWidth();

				// apply new list of directories in target directory when a directory was selected by the user
				if (applyTargetList)
					this->targetList.swap(newTargetList);
				this->selectedTarget = selectedTarget;
			}
			ImGui::End();
		}

		// image info
		{
			std::string info = this->picture->date.substr(0, 10) + "###info";
			//std::string info = this->picture->name + "###info";
			if (ImGui::Begin(info.c_str(), nullptr, 0)) {
				// ISO date
				ImGui::LabelText("Date", "%s", this->picture->date.c_str());

				// image size
				std::string size = std::to_string(this->picture->width) + " x " + std::to_string(this->picture->height);
				ImGui::LabelText("Size", "%s", size.c_str());

				// exists in target directory (by file name)?
				bool exists = fs::exists(this->targetDir / this->files[this->fileIndex].filename());
				ImGui::LabelText("Exists", "%s", exists ? "true" : "false");
			}
			ImGui::End();
		}

		ImGui::Render();

		// clear screen
		glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		// render image
		this->image.set(state.framebufferSize, picture->getImage());
		this->image.draw();

		drawGui();
	}


	// source images
	std::vector<fs::path> files;
	int fileIndex = 0;
	Picture* picture = nullptr;

	// target directory and list of directories in target directory
	fs::path targetDir;
	std::vector<fs::path> targetList;
	int selectedTarget = -1;


	// class for rendering a picture onto the screen
	Image image;

	char8_t newDirectoryBuffer[64];
};



int main(int argc, const char **argv) {
	MainWindow window(800, 800, "PicSorter");

	// main loop
	int frameCount = 0;
	auto start = std::chrono::steady_clock::now();
	int count = 5;
	while (!window.isClosed()) {
		auto frameStart = std::chrono::steady_clock::now();

		// process events
		if (count > 0) {
			glfwPollEvents();
			--count;
		} else {
			glfwWaitEvents();
			count = 5;
		}
		//glfwWaitEventsTimeout(0.03);

		// exit if all files sorted
		if (window.empty())
			break;

		window.draw();

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

	return 0;
}
