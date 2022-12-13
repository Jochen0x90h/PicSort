cmake_minimum_required(VERSION 3.19)

# set project
project(picsort)

#set(CMAKE_VERBOSE_MAKEFILE ON)

# configure languages
enable_language(CXX)
set(CMAKE_CXX_STANDARD 17)

# enable address sanitizer
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")


# dependencies
find_package(Boost CONFIG)
find_package(glfw3 CONFIG)
find_package(imgui CONFIG)
find_package(libjpeg-turbo CONFIG)
find_package(tinyxml2 CONFIG)


add_subdirectory(src)


# enable testing, adds test or RUN_TESTS target to run all tests
#enable_testing()
#add_subdirectory(test)
