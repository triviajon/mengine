CC = clang
CFLAGS = -Wall -Wextra -O0 -g -march=native -I.

ENGINE_SRC := $(shell find src -name '*.c' ! -name 'main.c')
ENGINE_OBJ := $(ENGINE_SRC:.c=.o)
ENGINE_LIB := libmengine.a

TEST_SRC := $(shell find tests -name '*.c' ! -path 'tests/helpers/*')
TEST_BINARIES := $(TEST_SRC:.c=)

HELPERS_SRC := $(shell find tests/helpers -name '*.c')
HELPERS_OBJ := $(HELPERS_SRC:.c=.o)

all: $(ENGINE_LIB) tests

$(ENGINE_LIB): $(ENGINE_OBJ)
	ar rcs $@ $^

tests: $(TEST_BINARIES)

$(TEST_BINARIES): %: %.c $(HELPERS_OBJ) $(ENGINE_LIB)
	$(CC) $(CFLAGS) -o $@ $< $(HELPERS_OBJ) $(ENGINE_LIB)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(ENGINE_OBJ) $(HELPERS_OBJ) $(ENGINE_LIB) $(TEST_BINARIES)

.PHONY: all clean
