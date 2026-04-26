CC = clang
# Optional compile-time overrides, e.g.:
# make TUNE_FLAGS="-DMAP_INITIAL_CAPACITY=32 -DHCMAP_INITIAL_CAPACITY=2048"

# Substitution toggle
# - top-down substitution (default): TUNE_SUBST_TOPDOWN=1
# - bottom-up/uplink based substitution: TUNE_SUBST_TOPDOWN=0
TUNE_SUBST_TOPDOWN ?= 1

# Order data structure toggle:
# - tag-range relabeling (default): TUNE_USE_RELABELING=1
# - linked-list: TUNE_USE_RELABELING=0
TUNE_USE_RELABELING ?= 1

TUNE_FLAGS ?=

ifeq ($(TUNE_SUBST_TOPDOWN), 1)
	TUNE_FLAGS += -DTUNE_SUBST_TOPDOWN=1
endif

ifeq ($(TUNE_USE_RELABELING), 1)
	TUNE_FLAGS += -DORDER_USE_RELABELING=1
endif

CFLAGS = -Wall -Wextra -O0 -g -march=native -I. $(TUNE_FLAGS)
LDFLAGS =

ENGINE_SRC := $(shell find src -name '*.c' ! -name 'main.c')
ENGINE_OBJ := $(ENGINE_SRC:.c=.o)
ENGINE_LIB := libmengine.a

MENGINE_BIN := mengine
MENGINE_SRC := src/main.c
MENGINE_OBJ := $(MENGINE_SRC:.c=.o)

TEST_SRC := $(shell find tests -name '*.c' ! -path 'tests/helpers/*' ! -name 'test_driver.c')
TEST_OBJ := $(TEST_SRC:.c=.o)
TEST_DRIVER_SRC := tests/test_driver.c
TEST_DRIVER := tests/test_driver

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

tests: $(TEST_DRIVER)

check: tests
	@./$(TEST_DRIVER)

examples: $(MENGINE_BIN)
	@echo "Running all examples with mengine..."
	@set -e; \
	for example in examples/*.me; do \
		echo "-- Running $$example"; \
		./$(MENGINE_BIN) "$$example" || exit 1; \
	done
	@echo "=== All examples ran successfully ==="

install: $(MENGINE_BIN)
	@echo "Installing mengine..."
	sudo install -m 755 $(MENGINE_BIN) /usr/bin/$(MENGINE_BIN)

uninstall:
	@echo "Uninstalling mengine..."
	sudo rm -f /usr/bin/$(MENGINE_BIN)

$(TEST_DRIVER): $(TEST_DRIVER_SRC) $(TEST_OBJ) $(HELPERS_OBJ) $(ENGINE_LIB)
	$(CC) $(CFLAGS) -o $@ $(TEST_DRIVER_SRC) $(TEST_OBJ) $(HELPERS_OBJ) $(ENGINE_LIB) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(ENGINE_OBJ) $(MENGINE_OBJ) $(HELPERS_OBJ) $(TEST_OBJ) $(ENGINE_LIB) $(MENGINE_BIN) $(TEST_DRIVER)
	if [ "$(UNAME)" = "Darwin" ]; then \
		find . -name "*.dSYM" -type d -exec rm -rf {} +; \
	fi

clangd: .clangd

.PHONY: all clean tests check install uninstall clangd