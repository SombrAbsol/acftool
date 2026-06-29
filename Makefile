# SPDX-FileCopyrightText: 2026 SombrAbsol
#
# SPDX-License-Identifier: MIT

CC    := $(shell command -v clang >/dev/null 2>&1 && echo clang || echo gcc)
STRIP := $(shell command -v llvm-strip >/dev/null 2>&1 && echo llvm-strip || echo strip)

SRC_DIR    := src
HEADER_DIR := include
BUILD_DIR  := build
PREFIX     := /usr/local

STATIC ?= 0
ifeq ($(OS),Darwin)
ifeq ($(STATIC),1)
$(error Static linking is not supported on macOS)
endif
endif

CFLAGS   := -Wall -Wextra -Werror
CPPFLAGS := -I $(HEADER_DIR)
LDFLAGS   = $(if $(filter 1,$(STATIC)),-static)
LDLIBS   :=
DEPFLAGS := -MMD -MP

TARGET_NAME := acftool
EXTENSION   := $(if $(filter Windows_NT,$(OS)),.exe,)
TARGET      := $(BUILD_DIR)/$(TARGET_NAME)$(EXTENSION)

SRCS    := $(wildcard $(SRC_DIR)/*.c)
HEADERS := $(wildcard $(HEADER_DIR)/*.h)
OBJ_DIR := $(BUILD_DIR)/$(TARGET_NAME).dir
OBJS    := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))
DEPS    := $(OBJS:.o=.d)

$(if $(SRCS),,$(error No .c files found in $(SRC_DIR)))

.PHONY: all clean format release native debug install uninstall $(TARGET_NAME)

all: release

$(TARGET_NAME): $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(DEPFLAGS) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

-include $(DEPS)

release: CFLAGS  += -O3 -DNDEBUG
release: $(TARGET)
	$(STRIP) $(TARGET)

native: CFLAGS  += -O3 -march=native -flto -DNDEBUG
native: LDFLAGS += -flto
native: $(TARGET)
	$(STRIP) $(TARGET)

debug: CFLAGS  += -Og -g -fsanitize=address,undefined -fno-omit-frame-pointer
debug: LDFLAGS += -fsanitize=address,undefined
debug: $(TARGET)

install: all
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/$(TARGET_NAME)$(EXTENSION)

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(TARGET_NAME)$(EXTENSION)

format:
	@command -v clang-format >/dev/null 2>&1 || \
		{ echo "clang-format not found"; exit 1; }
	clang-format -i $(SRCS) $(HEADERS)

clean:
	rm -rf $(BUILD_DIR)
