
LIB_NAME = communication

# flags
CXX ?= g++
LOG_LEVEL = 2
APP_FLAGS = -DLOG_LEVEL=$(LOG_LEVEL)
COMPILE_FLAGS = -std=c++20 -Wall -Wextra -g $(APP_FLAGS)
INCLUDES = -I include/ -I lib/ -I /usr/local/include

# caminhos
SRC_PATH = lib
TEST_PATH = test
SRC_EXT = cpp
BUILD_PATH = build
BIN_PATH = $(BUILD_PATH)/bin
LIB_PATH = $(BUILD_PATH)/lib

# arquivos de saída
LIB_FILENAME = lib$(LIB_NAME).a
TEST_BIN_FILENAME = program


LIB_SOURCES = $(shell find $(SRC_PATH) -name '*.$(SRC_EXT)' | sort -k 1nr | cut -f2-)
LIB_OBJECTS = $(LIB_SOURCES:$(SRC_PATH)/%.$(SRC_EXT)=$(BUILD_PATH)/%.o)
DEPS = $(LIB_OBJECTS:.o=.d)

TEST_SOURCES = $(shell find $(TEST_PATH) -name '*.$(SRC_EXT)' | sort -k 1nr | cut -f2-)
TEST_OBJECTS = $(TEST_SOURCES:$(TEST_PATH)/%.$(SRC_EXT)=$(BUILD_PATH)/test/%.o)
TEST_DEPS = $(TEST_OBJECTS:.o=.d)

# argumentos do programa de testes
id = 0
cases_dir = ./test/cases

.PHONY: default_target
default_target: release


.PHONY: release
release: shell

.PHONY: all
all: lib


# make dirs
#
# Cria os diretórios de build
.PHONY: dirs
dirs:
	@mkdir -p $(dir $(LIB_OBJECTS)) $(dir $(TEST_OBJECTS))
	@mkdir -p $(LIB_PATH)
	@mkdir -p $(BIN_PATH)


# make clean
#
# limpa todos os diretórios e arquivos gerados na build
.PHONY: clean
clean:
	@$(RM) $(LIB_FILENAME)
	@$(RM) $(TEST_BIN_FILENAME)
	@$(RM) -r $(BUILD_PATH)
	@$(RM) -r $(LIB_PATH)


# make lib
#
# Compila a biblioteca e gera um arquivo .a
.PHONY: lib
lib: $(LIB_PATH)/$(LIB_FILENAME)
	@$(RM) $(LIB_FILENAME)
	@ln -s $(LIB_PATH)/$(LIB_FILENAME) $(LIB_FILENAME)

-include $(DEPS)

$(BUILD_PATH)/%.o: $(SRC_PATH)/%.$(SRC_EXT)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -MP -MMD -c $< -o $@

# Criar biblioteca estática
$(LIB_PATH)/$(LIB_FILENAME): $(LIB_OBJECTS)
	ar rcs $(LIB_PATH)/$(LIB_FILENAME) $(LIB_OBJECTS)


# make program
#
# Comando para compilar programa de testes. Irá compilar a biblioteca em conjunto.
.PHONY: program
program: export CXXFLAGS := $(CXXFLAGS) $(COMPILE_FLAGS)
program: dirs $(BIN_PATH)/$(TEST_BIN_FILENAME)
	@$(RM) $(TEST_BIN_FILENAME)
	@ln -s $(BIN_PATH)/$(TEST_BIN_FILENAME) $(TEST_BIN_FILENAME)

-include $(TEST_DEPS)

$(BUILD_PATH)/test/%.o: $(TEST_PATH)/%.$(SRC_EXT)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -MP -MMD -c $< -o $@

$(BIN_PATH)/$(TEST_BIN_FILENAME): $(LIB_PATH)/$(LIB_FILENAME) $(TEST_OBJECTS)
	$(CXX) -o $@ $(TEST_OBJECTS) -L $(LIB_PATH) -l$(LIB_NAME)


# make shell id=...
#
# Comando para compilar programa de teste de automaticamente executá-lo
.PHONY: shell
shell: export CXXFLAGS := $(CXXFLAGS) $(COMPILE_FLAGS)
shell: dirs $(BIN_PATH)/$(TEST_BIN_FILENAME)
	./$(BIN_PATH)/$(TEST_BIN_FILENAME) $(id)

# make test case=...
#
# Comando para compilar programa de teste e automaticamente executá-lo 
.PHONY: test
test: export CXXFLAGS := $(CXXFLAGS) $(COMPILE_FLAGS)
test: dirs $(BIN_PATH)/$(TEST_BIN_FILENAME)
	./$(BIN_PATH)/$(TEST_BIN_FILENAME) test $(case)

.PHONY: test-all
test-all:
	$(foreach file, $(wildcard $(cases_dir)/*.case), $(MAKE) test case=$(file);)