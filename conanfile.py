from conans import ConanFile, CMake, tools
import os

# linux:
# install conan: pip3 install --user conan
# upgrade conan: pip3 install --upgrade --user conan

# macos:
# install conan: brew install conan

# create default profile: conan profile new default --detect
# create debug profile: copy ~/.conan/profiles/default to Debug, replace Release by Debug

def get_version():
    git = tools.Git()
    try:
        version = git.get_tag() # check if a tagged version
        if version is None:
            if 'BRANCH_NAME' in os.environ:
                # this is probably Jenkins
                version = os.environ['BRANCH_NAME']
            else:
                # this is probably building manually
                version = git.get_branch()
        return version.replace("/", "-")
    except:
        None

def get_reference():
    return f"{Project.name}/{Project.version}@"

class Project(ConanFile):
    name = "PicSorter"
    version = get_version()
    description = "Tool for sorting pictures"
    url = ""
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "debug": [False, True]}
    default_options = {
        "debug": False,
        "boost:shared": True}
    generators = "cmake"
    exports_sources = "conanfile.py", "CMakeLists.txt", "src/*", "test/*"
    requires = \
        "boost/1.74.0", \
        "imgui/1.79", \
        "glfw/3.3.2", \
        "libjpeg-turbo/2.0.5", \
        "tinyxml2/8.0.0"
        

    def configure_cmake(self):
        cmake = CMake(self, build_type = "RelWithDebInfo" if self.options.debug and self.settings.build_type == "Release" else None)
        cmake.configure()
        return cmake

    def build(self):
        cmake = self.configure_cmake()
        cmake.build()

    def imports(self):
        # copy shared libs into our project
        self.copy("*.dll", src="bin", dst="bin")
        self.copy("*.dylib*", src="lib", dst="bin")
        self.copy("*.so*", src="lib", dst="bin")
        
    def package(self):
        cmake = self.configure_cmake()
        cmake.install()

    def package_info(self):
        self.cpp_info.name = name
