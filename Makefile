PLUGIN := OttoThrottle

SDK := SDK

CXX := g++

SRC := $(wildcard src/*.cpp)
OBJ := $(patsubst src/%.cpp,build/%.o,$(SRC))

OS ?= lin

ifeq ($(OS),lin)
    CXX = g++
    DEFINES = -DLIN
    OUTPUT = build/lin/lin.xpl
endif

ifeq ($(OS),win)
    CXX = x86_64-w64-mingw32-g++
    DEFINES = -DIBM
    OUTPUT = build/win/win.xpl
endif

ifeq ($(OS),mac)
    CXX = clang++
    DEFINES = -DAPL
    OUTPUT = build/mac/mac.xpl
endif

CXXFLAGS := \
    -std=c++20 \
    -Wall \
    -Wextra \
    -O2 \
    -fPIC \
    -I$(SDK)/CHeaders/XPLM \
    -I$(SDK)/CHeaders/Wrappers

CXXFLAGS += -DLIN

LDFLAGS := \
    -shared

TARGET := build/lin.xpl

all: $(TARGET)

build:
	mkdir -p build

build/%.o: src/%.cpp | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET): $(OBJ)
	$(CXX) $(OBJ) $(LDFLAGS) -o $@

clean:
	rm -rf build

# install: $(TARGET)
# 	mkdir -p "$(HOME)/X-Plane 12/Resources/plugins/$(PLUGIN)/64"
# 	cp $(TARGET) \
# 	   "$(HOME)/X-Plane 12/Resources/plugins/$(PLUGIN)/64/lin.xpl"

.PHONY: all clean install
