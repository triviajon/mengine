CC = clang
CFLAGS = -Wall -Wextra -std=c11 -D_GNU_SOURCE -g
SRC_DIR = src
OBJ = $(SRC_DIR)/doubly_linked_list.o $(SRC_DIR)/context.o $(SRC_DIR)/expression.o $(SRC_DIR)/utils.o $(SRC_DIR)/beta_reduction.o $(SRC_DIR)/alpha_equivalent.o
EXEC = main

all: $(OBJ) $(SRC_DIR)/main.o $(EXEC)

$(EXEC): $(OBJ) $(SRC_DIR)/main.o
	$(CC) $(CFLAGS) $(OBJ) $(SRC_DIR)/main.o -o $(EXEC)

$(SRC_DIR)/main.o: $(SRC_DIR)/main.c $(SRC_DIR)/expression.h $(SRC_DIR)/beta_reduction.h
	$(CC) $(CFLAGS) -c $(SRC_DIR)/main.c -o $(SRC_DIR)/main.o

$(SRC_DIR)/doubly_linked_list.o: $(SRC_DIR)/doubly_linked_list.c $(SRC_DIR)/doubly_linked_list.h
	$(CC) $(CFLAGS) -c $(SRC_DIR)/doubly_linked_list.c -o $@

$(SRC_DIR)/context.o: $(SRC_DIR)/context.c $(SRC_DIR)/context.h
	$(CC) $(CFLAGS) -c $(SRC_DIR)/context.c -o $@

$(SRC_DIR)/utils.o: $(SRC_DIR)/utils.c $(SRC_DIR)/utils.h $(SRC_DIR)/expression.h
	$(CC) $(CFLAGS) -c $(SRC_DIR)/utils.c -o $@

$(SRC_DIR)/alpha_equivalent.o: $(SRC_DIR)/alpha_equivalent.c $(SRC_DIR)/alpha_equivalent.h $(SRC_DIR)/expression.h
	$(CC) $(CFLAGS) -c $(SRC_DIR)/alpha_equivalent.c -o $@

$(SRC_DIR)/expression.o: $(SRC_DIR)/expression.c $(SRC_DIR)/expression.h $(SRC_DIR)/doubly_linked_list.h
	$(CC) $(CFLAGS) -c $(SRC_DIR)/expression.c -o $@

$(SRC_DIR)/beta_reduction.o: $(SRC_DIR)/beta_reduction.c $(SRC_DIR)/beta_reduction.h $(SRC_DIR)/expression.h
	$(CC) $(CFLAGS) -c $(SRC_DIR)/beta_reduction.c -o $@

clean:
	rm -f $(OBJ) $(SRC_DIR)/main.o $(EXEC)
