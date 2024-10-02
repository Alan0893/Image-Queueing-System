# Makefile

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra


# Build target
all: malloc-arr

# Linking and creating the arr executable
malloc-arr: malloc-arr.o
	$(CC) -o $@ $@.o

# Generic rule to compile .c files into .o files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up
clean:
	rm -f malloc-arr *.o
