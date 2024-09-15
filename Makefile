# Compiler and Flags
CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Werror
DEBUG_FLAGS := -g
CXXFLAGS_DEBUG := $(CXXFLAGS) $(DEBUG_FLAGS)

# Directories
CLIENT_DIR := client
LIB_DIR := lib
BUILD_DIR := build
OBJ_DIR := obj
BIN_DIR := bin
SRC_DIR := src
CPP_DIR := cpp
HEADER_DIR := header

# Client Directories
CLIENT_SRC_DIR := $(CLIENT_DIR)/$(SRC_DIR)
CLIENT_CPP_DIR := $(CLIENT_SRC_DIR)/$(CPP_DIR)
CLIENT_HEADER_DIR := $(CLIENT_SRC_DIR)/$(HEADER_DIR)

CLIENT_BUILD_DIR := $(CLIENT_DIR)/$(BUILD_DIR)
CLIENT_OBJ_DIR := $(CLIENT_BUILD_DIR)/$(OBJ_DIR)
CLIENT_BIN_DIR := $(CLIENT_BUILD_DIR)/$(BIN_DIR)

# Library Directories
LIB_BUILD_DIR := $(LIB_DIR)/$(BUILD_DIR)
LIB_OBJ_DIR := $(LIB_BUILD_DIR)/$(OBJ_DIR)
LIB_SRC_DIR := $(LIB_DIR)/$(SRC_DIR)
LIB_CPP_DIR := $(LIB_SRC_DIR)/$(CPP_DIR)
LIB_HEADER_DIR := $(LIB_SRC_DIR)/$(HEADER_DIR)

# Library Sources and Objects
LIB_SOURCES := $(wildcard $(LIB_CPP_DIR)/*.cpp)
LIB_OBJECTS := $(patsubst $(LIB_CPP_DIR)/%.cpp,$(LIB_OBJ_DIR)/%.o,$(LIB_SOURCES))

# Client Sources and Objects
CLIENT_SOURCES := $(wildcard $(CLIENT_CPP_DIR)/*.cpp)
CLIENT_OBJECTS := $(patsubst $(CLIENT_CPP_DIR)/%.cpp,$(CLIENT_OBJ_DIR)/%.o,$(CLIENT_SOURCES))

# Targets
TARGET := $(CLIENT_BIN_DIR)/client

# Default target
all: .directories $(TARGET)

# Build target
$(TARGET): $(CLIENT_OBJECTS) $(LIB_OBJECTS)
	$(CXX) $(CXXFLAGS) $(CLIENT_OBJECTS) $(LIB_OBJECTS) -o $(TARGET)

# Build target with debugging information
.debug: CXXFLAGS := $(CXXFLAGS_DEBUG)
.debug: .directories $(TARGET)

# Build object files from library source
$(LIB_OBJ_DIR)/%.o: $(LIB_CPP_DIR)/%.cpp
	@mkdir -p $(LIB_OBJ_DIR)
	$(CXX) $(CXXFLAGS) -I$(LIB_HEADER_DIR) -c $< -o $@

# Build object files from send source
$(CLIENT_OBJ_DIR)/%.o: $(CLIENT_CPP_DIR)/%.cpp
	@mkdir -p $(CLIENT_OBJ_DIR)
	$(CXX) $(CXXFLAGS) -I$(CLIENT_HEADER_DIR) -c $< -o $@

# Create directories
.directories:
	mkdir -p $(CLIENT_OBJ_DIR) $(CLIENT_BIN_DIR) $(LIB_OBJ_DIR)

# Clean target
.clean:
	rm -rf $(CLIENT_OBJ_DIR) $(CLIENT_BIN_DIR) $(LIB_OBJ_DIR) $(TARGET)
