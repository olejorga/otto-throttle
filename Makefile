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
