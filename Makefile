# SPDX-FileCopyrightText: 2026 SombrAbsol
#
# SPDX-License-Identifier: MIT

CC    := $(shell command -v clang >/dev/null 2>&1 && echo clang || echo gcc)
STRIP := $(shell command -v llvm-strip >/dev/null 2>&1 && echo llvm-strip || echo strip)

CFLAGS   := -O3 -Wall -Wextra -Werror -MMD -MP
CPPFLAGS := -I include
LDFLAGS  :=
LDLIBS   :=

SRC_DIR   := src
BUILD_DIR := build
PREFIX    := /usr/local

TARGET_NAME := acftool
EXTENSION   := $(if $(filter Windows_NT,$(OS)),.exe)
TARGET      := $(BUILD_DIR)/$(TARGET_NAME)$(EXTENSION)

SRCS    := $(wildcard $(SRC_DIR)/*.c)
OBJ_DIR := $(BUILD_DIR)/$(TARGET_NAME).dir
OBJS    := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))
DEPS    := $(OBJS:.o=.d)

.PHONY: all clean install uninstall release $(TARGET_NAME)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(TARGET_NAME): $(TARGET)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $@

-include $(DEPS)

release: $(TARGET)
	$(STRIP) $(TARGET)

install: all
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/$(TARGET_NAME)$(EXTENSION)
	$(STRIP) $(DESTDIR)$(PREFIX)/bin/$(TARGET_NAME)$(EXTENSION)

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(TARGET_NAME)$(EXTENSION)

clean:
	rm -rf $(BUILD_DIR)
