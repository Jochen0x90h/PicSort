#pragma once

#include "glad/glad.h"
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <functional>

struct Size {
	int width;
	int height;
};

class GuiWindow {
	static void keyCallback(GLFWwindow* w, int key, int scancode, int action, int modifiers);
	static void charCallback(GLFWwindow* w, unsigned int c);
	static void mouseCallback(GLFWwindow* w, int button, int action, int modifiers);
	static void scrollCallback(GLFWwindow* w, double xoffset, double yoffset);

public:

	/**
	 * Constructor
	 * @param width width of window
	 * @param height height of window
	 * @param title title of window
	 */
	GuiWindow(int width, int height, char const *title, bool visible = true);

	/**
	 * Destructor
	 */
	virtual ~GuiWindow();

	/**
	 * Get scale factor between window and framebuffer coordinates, e.g. 1 on normal and 2 on high-dpi displays
	 * @return scale factor
	 */
	float getScale() {return this->scale;}

	/**
	 * Returns true when when window was closed by the user. Call this from the main loop
	 * @return true when closed by user
	 */
	bool isClosed() {return glfwWindowShouldClose(this->window);}

	/**
	 * Call to indicate that the window should close
	 */
	void close() {glfwSetWindowShouldClose(window, GLFW_TRUE);}

	/**
	 * Get window size
	 * @return
	 */
	Size getSize();

	/**
	 * Call this from the main loop, sets up the context and swaps buffers at the end
	 */
	void draw();

protected:
	/**
	 * Called on key event
	 * @param key key code, one of GLFW_KEY_...
	 * @param scancode keyboard scancode
	 * @param action action, GLFW_PRESS or GLFW_RELEASE
	 * @param modifiers modifiers, GLFW_MOD_SHIFT, GLFW_MOD_CONTROL, GLFW_MOD_ALT or GLFW_MOD_SUPER
	 * @param neededByGui indicates if the gui expects key input
	 * @return true if event was processed and should not be forwarded to the gui
	 */
	virtual bool onKey(int key, int scancode, int action, int modifiers, bool neededByGui);

	/**
	 * Called on character input event
	 * @param c character
	 * @return true if event was processed and should not be forwarded to the gui
	 */
	virtual bool onChar(unsigned int c);

	/**
	 * Called on mouse button event
	 * @param buttons button, GLFW_MOUSE_BUTTON_LEFT, GLFW_MOUSE_BUTTON_RIGHT or GLFW_MOUSE_BUTTON_MIDDLE
	 * @param action action, GLFW_PRESS or GLFW_RELEASE
	 * @param mods modifiers, GLFW_MOD_SHIFT, GLFW_MOD_CONTROL, GLFW_MOD_ALT or GLFW_MOD_SUPER
	 * @return true if event was processed and should not be forwarded to the gui
	 */
	virtual bool onMouse(int button, int action, int modifiers);

	/**
	 * Called on scroll event
	 * @param dx horizontal scroll
	 * @param dy vertical scroll
	 * @return true if event was processed and should not be forwarded to the gui
	 */
	virtual bool onScroll(float dx, float dy);


	struct State {
		// size of window
		Size windowSize;

		// size of framebuffer in pixels (different from window size on high-dpi displays)
		Size framebufferSize;

		// modifiers, GLFW_MOD_SHIFT, GLFW_MOD_CONTROL, GLFW_MOD_ALT or GLFW_MOD_SUPER
		int modifiers;

		// mouse button flags, 1 << GLFW_MOUSE_BUTTON_LEFT, GLFW_MOUSE_BUTTON_RIGHT or GLFW_MOUSE_BUTTON_MIDDLE
		int mouseButtons;

		// position of mouse, nan if undefined e.g. when window not focused
		float mouseX;
		float mouseY;
		bool mouseOverGui;

		// scroll on touchpad or mouse wheel since last onDraw()
		float scrollX;
		float scrollY;

		// time step in seconds since last onDraw()
		float timeStep;
	};

	/**
	 * Do the actual drawing here. Constuct a gui, call ImGui::Render() and then call drawGui() to actually draw it 
	 * @param state state of window, framebuffer, keyboard modifiers and mouse
	 */
	virtual void onDraw(State const &state);

	/**
	 * Draw the gui, do this inside onDraw() after ImGui::Render()
	 */
	void drawGui();

	/**
	 * Selectively draw parts of the gui, do this inside onDraw() after ImGui::Render()
	 */
	void drawGui(std::function<bool (char const *)> filter);

private:
	GLFWwindow *window;

	// scale factor between screen and frame buffer coordinates
	float scale;

	// variables for ImGui
	bool mouseJustPressed[ImGuiMouseButton_COUNT];
	double time;
};