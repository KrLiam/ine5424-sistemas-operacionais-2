CXX ?= g++

# flags
LOG_LEVEL = 0
COMPILE_FLAGS = -std=c++20 -Wall -Wextra -g -DLOG_LEVEL=$(LOG_LEVEL)
INCLUDES = -I ../include/ -I ../lib/ -I src/ -I /usr/local/include
LIBS =

# caminhos
SRC_PATH = src
SRC_EXT = cpp
BUILD_PATH = build
BIN_PATH = $(BUILD_PATH)/bin

LIB_NAME = communication
LIB_PATH = ../lib$(LIB_NAME).a

# executavel
BIN_NAME = program

SOURCES = $(shell find $(SRC_PATH) -name '*.$(SRC_EXT)' | sort -k 1nr | cut -f2-)
OBJECTS = $(SOURCES:$(SRC_PATH)/%.$(SRC_EXT)=$(BUILD_PATH)/%.o)
DEPS = $(OBJECTS:.o=.d)

.PHONY: default_target
default_target: release

.PHONY: release
release: export CXXFLAGS := $(CXXFLAGS) $(COMPILE_FLAGS)
release: dirs
	@$(MAKE) all

.PHONY: dirs
dirs:
	@mkdir -p $(dir $(OBJECTS))
	@mkdir -p $(BIN_PATH)

.PHONY: clean
clean:
	@$(RM) $(BIN_NAME)
	@$(RM) -r $(BUILD_PATH)
	@$(RM) -r $(BIN_PATH)
	@$(MAKE) -C ../ clean

.PHONY: all
all: $(BIN_PATH)/$(BIN_NAME)
	@$(RM) $(BIN_NAME)
	@ln -s $(BIN_PATH)/$(BIN_NAME) $(BIN_NAME)

# Criando o executável
$(BIN_PATH)/$(BIN_NAME): $(LIB_PATH) $(OBJECTS)
	$(CXX) $(OBJECTS) -L../ -l${LIB_NAME} -o $@ ${LIBS}

# compilar .cpp's
$(BUILD_PATH)/%.o: $(SRC_PATH)/%.$(SRC_EXT)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -MP -MMD -c $< -o $@

# compilar biblioteca
$(LIB_PATH):
	$(MAKE) -C ../
