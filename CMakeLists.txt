cmake_minimum_required(VERSION 3.9)

# set project
project(picsort)

#set(CMAKE_VERBOSE_MAKEFILE ON)

# use C++20 standard by default
set(CMAKE_CXX_STANDARD 20)


# enable address sanitizer
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")

# dependencies
find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(JPEG REQUIRED)
find_package(libjpeg-turbo CONFIG REQUIRED)
find_package(tinyxml2 CONFIG REQUIRED)


add_subdirectory(src)


# enable testing, adds test or RUN_TESTS target to run all tests
#enable_testing()
#add_subdirectory(test)
