CXX ?= g++

# flags
LOG_LEVEL = 0
COMPILE_FLAGS = -std=c++20 -Wall -Wextra -g -DLOG_LEVEL=$(LOG_LEVEL)
INCLUDES = -I include/ -I lib/ -I /usr/local/include
LIBS =

# caminhos
SRC_PATH = lib
SRC_EXT = cpp
BUILD_PATH = build
LIB_PATH = $(BUILD_PATH)/lib

# executavel
LIB_NAME = libcommunication.a


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
	@mkdir -p $(LIB_PATH)

.PHONY: clean
clean:
	@$(RM) $(LIB_NAME)
	@$(RM) -r $(BUILD_PATH)
	@$(RM) -r $(LIB_PATH)

.PHONY: all
all: $(LIB_PATH)/$(LIB_NAME)
	@$(RM) $(LIB_NAME)
	@ln -s $(LIB_PATH)/$(LIB_NAME) $(LIB_NAME)

# Criando o executável
# $(LIB_PATH)/$(LIB_NAME): $(OBJECTS)
# 	$(CXX) $(OBJECTS) -o $@ ${LIBS}

# Criar biblioteca estática
$(LIB_PATH)/$(LIB_NAME): $(OBJECTS)
	@ar rcs $(LIB_PATH)/$(LIB_NAME) $(OBJECTS)

-include $(DEPS)

$(BUILD_PATH)/%.o: $(SRC_PATH)/%.$(SRC_EXT)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -MP -MMD -c $< -o $@