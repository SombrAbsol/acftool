CC := $(shell command -v clang >/dev/null 2>&1 && echo clang || echo gcc)
CFLAGS  := -O3 -Wall -Wextra -Werror

ifeq ($(OS),Windows_NT)
	TARGET := acftool.exe
else
	TARGET := acftool
endif

SRCS    := acftool.c
OBJS    := $(SRCS:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^

%.o: %.c acfdump.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJS) $(TARGET)
