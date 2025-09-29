INC_DIR := inc
SRC_DIR := src
BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
DEP_DIR := $(BUILD_DIR)/dep
BIN_DIR := bin

SRCS := $(wildcard $(SRC_DIR)/*.c) 
OBJS := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))
DEPS := $(patsubst $(SRC_DIR)/%.c, $(DEP_DIR)/%.d, $(SRCS))
TARGET := $(BIN_DIR)/main

CXX := gcc
WARNINGS := -Wall -Wextra -Werror -Wpedantic
CPPFLAGS := -I$(INC_DIR) -MMD -MP
CXXFLAGS := -g $(WARNINGS) $(CPPFLAGS)

LD := gcc
LDFLAGS := -lm

all: $(TARGET)

$(TARGET): $(OBJS) | $(BIN_DIR)
	@echo "Linking $@..."
	@$(LD) $(LDFLAGS) $^ -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR) $(DEP_DIR)
	@echo "Compiling $@..."
	@$(CXX) $(CXXFLAGS) -MF $(DEP_DIR)/$*.d -c $< -o $@

$(OBJ_DIR) $(DEP_DIR) $(BIN_DIR):
	@mkdir -p $@

clean:
	@echo "Cleaning up..."
	@rm -rf $(BUILD_DIR) $(BIN_DIR)

run: $(TARGET)
	@echo "Running $(TARGET)..."
	@./$(TARGET)

debug: $(TARGET)
	@echo "debugging $(TARGET)..."
	@gdb ./$(TARGET)

valgrind: $(TARGET)
	@echo "Running $(TARGET) with valgrind..."
	@valgrind ./$(TARGET)

-include $(DEPS)

.PHONY: all clean run debug