# OS ?= lin
# CXX := g++

# BUILD := build/$(OS)
# SDK := SDK

# SRC := $(wildcard src/*.cpp)
# OBJ := $(SRC:src/%.cpp=$(BUILD)/%.o)

# TARGET := $(BUILD)/$(OS).xpl

# CXXFLAGS := \
#     -std=c++20 \
#     -Wall \
#     -Wextra \
#     -O2 \
#     -fPIC \
#     -I$(SDK)/CHeaders/XPLM \
#     -I$(SDK)/CHeaders/Wrappers

# ifeq ($(OS),lin)
#     CXXFLAGS += -DLIN
# endif

# ifeq ($(OS),win)
#     CXXFLAGS += -DIBM
# endif

# ifeq ($(OS),mac)
#     CXXFLAGS += -DAPL
# endif

# all: $(TARGET)

# $(BUILD)/:
# 	mkdir -p $@

# $(BUILD)/%.o: src/%.cpp | $(BUILD)/
# 	$(CXX) $(CXXFLAGS) -c $< -o $@

# $(TARGET): $(OBJ)
# 	$(CXX) $(OBJ) -shared -o $@

# clean:
# 	rm -rf build

# .PHONY: all clean

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
    -I$(SDK)/CHeaders/Wrappers \
    -DXPLM200 -DXPLM210 -DXPLM300 -DXPLM301 -DXPLM303 -DXPLM400

LDFLAGS :=
LIBS :=

ifeq ($(OS),lin)
    CXXFLAGS += -DLIN
    LDFLAGS  += -shared
endif

ifeq ($(OS),win)
    CXXFLAGS += -DIBM
    LDFLAGS  += -shared -static-libgcc -static-libstdc++
    LIBS     += -L$(SDK)/Libraries/Win -lXPLM_64 -lXPWidgets_64
endif

ifeq ($(OS),mac)
    CXXFLAGS += -DAPL
    LDFLAGS  += -bundle
    LIBS     += -F$(SDK)/Libraries/Mac -framework XPLM -framework XPWidgets
endif

all: $(TARGET)

$(BUILD)/:
	mkdir -p $@

$(BUILD)/%.o: src/%.cpp | $(BUILD)/
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET): $(OBJ)
	$(CXX) $(OBJ) $(LDFLAGS) $(LIBS) -o $@

clean:
	rm -rf build

.PHONY: all clean
