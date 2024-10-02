# Makefile

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra


# Build target
all: linked_list ptr_to_struct arr_of_struct struct_review struct_w_ptr

# Linking and creating the arr executable
linked_list: linked_list.o
	$(CC) -o $@ $@.o


arr_of_struct: arr_of_struct.o
	$(CC) -o $@ $@.o

struct_review: struct_review.o
	$(CC) -o $@ $@.o

struct_w_ptr: struct_w_ptr.o
	$(CC) -o $@ $@.o

# Linking and creating the ptr_to_struct executable
ptr_to_struct: ptr_to_struct.o
	$(CC) -o $@ $@.o

# Generic rule to compile .c files into .o files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up
clean:
	rm -f linked_list ptr_to_struct arr_of_struct struct_review struct_w_ptr *.o
