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
ifeq ($(shell uname -s),Darwin)
ifeq ($(STATIC),1)
$(error Static linking is not supported on macOS)
endif
endif

CFLAGS   := -Wall -Wextra -Werror
CPPFLAGS := -I $(HEADER_DIR) $(if $(filter 1,$(STATIC)),-DSTATIC_LINK)
LDFLAGS   = $(if $(filter 1,$(STATIC)),-static)
LDLIBS   :=
DEPFLAGS := -MMD -MP

ifeq ($(OS),Windows_NT)
SANITIZE := -fsanitize=undefined -fsanitize-trap=undefined
LTO_LD   := $(if $(findstring clang,$(CC)),-fuse-ld=lld)
else
SANITIZE := -fsanitize=address,undefined
LTO_LD   :=
endif

ifeq ($(STATIC),1)
SANITIZE := -fsanitize=undefined -fsanitize-trap=undefined
endif

TARGET_NAME := acftool
EXTENSION   := $(if $(filter Windows_NT,$(OS)),.exe,)
TARGET      := $(BUILD_DIR)/$(TARGET_NAME)$(EXTENSION)

SRCS    := $(wildcard $(SRC_DIR)/*.c)
HEADERS := $(wildcard $(HEADER_DIR)/*.h)
OBJ_DIR := $(BUILD_DIR)/$(TARGET_NAME).dir
OBJS    := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))
DEPS    := $(OBJS:.o=.d)

$(if $(SRCS),,$(error No .c files found in $(SRC_DIR)))

.PHONY: all clean format release native debug install uninstall FORCE $(TARGET_NAME)

all: release

$(TARGET_NAME): $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(DEPFLAGS) $(CFLAGS) $(CPPFLAGS) $(VERSION_FLAGS) -c $< -o $@

-include $(DEPS)

$(OBJ_DIR)/main.o: FORCE
$(OBJ_DIR)/main.o: VERSION_FLAGS := -DBUILD_DATE=$(shell date -u +%Y.%m.%d-%H:%M:%S)

$(OBJS): $(OBJ_DIR)/flags
$(OBJ_DIR)/flags: FORCE
	@mkdir -p $(@D)
	@flags='$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS)'; \
		[ "$$(cat $@ 2>/dev/null)" = "$$flags" ] || echo "$$flags" > $@

release: CFLAGS   += -O3 -DNDEBUG
release: CPPFLAGS += -DMAKE_RECIPE=release
release: $(TARGET)
	$(STRIP) $(TARGET)

native: CFLAGS   += -O3 -march=native -flto -DNDEBUG
native: CPPFLAGS += -DMAKE_RECIPE=native
native: LDFLAGS  += -flto $(LTO_LD)
native: $(TARGET)
	$(STRIP) $(TARGET)

debug: CFLAGS   += -Og -g $(SANITIZE) -fno-omit-frame-pointer
debug: CPPFLAGS += -DMAKE_RECIPE=debug
debug: LDFLAGS  += $(SANITIZE)
debug: $(TARGET)

install: $(if $(wildcard $(TARGET)),,release)
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
