
LIB_NAME = communication

# flags
CXX ?= g++
LOG_LEVEL = 0
COMPILE_FLAGS = -std=c++20 -Wall -Wextra -g -DLOG_LEVEL=$(LOG_LEVEL)
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



.PHONY: default_target
default_target: release

.PHONY: release
release: export CXXFLAGS := $(CXXFLAGS) $(COMPILE_FLAGS)
release: dirs
	@$(MAKE) all



.PHONY: dirs
dirs:
	@mkdir -p $(dir $(LIB_OBJECTS)) $(dir $(TEST_OBJECTS))
	@mkdir -p $(LIB_PATH)
	@mkdir -p $(BIN_PATH)



.PHONY: clean
clean:
	@$(RM) $(LIB_FILENAME)
	@$(RM) $(TEST_BIN_FILENAME)
	@$(RM) -r $(BUILD_PATH)
	@$(RM) -r $(LIB_PATH)


# make all
#
# Compila a biblioteca e gera um arquivo .a
.PHONY: all
all: $(LIB_PATH)/$(LIB_FILENAME)
	@$(RM) $(LIB_FILENAME)
	@ln -s $(LIB_PATH)/$(LIB_FILENAME) $(LIB_FILENAME)

-include $(DEPS)

$(BUILD_PATH)/%.o: $(SRC_PATH)/%.$(SRC_EXT)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -MP -MMD -c $< -o $@

# Criar biblioteca estática
$(LIB_PATH)/$(LIB_FILENAME): $(LIB_OBJECTS)
	ar rcs $(LIB_PATH)/$(LIB_FILENAME) $(LIB_OBJECTS)


# Nome do arquivo binário para os testes unitários
UNITARY_TEST_BIN = unitaryTests

# Caminho para os arquivos de teste
TEST_PATH = test

# make test
#
# Comando para compilar programa de testes a partir de unitaryTests.cpp e process_runner.cpp
.PHONY: test
test: export CXXFLAGS := $(CXXFLAGS) $(COMPILE_FLAGS)
test: dirs $(BIN_PATH)/$(UNITARY_TEST_BIN)
	@$(RM) $(UNITARY_TEST_BIN)
	@ln -s $(BIN_PATH)/$(UNITARY_TEST_BIN) $(UNITARY_TEST_BIN)

$(BUILD_PATH)/test/unitaryTests.o: $(TEST_PATH)/unitaryTests.$(SRC_EXT)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -MP -MMD -c $< -o $@

# Compila o process_runner.cpp que está no diretório test/
$(BUILD_PATH)/test/process_runner.o: $(TEST_PATH)/process_runner.$(SRC_EXT)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -MP -MMD -c $< -o $@

# Agora, ao compilar unitaryTests, também linkamos o process_runner.o
$(BIN_PATH)/$(UNITARY_TEST_BIN): $(LIB_PATH)/$(LIB_FILENAME) $(BUILD_PATH)/test/unitaryTests.o $(BUILD_PATH)/test/process_runner.o
	$(CXX) -o $@ $(BUILD_PATH)/test/unitaryTests.o $(BUILD_PATH)/test/process_runner.o -L $(LIB_PATH) -l$(LIB_NAME)

# make run id=...
#
# Comando para compilar programa de teste de automaticamente executá-lo
.PHONY: run
run: export CXXFLAGS := $(CXXFLAGS) $(COMPILE_FLAGS)
run: dirs $(BIN_PATH)/$(TEST_BIN_FILENAME)
	./$(BIN_PATH)/$(TEST_BIN_FILENAME) $(id)
