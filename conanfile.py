from conan import ConanFile
from conan.tools.files import copy
from conan.tools.cmake import CMake


class Project(ConanFile):
    name = "PicSort"
    description = "Tool for sorting pictures"
    url = "https://github.com/Jochen0x90h/PicSort"
    license = "MIT"
    settings = "os", "compiler", "build_type", "arch"
    default_options = {}
    generators = "CMakeDeps", "CMakeToolchain"
    exports_sources = "conanfile.py", "CMakeLists.txt", "src/*", "test/*"
    requires = [
        "glfw/3.4",
        "imgui/1.92.0",
        "libjpeg-turbo/3.1.1",
        "tinyxml2/11.0.0"
    ]


    keep_imports = True
    def imports(self):
        # copy dependent libraries into the build folder
        self.copy("*", src="@bindirs", dst="bin")
        self.copy("*", src="@libdirs", dst="lib")

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

        # run unit tests if CONAN_RUN_TESTS environment variable is set to 1
        #if os.getenv("CONAN_RUN_TESTS") == "1":
        #    cmake.test()

    def package(self):
        # install from build directory into package directory
        cmake = CMake(self)
        cmake.install()

        # also copy dependent libraries into the package
        #self.copy("*.dll", "bin", "bin")
        #self.copy("*.dylib*", "lib", "lib", symlinks = True)
        #self.copy("*.so*", "lib", "lib", symlinks = True)

    def package_info(self):
        pass
