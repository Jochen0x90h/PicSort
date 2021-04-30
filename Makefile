.PHONY: release debug install upload

# name (from conan package)
NAME := $(shell python3 -c 'from conanfile import Project; print(Project.name)')

# version (from git branch or tag)
left := (
right := )
BRANCH := $(shell git tag -l --points-at HEAD)
ifeq ($(BRANCH),)
	BRANCH := $(shell git rev-parse --abbrev-ref HEAD)
endif
VERSION := $(subst /,-,$(subst $(left),_,$(subst $(right),_,$(BRANCH))))

# name/version@user/channel
REFERENCE := $(NAME)/$(VERSION)@

# options
export CXXFLAGS=-march=core-avx2
OPTIONS :=


# default target
all: release debug

# build release and run unit tests
release: export CONAN_RUN_TESTS=1
release:
	conan create $(OPTIONS) . $(REFERENCE)

# build debug and run unit tests
debug: export CONAN_RUN_TESTS=1
debug:
	conan create --profile Debug $(OPTIONS) . $(REFERENCE)

# install to ~/.local
install: export CONAN_INSTALL_PREFIX=${HOME}/.local
install:
	conan install $(REFERENCE)

# upload package to conan repository
upload:
	conan upload $(REFERENCE) --all --force
