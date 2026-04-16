CC := $(shell command -v clang >/dev/null 2>&1 && echo clang || echo gcc)
CFLAGS := -O3 -Wall -Wextra -Werror
SRC_DIR := src
BUILD_DIR := build

TARGET_NAME := acftool
TARGET := $(BUILD_DIR)/$(TARGET_NAME)$(if $(filter Windows_NT,$(OS)),.exe)

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJ_DIR := $(BUILD_DIR)/$(TARGET_NAME).dir
OBJS := $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS) | $(BUILD_DIR)
	$(CC) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR) $(OBJ_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR)
