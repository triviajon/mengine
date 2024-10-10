CC = clang
CFLAGS = -Wall -Wextra -std=c11 -D_GNU_SOURCE -g -O0
SRC_DIR = src
KERNEL_DIR = $(SRC_DIR)/kernel
OBJ = $(KERNEL_DIR)/doubly_linked_list.o $(KERNEL_DIR)/context.o $(KERNEL_DIR)/expression.o $(KERNEL_DIR)/utils.o $(KERNEL_DIR)/beta_reduction.o $(KERNEL_DIR)/alpha_equivalent.o $(KERNEL_DIR)/inductive.o
EXEC = main

all: $(OBJ) $(SRC_DIR)/main.o $(EXEC)

$(EXEC): $(OBJ) $(SRC_DIR)/main.o
	$(CC) $(CFLAGS) $(OBJ) $(SRC_DIR)/main.o -o $(EXEC)

$(SRC_DIR)/main.o: $(SRC_DIR)/main.c $(KERNEL_DIR)/expression.h $(KERNEL_DIR)/beta_reduction.h
	$(CC) $(CFLAGS) -c $(SRC_DIR)/main.c -o $(SRC_DIR)/main.o

$(KERNEL_DIR)/doubly_linked_list.o: $(KERNEL_DIR)/doubly_linked_list.c $(KERNEL_DIR)/doubly_linked_list.h
	$(CC) $(CFLAGS) -c $(KERNEL_DIR)/doubly_linked_list.c -o $@

$(KERNEL_DIR)/context.o: $(KERNEL_DIR)/context.c $(KERNEL_DIR)/context.h
	$(CC) $(CFLAGS) -c $(KERNEL_DIR)/context.c -o $@

$(KERNEL_DIR)/inductive.o: $(KERNEL_DIR)/inductive.c $(KERNEL_DIR)/inductive.h $(KERNEL_DIR)/doubly_linked_list.h
	$(CC) $(CFLAGS) -c $(KERNEL_DIR)/inductive.c -o $@

$(KERNEL_DIR)/utils.o: $(KERNEL_DIR)/utils.c $(KERNEL_DIR)/utils.h $(KERNEL_DIR)/expression.h
	$(CC) $(CFLAGS) -c $(KERNEL_DIR)/utils.c -o $@

$(KERNEL_DIR)/alpha_equivalent.o: $(KERNEL_DIR)/alpha_equivalent.c $(KERNEL_DIR)/alpha_equivalent.h $(KERNEL_DIR)/expression.h
	$(CC) $(CFLAGS) -c $(KERNEL_DIR)/alpha_equivalent.c -o $@

$(KERNEL_DIR)/expression.o: $(KERNEL_DIR)/expression.c $(KERNEL_DIR)/expression.h $(KERNEL_DIR)/doubly_linked_list.h
	$(CC) $(CFLAGS) -c $(KERNEL_DIR)/expression.c -o $@

$(KERNEL_DIR)/beta_reduction.o: $(KERNEL_DIR)/beta_reduction.c $(KERNEL_DIR)/beta_reduction.h $(KERNEL_DIR)/expression.h
	$(CC) $(CFLAGS) -c $(KERNEL_DIR)/beta_reduction.c -o $@

clean:
	rm -f $(OBJ) $(SRC_DIR)/main.o $(EXEC)
