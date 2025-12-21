CC = clang
CFLAGS = -Wall -Wextra -O0 -g -march=native -I.
LDFLAGS =

ENGINE_SRC := $(shell find src -name '*.c' ! -name 'main.c')
ENGINE_OBJ := $(ENGINE_SRC:.c=.o)
ENGINE_LIB := libmengine.a

MENGINE_BIN := mengine
MENGINE_SRC := src/main.c
MENGINE_OBJ := $(MENGINE_SRC:.c=.o)

TEST_SRC := $(shell find tests -name '*.c' ! -path 'tests/helpers/*' ! -name 'test_driver.c')
TEST_BINARIES := $(TEST_SRC:.c=)

HELPERS_SRC := $(shell find tests/helpers -name '*.c')
HELPERS_OBJ := $(HELPERS_SRC:.c=.o)

UNAME := $(shell uname)

# On macOS, detect and link argp-standalone if available
ifeq ($(UNAME), Darwin)
    ARGP_PREFIX := $(shell brew --prefix argp-standalone 2>/dev/null || echo "")
    ifneq ($(ARGP_PREFIX),)
        CFLAGS += -I$(ARGP_PREFIX)/include
        LDFLAGS += -L$(ARGP_PREFIX)/lib -largp
    else
        $(error argp-standalone not found via brew, please install it with `brew install argp-standalone`)
    endif
endif

all: $(ENGINE_LIB) $(MENGINE_BIN)

.clangd: .clangd.template
	@echo "Generating .clangd..."
ifeq ($(UNAME), Darwin)
	@echo "CompileFlags:" > .clangd
	@echo "  Add:" >> .clangd
	@echo "    - -I$(ARGP_PREFIX)/include" >> .clangd
	@echo "" >> .clangd
	@cat .clangd.template >> .clangd
else
	@cp .clangd.template .clangd
endif
	@echo ".clangd generated successfully"

$(ENGINE_LIB): $(ENGINE_OBJ)
	ar rcs $@ $^

$(MENGINE_BIN): $(MENGINE_OBJ) $(ENGINE_LIB)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests: $(TEST_BINARIES)

check: tests
	@echo "=== Running test suite ==="
	@set -e; \
	for t in $(TEST_BINARIES); do \
		echo "-- Running $$t"; \
		./$$t || exit 1; \
	done
	@echo "=== All tests passed ==="

$(TEST_BINARIES): %: %.c $(HELPERS_OBJ) $(ENGINE_LIB)
	$(CC) $(CFLAGS) -o $@ $< $(HELPERS_OBJ) $(ENGINE_LIB)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(ENGINE_OBJ) $(MENGINE_OBJ) $(HELPERS_OBJ) $(ENGINE_LIB) $(MENGINE_BIN) $(TEST_BINARIES)
	if [ "$(UNAME)" = "Darwin" ]; then \
		find . -name "*.dSYM" -type d -exec rm -rf {} +; \
	fi

.PHONY: all clean tests check clangd

clangd: .clangd
