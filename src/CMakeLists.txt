# picsort: GUI tool for sorting pictures
add_executable(picsort
	glad/glad.c
	glad/glad.h
	glad/khrplatform.h
	imgui/imgui_impl_opengl3.cpp
	imgui/imgui_impl_opengl3.h
	imgui/Fonts.hpp
	GuiWindow.cpp
	GuiWindow.hpp
	picsort.cpp
	TinyEXIF.cpp
	TinyEXIF.h
)
target_link_libraries(picsort
	PRIVATE
		glfw
		imgui::imgui
		JPEG::JPEG
		#libjpeg-turbo::turbojpeg
		libjpeg-turbo::turbojpeg-static
		tinyxml2::tinyxml2
)

# picfix: Command line tool for fixing file data from exif data
add_executable(picfix
	picfix.cpp
	TinyEXIF.cpp
	TinyEXIF.h
)
target_link_libraries(picfix
	PRIVATE
		tinyxml2::tinyxml2
)


# install
install(TARGETS picsort picfix)
