add_executable(${PROJECT_NAME}
	glad/glad.c
	glad/glad.h
	glad/khrplatform.h
	imgui/imgui_impl_opengl3.cpp
	imgui/imgui_impl_opengl3.h
	imgui/Fonts.hpp
	GuiWindow.cpp
	GuiWindow.hpp
	main.cpp
	TinyEXIF.cpp
	TinyEXIF.h
)
target_link_libraries(${PROJECT_NAME}
	PRIVATE
		glfw
		imgui::imgui
		JPEG::JPEG
		libjpeg-turbo::turbojpeg
		#libjpeg-turbo::turbojpeg-static
		tinyxml2::tinyxml2
)


# install
install(TARGETS ${PROJECT_NAME})
