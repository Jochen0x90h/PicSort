#include "GuiWindow.hpp"
#include "imgui/imgui_impl_opengl3.h"
#include "imgui/Fonts.hpp"
//#include <cstdio>
//#include <cstdlib>
#include <mutex>


// global variables
static std::mutex g_mutex;
static int g_count;
static GLFWcursor* g_MouseCursors[ImGuiMouseCursor_COUNT] = {};


// static functions

static void errorCallback(int error, const char* description) {
	fprintf(stderr, "Error: %s\n", description);
}

static const char *getClipboardText(void *user_data) {
	return glfwGetClipboardString((GLFWwindow *)user_data);
}

static void setClipboardText(void *user_data, const char* text) {
	glfwSetClipboardString((GLFWwindow *)user_data, text);
}

void GuiWindow::keyCallback(GLFWwindow* w, int key, int scancode, int action, int modifiers) {
	GuiWindow *window = (GuiWindow *)glfwGetWindowUserPointer(w);
	ImGuiIO &io = ImGui::GetIO();

	// call user code
	if (!window->onKey(key, scancode, action, modifiers, io.WantCaptureKeyboard)) {
		// handle ImGui
		if (action == GLFW_PRESS)
			io.KeysDown[key] = true;
		if (action == GLFW_RELEASE)
			io.KeysDown[key] = false;

		// modifiers are not reliable across systems
		io.KeyShift = io.KeysDown[GLFW_KEY_LEFT_SHIFT] || io.KeysDown[GLFW_KEY_RIGHT_SHIFT];
		io.KeyCtrl = io.KeysDown[GLFW_KEY_LEFT_CONTROL] || io.KeysDown[GLFW_KEY_RIGHT_CONTROL];
		io.KeyAlt = io.KeysDown[GLFW_KEY_LEFT_ALT] || io.KeysDown[GLFW_KEY_RIGHT_ALT];
		io.KeySuper = io.KeysDown[GLFW_KEY_LEFT_SUPER] || io.KeysDown[GLFW_KEY_RIGHT_SUPER];
	}
}

void GuiWindow::charCallback(GLFWwindow* w, unsigned int c) {
	GuiWindow *window = (GuiWindow *)glfwGetWindowUserPointer(w);

	// call user code
	if (!window->onChar(c)) {
		// handle ImGui
		ImGuiIO &io = ImGui::GetIO();
		io.AddInputCharacter(c);
	}
}

void GuiWindow::mouseCallback(GLFWwindow* w, int button, int action, int mods) {
	GuiWindow *window = (GuiWindow *)glfwGetWindowUserPointer(w);

	// call user code
	if (!window->onMouse(button, action, mods)) {
		// handle ImGui
		if (action == GLFW_PRESS && button >= 0 && button < std::size(window->mouseJustPressed))
			window->mouseJustPressed[button] = true;
	}
}

void GuiWindow::scrollCallback(GLFWwindow* w, double xoffset, double yoffset) {
	GuiWindow *window = (GuiWindow *)glfwGetWindowUserPointer(w);

	// call user code
	if (!window->onScroll(float(xoffset), float(yoffset))) {
		// handle ImGui
		ImGuiIO &io = ImGui::GetIO();
		io.MouseWheelH += float(xoffset);
		io.MouseWheel += float(yoffset);
	}
}

// GuiWindow

GuiWindow::GuiWindow(int width, int height, char const *title, bool visible)
	: mouseJustPressed{}
{
	std::lock_guard<std::mutex> lock(g_mutex);

#ifdef __linux__
	// assume high-dpi display until support is added to glfw
	width *= 2;
	height *= 2;
#endif
	// global init for GLFW
	if (g_count == 0) {
		if (!glfwInit())
			exit(EXIT_FAILURE);

		// Create mouse cursors
		// (By design, on X11 cursors are user configurable and some cursors may be missing. When a cursor doesn't exist,
		// GLFW will emit an error which will often be printed by the app, so we temporarily disable error reporting.
		// Missing cursors will return NULL and our _UpdateMouseCursor() function will use the Arrow cursor instead.)
		g_MouseCursors[ImGuiMouseCursor_Arrow] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
		g_MouseCursors[ImGuiMouseCursor_TextInput] = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
		g_MouseCursors[ImGuiMouseCursor_ResizeNS] = glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR);
		g_MouseCursors[ImGuiMouseCursor_ResizeEW] = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
		g_MouseCursors[ImGuiMouseCursor_Hand] = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
#if GLFW_HAS_NEW_CURSORS
		g_MouseCursors[ImGuiMouseCursor_ResizeAll] = glfwCreateStandardCursor(GLFW_RESIZE_ALL_CURSOR);
		g_MouseCursors[ImGuiMouseCursor_ResizeNESW] = glfwCreateStandardCursor(GLFW_RESIZE_NESW_CURSOR);
		g_MouseCursors[ImGuiMouseCursor_ResizeNWSE] = glfwCreateStandardCursor(GLFW_RESIZE_NWSE_CURSOR);
		g_MouseCursors[ImGuiMouseCursor_NotAllowed] = glfwCreateStandardCursor(GLFW_NOT_ALLOWED_CURSOR);
#else
		g_MouseCursors[ImGuiMouseCursor_ResizeAll] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
		g_MouseCursors[ImGuiMouseCursor_ResizeNESW] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
		g_MouseCursors[ImGuiMouseCursor_ResizeNWSE] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
		g_MouseCursors[ImGuiMouseCursor_NotAllowed] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
#endif

		glfwSetErrorCallback(errorCallback);
	}

	// create GLFW window and OpenGL 3.3 Core context
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	//glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GL_TRUE);
	glfwWindowHint(GLFW_VISIBLE, visible);
	this->window = glfwCreateWindow(width, height, title, NULL, NULL);
	if (!this->window) {
		glfwTerminate();
		exit(EXIT_FAILURE);
	}
	glfwSetWindowUserPointer(this->window, this);

	// set callbacks
	glfwSetKeyCallback(this->window, keyCallback);
	glfwSetCharCallback(this->window, charCallback);
	glfwSetMouseButtonCallback(this->window, mouseCallback);
	glfwSetScrollCallback(this->window, scrollCallback);

	// make OpenGL context current
	glfwMakeContextCurrent(this->window);

	// load OpenGL functions
	if (g_count == 0)
		gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

	// v-sync
	glfwSwapInterval(1);

	// determine scale between window and framebuffer coordinates which is > 1 on high-dpi displays
#ifdef __linux__
	this->scale = 2;
#else
	{
		// get window size in screen coordinates, scaled on high-dpi (retina) displays
		int wWidth, wHeight;
		glfwGetWindowSize(this->window, &wWidth, &wHeight);

		// get frame buffer size in pixels, real resolution on high-dpi displays
		int fbWidth, fbHeight;
		glfwGetFramebufferSize(this->window, &fbWidth, &fbHeight);

		this->scale = float(fbWidth) / wWidth;
	}
#endif

	// global init for ImGui, requires OpenGL context
	if (g_count == 0) {
		// setup ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();

		// setup font, has to be done before ImGui_ImplOpenGL3_Init()
		const float fontSize = 14 * this->scale;
		ImFontConfig fontConfig;
		fontConfig.FontDataOwnedByAtlas = false;
		io.Fonts->AddFontFromMemoryTTF(FONT_ROBOTO_REGULAR, sizeof(FONT_ROBOTO_REGULAR), fontSize, &fontConfig);
		//io.Fonts->AddFontFromFileTTF("Roboto-Regular.ttf", 26);

		// setup OpenGL backend, needs ImGui context
		ImGui_ImplOpenGL3_Init();

		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

		// setup ImGui style
		ImGui::StyleColorsDark();
		//ImGui::StyleColorsClassic();

		// setup platform/renderer backends
		//ImGui_ImplGlfw_InitForOpenGL(this->window, true);
		io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;         // We can honor GetMouseCursor() values (optional)
		io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;          // We can honor io.WantSetMousePos requests (optional, rarely used)
		io.BackendPlatformName = "GLFW+OpenGL3.3-Core";

		// Keyboard mapping. ImGui will use those indices to peek into the io.KeysDown[] array.
		io.KeyMap[ImGuiKey_Tab] = GLFW_KEY_TAB;
		io.KeyMap[ImGuiKey_LeftArrow] = GLFW_KEY_LEFT;
		io.KeyMap[ImGuiKey_RightArrow] = GLFW_KEY_RIGHT;
		io.KeyMap[ImGuiKey_UpArrow] = GLFW_KEY_UP;
		io.KeyMap[ImGuiKey_DownArrow] = GLFW_KEY_DOWN;
		io.KeyMap[ImGuiKey_PageUp] = GLFW_KEY_PAGE_UP;
		io.KeyMap[ImGuiKey_PageDown] = GLFW_KEY_PAGE_DOWN;
		io.KeyMap[ImGuiKey_Home] = GLFW_KEY_HOME;
		io.KeyMap[ImGuiKey_End] = GLFW_KEY_END;
		io.KeyMap[ImGuiKey_Insert] = GLFW_KEY_INSERT;
		io.KeyMap[ImGuiKey_Delete] = GLFW_KEY_DELETE;
		io.KeyMap[ImGuiKey_Backspace] = GLFW_KEY_BACKSPACE;
		io.KeyMap[ImGuiKey_Space] = GLFW_KEY_SPACE;
		io.KeyMap[ImGuiKey_Enter] = GLFW_KEY_ENTER;
		io.KeyMap[ImGuiKey_Escape] = GLFW_KEY_ESCAPE;
		io.KeyMap[ImGuiKey_KeyPadEnter] = GLFW_KEY_KP_ENTER;
		io.KeyMap[ImGuiKey_A] = GLFW_KEY_A;
		io.KeyMap[ImGuiKey_C] = GLFW_KEY_C;
		io.KeyMap[ImGuiKey_V] = GLFW_KEY_V;
		io.KeyMap[ImGuiKey_X] = GLFW_KEY_X;
		io.KeyMap[ImGuiKey_Y] = GLFW_KEY_Y;
		io.KeyMap[ImGuiKey_Z] = GLFW_KEY_Z;

		io.SetClipboardTextFn = setClipboardText;
		io.GetClipboardTextFn = getClipboardText;
		io.ClipboardUserData = this->window;
	}

	this->time = glfwGetTime();

	++g_count;
}

GuiWindow::~GuiWindow() {
	std::lock_guard<std::mutex> lock(g_mutex);
	--g_count;

	// global terminate for ImGui, requires OpenGL contex
	if (g_count == 0) {
		ImGui_ImplOpenGL3_Shutdown();
		ImGui::DestroyContext();
	}

	glfwDestroyWindow(window);

	// global terminate for GLFW
	if (g_count == 0) {
		glfwTerminate();
	}
}

GuiWindow::Size GuiWindow::getSize() {
	int width, height;
	glfwGetWindowSize(this->window, &width, &height);
#ifdef __linux__
	width /= 2;
	height /= 2;
#endif
	return {width, height};
}

void GuiWindow::draw() {
	// make OpenGL context current
	glfwMakeContextCurrent(this->window);

	// get window size in screen coordinates, scaled on high-dpi (retina) displays
	int width, height;
	glfwGetWindowSize(this->window, &width, &height);

	// get frame buffer size in pixels, real resolution on high-dpi displays
	int fbWidth, fbHeight;
	glfwGetFramebufferSize(this->window, &fbWidth, &fbHeight);

	// set viewport
	glViewport(0, 0, fbWidth, fbHeight);


	ImGuiIO& io = ImGui::GetIO();
	IM_ASSERT(io.Fonts->IsBuilt() && "Font atlas not built! It is generally built by the renderer back-end. Missing call to renderer _NewFrame() function? e.g. ImGui_ImplOpenGL3_NewFrame().");

	// Setup display size (every frame to accommodate for window resizing)
	int w, h;
	int display_w, display_h;
	glfwGetWindowSize(this->window, &w, &h);
	glfwGetFramebufferSize(this->window, &display_w, &display_h);
	io.DisplaySize = ImVec2(float(width), float(height));
	if (width > 0 && height > 0) {
		io.DisplayFramebufferScale = ImVec2(float(fbWidth) / width, float(fbHeight) / height);

		// counteract upscaling of font on high-dpi displays
		io.FontGlobalScale = float(width) / fbWidth;
	}

	// Setup time step
	double currentTime = glfwGetTime();
	io.DeltaTime = float(currentTime - this->time);
	this->time = currentTime;

	// Update buttons
	int buttons = 0;
	for (int i = 0; i < std::size(io.MouseDown); i++) {
		// If a mouse press event came, always pass it as "mouse held this frame", so we don't miss click-release events that are shorter than 1 frame.
		bool down = this->mouseJustPressed[i] || glfwGetMouseButton(this->window, i) != 0;
		io.MouseDown[i] = down;
		this->mouseJustPressed[i] = false;
		if (down)
			buttons |= 1 << i;
	}

	// Update mouse position
	const ImVec2 mouse_pos_backup = io.MousePos;
	io.MousePos = ImVec2(std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN());
#ifdef __EMSCRIPTEN__
	const bool focused = true; // Emscripten
#else
	const bool focused = glfwGetWindowAttrib(this->window, GLFW_FOCUSED) != 0;
#endif
	if (focused) {
		if (io.WantSetMousePos) {
			glfwSetCursorPos(this->window, (double)mouse_pos_backup.x, (double)mouse_pos_backup.y);
		} else {
			double mouse_x, mouse_y;
			glfwGetCursorPos(this->window, &mouse_x, &mouse_y);
			io.MousePos = ImVec2((float)mouse_x, (float)mouse_y);
		}
	}

	// Update mouse cursor
	if (!(io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) && glfwGetInputMode(this->window, GLFW_CURSOR) != GLFW_CURSOR_DISABLED) {
		ImGuiMouseCursor imgui_cursor = ImGui::GetMouseCursor();
		if (imgui_cursor == ImGuiMouseCursor_None || io.MouseDrawCursor) {
			// Hide OS mouse cursor if imgui is drawing it or if it wants no cursor
			glfwSetInputMode(this->window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
		} else {
			// Show OS mouse cursor
			// FIXME-PLATFORM: Unfocused windows seems to fail changing the mouse cursor with GLFW 3.2, but 3.3 works here.
			glfwSetCursor(this->window, g_MouseCursors[imgui_cursor] ? g_MouseCursors[imgui_cursor]
				: g_MouseCursors[ImGuiMouseCursor_Arrow]);
			glfwSetInputMode(this->window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		}
	}

	// draw
	State state;
	state.framebufferWidth = fbWidth;
	state.framebufferHeight = fbHeight;
	state.modifiers = 0;
	if (io.KeyCtrl)
		state.modifiers |= GLFW_MOD_CONTROL;
	if (io.KeyShift)
		state.modifiers |= GLFW_MOD_SHIFT;
	if (io.KeyAlt)
		state.modifiers |= GLFW_MOD_ALT;
	if (io.KeySuper)
		state.modifiers |= GLFW_MOD_SUPER;
	state.mouseButtons = buttons;
	state.mouseX = io.MousePos.x;
	state.mouseY = io.MousePos.y;
	state.mouseOverGui = io.WantCaptureMouse;
	state.scrollX = io.MouseWheelH;
	state.scrollY = io.MouseWheel;
	state.timeStep = io.DeltaTime;

	ImGui::NewFrame();
	this->rendered = false;
	onDraw(state);
	if (!this->rendered)
		ImGui::Render();
	//ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	// swap render buffer to screen
	glfwSwapBuffers(window);
}

void GuiWindow::drawGui() {
	if (!this->rendered) {
		ImGui::Render();
		this->rendered = true;
	}
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData(), [](char const *){return true;});
}

void GuiWindow::drawGui(std::function<bool (char const *)> filter) {
	if (!this->rendered) {
		ImGui::Render();
		this->rendered = true;
	}
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData(), filter);
}

bool GuiWindow::onKey(int key, int scancode, int action, int mods, bool neededByGui) {
	return false;
}

bool GuiWindow::onChar(unsigned int c) {
	return false;
}

bool GuiWindow::onMouse(int button, int action, int mods) {
	return false;
}

bool GuiWindow::onScroll(float dx, float dy) {
	return false;
}

void GuiWindow::onDraw(State const &state) {
}
