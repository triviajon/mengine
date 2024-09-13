CC = clang
CFLAGS = -Wall -Wextra -std=c11 -D_GNU_SOURCE
SRC_DIR = src
OBJ = $(SRC_DIR)/doubly_linked_list.o $(SRC_DIR)/env.o $(SRC_DIR)/expression.o $(SRC_DIR)/beta_reduce.o $(SRC_DIR)/typecheck.o

all: $(OBJ)

$(SRC_DIR)/doubly_linked_list.o: $(SRC_DIR)/doubly_linked_list.c $(SRC_DIR)/doubly_linked_list.h
	$(CC) $(CFLAGS) -c $(SRC_DIR)/doubly_linked_list.c -o $@

$(SRC_DIR)/env.o: $(SRC_DIR)/env.c $(SRC_DIR)/env.h
	$(CC) $(CFLAGS) -c $(SRC_DIR)/env.c -o $@

$(SRC_DIR)/expression.o: $(SRC_DIR)/expression.c $(SRC_DIR)/expression.h $(SRC_DIR)/doubly_linked_list.h
	$(CC) $(CFLAGS) -c $(SRC_DIR)/expression.c -o $@

$(SRC_DIR)/beta_reduce.o: $(SRC_DIR)/beta_reduce.c $(SRC_DIR)/beta_reduce.h $(SRC_DIR)/expression.h
	$(CC) $(CFLAGS) -c $(SRC_DIR)/beta_reduce.c -o $@

$(SRC_DIR)/typecheck.o: $(SRC_DIR)/typecheck.c $(SRC_DIR)/expression.h $(SRC_DIR)/env.h
	$(CC) $(CFLAGS) -c $(SRC_DIR)/typecheck.c -o $@

clean:
	rm -f $(OBJ)
