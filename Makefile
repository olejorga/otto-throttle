# PLUGIN := OttoThrottle

# SDK := SDK

# CXX := g++

# OS ?= lin

# BUILD := build/$(OS)

# SRC := $(wildcard src/*.cpp)
# OBJ := $(patsubst src/%.cpp,$(OS)/%.o,$(SRC))

# ifeq ($(OS),lin)
#     CXX = g++
#     DEFINES = -DLIN
#     OUTPUT = build/lin/lin.xpl
# endif

# ifeq ($(OS),win)
#     CXX = x86_64-w64-mingw32-g++
#     DEFINES = -DIBM
#     OUTPUT = build/win/win.xpl
# endif

# ifeq ($(OS),mac)
#     CXX = clang++
#     DEFINES = -DAPL
#     OUTPUT = build/mac/mac.xpl
# endif

# CXXFLAGS := \
#     -std=c++20 \
#     -Wall \
#     -Wextra \
#     -O2 \
#     -fPIC \
#     -I$(SDK)/CHeaders/XPLM \
#     -I$(SDK)/CHeaders/Wrappers

# CXXFLAGS += -DLIN

# LDFLAGS := \
#     -shared

# TARGET := build/$(OS)/lin.xpl

# all: $(TARGET)

# build/$(OS):
# 	mkdir -p build/$(OS)

# build/$(OS)/%.o: src/%.cpp | build/$(OS)
# 	$(CXX) $(CXXFLAGS) -c $< -o $@

# $(TARGET): $(OBJ)
# 	$(CXX) $(OBJ) $(LDFLAGS) -o $@

# clean:
# 	rm -rf build/$(OS)

# # install: $(TARGET)
# # 	mkdir -p "$(HOME)/X-Plane 12/Resources/plugins/$(PLUGIN)/64"
# # 	cp $(TARGET) \
# # 	   "$(HOME)/X-Plane 12/Resources/plugins/$(PLUGIN)/64/lin.xpl"

# .PHONY: all clean install

OS ?= lin
CXX := g++

BUILD := build/$(OS)
SDK := SDK

SRC := $(wildcard src/*.cpp)
OBJ := $(SRC:src/%.cpp=$(BUILD)/%.o)

TARGET := $(BUILD)/$(OS).xpl

CXXFLAGS := \
    -std=c++20 \
    -Wall \
    -Wextra \
    -O2 \
    -fPIC \
    -I$(SDK)/CHeaders/XPLM \
    -I$(SDK)/CHeaders/Wrappers

ifeq ($(OS),lin)
    CXXFLAGS += -DLIN
endif

ifeq ($(OS),win)
    CXXFLAGS += -DIBM
endif

ifeq ($(OS),mac)
    CXXFLAGS += -DAPL
endif

all: $(TARGET)

$(BUILD)/:
	mkdir -p $@

$(BUILD)/%.o: src/%.cpp | $(BUILD)/
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET): $(OBJ)
	$(CXX) $(OBJ) -shared -o $@

clean:
	rm -rf build

.PHONY: all clean
