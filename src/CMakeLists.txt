add_executable(picsort
	glad/glad.c
	glad/glad.h
	glad/khrplatform.h
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
