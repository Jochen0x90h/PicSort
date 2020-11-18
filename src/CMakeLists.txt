add_executable(picsort
	glad/glad.c
	glad/glad.h
	glad/khrplatform.h
	imgui/imgui_impl_glfw.cpp
	imgui/imgui_impl_glfw.h
	imgui/imgui_impl_opengl3.cpp
	imgui/imgui_impl_opengl3.h
	main.cpp
	TinyEXIF.cpp
	TinyEXIF.h
)
conan_target_link_libraries(picsort)


# install
install(TARGETS picsort
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
	RUNTIME DESTINATION bin
)
