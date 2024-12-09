CC = clang
CFLAGS = -Wall -Wextra -O0 -g -march=native -ffast-math -funroll-loops -I./src/kernel -I./src/engine
SRC = ./src/main.c $(wildcard ./src/kernel/*.c) $(wildcard ./src/engine/*.c) $(wildcard ./src/examples/*.c)
OBJ = $(SRC:.c=.o)
TARGET = main

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean
