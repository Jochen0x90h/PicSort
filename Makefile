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
export CONAN_RUN_TESTS=0
export CONAN_INSTALL_PREFIX=${HOME}/.local
OPTIONS := --build=missing


# default target
all: release debug

# build release and run unit tests
release:
	conan create $(OPTIONS) . $(REFERENCE)

# build debug and run unit tests
debug:
	conan create --profile Debug $(OPTIONS) . $(REFERENCE)

# install (e.g. to ~/.local)
install:
	conan install $(REFERENCE)

# upload package to conan repository
upload:
	conan upload $(REFERENCE) --all --force
