CC      := gcc
CFLAGS  := -O3 -Wall -Wextra -Werror

TARGET  := acftool
SRCS    := acftool.c
OBJS    := $(SRCS:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c acfdump.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJS) $(TARGET) $(TARGET).exe
