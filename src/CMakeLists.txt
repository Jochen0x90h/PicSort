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
conan_target_link_libraries(${PROJECT_NAME})


# install
install(TARGETS ${PROJECT_NAME}
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
	RUNTIME DESTINATION bin
)
