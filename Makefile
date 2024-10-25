CC = clang
CFLAGS = -Wall -Wextra -g -O0 -I./src/kernel -I./src/engine
SRC = ./src/main.c $(wildcard ./src/kernel/*.c) $(wildcard ./src/engine/*.c)
OBJ = $(SRC:.c=.o)
TARGET = main

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean
