import os, shutil
from conans import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps, CMake

# linux / windows (python via Microsoft Store):
# install conan: pip3 install --user conan
# upgrade conan: pip3 install --upgrade --user conan

# macos (homebrew):
# install conan: brew install conan

# create default profile: conan profile new default --detect
# create debug profile: copy ~/.conan/profiles/default to Debug, replace Release by Debug

# helper function for deploy()
def copy(src, dst):
    if os.path.islink(src):
        if os.path.lexists(dst):
            os.unlink(dst)
        linkto = os.readlink(src)
        os.symlink(linkto, dst)
    else:
        shutil.copy(src, dst)

class Project(ConanFile):
    name = "PicSorter"
    description = "Tool for sorting pictures"
    url = "https://github.com/Jochen0x90h/PicSort"
    license = "MIT License"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps"
    exports_sources = "conanfile.py", "CMakeLists.txt", "src/*", "test/*"
    requires = [
        "boost/1.80.0",
        "glfw/3.3.8",
        "imgui/1.89.1",
        "libjpeg-turbo/2.1.4",
        "tinyxml2/9.0.0"]


    keep_imports = True
    def imports(self):
        # copy dependent libraries into the build folder
        self.copy("*", src="@bindirs", dst="bin")
        self.copy("*", src="@libdirs", dst="lib")

    def generate(self):
        # generate "conan_toolchain.cmake"
        toolchain = CMakeToolchain(self)

        # bake CC and CXX from profile into toolchain
        toolchain.blocks["generic_system"].values["compiler"] = self.env.get("CC", None)
        toolchain.blocks["generic_system"].values["compiler_cpp"] = self.env.get("CXX", None)

        toolchain.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

        # run unit tests if CONAN_RUN_TESTS environment variable is set to 1
        if os.getenv("CONAN_RUN_TESTS") == "1":
            cmake.test()

    def package(self):
        # install from build directory into package directory
        cmake = CMake(self)
        cmake.install()

        # also copy dependent libraries into the package
        self.copy("*.dll", "bin", "bin")
        self.copy("*.dylib*", "lib", "lib", symlinks = True)
        self.copy("*.so*", "lib", "lib", symlinks = True)

    def package_info(self):
        pass

    def deploy(self):
        # install if CONAN_INSTALL_PREFIX env variable is set
        prefix = os.getenv("CONAN_INSTALL_PREFIX")
        if prefix == None:
            print("set CONAN_INSTALL_PREFIX env variable to install to local directory, e.g.")
            print("export CONAN_INSTALL_PREFIX=$HOME/.local")
        else:
            print(f"Installing {self.name} to {prefix}")

            # create destination directories if necessary
            dstBinPath = os.path.join(prefix, "bin")
            if not os.path.exists(dstBinPath):
                os.mkdir(dstBinPath)
            #print(f"dstBinPath: {dstBinPath}")
            dstLibPath = os.path.join(prefix, "lib")
            if not os.path.exists(dstLibPath):
                os.mkdir(dstLibPath)
            #print(f"dstLibPath: {dstLibPath}")

            # copy executables
            for bindir in self.cpp_info.bindirs:
                srcBinPath = os.path.join(self.cpp_info.rootpath, bindir)
                #print(f"srcBinPath {srcBinPath}")
                if os.path.isdir(srcBinPath):
                    files = os.listdir(srcBinPath)
                    for file in files:
                        print(f"install {file}")
                        src = os.path.join(srcBinPath, file)
                        dst = os.path.join(dstBinPath, file)
                        if os.path.isfile(src):
                            copy(src, dst)

            # copy libraries
            for libdir in self.cpp_info.libdirs:
                srcLibPath = os.path.join(self.cpp_info.rootpath, libdir)
                #print(f"srcLibPath {srcLibPath}")
                if os.path.isdir(srcLibPath):
                    files = os.listdir(srcLibPath)
                    for file in files:
                        print(f"install {file}")
                        src = os.path.join(srcLibPath, file)
                        dst = os.path.join(dstLibPath, file)
                        if os.path.isfile(src):
                            copy(src, dst)
