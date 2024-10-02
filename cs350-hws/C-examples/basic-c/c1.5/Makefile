# Makefile

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra


# Build target
all: arr arr-func string

# Linking and creating the arr executable
arr: arr.o
	$(CC) -o $@ $@.o

# Linking and creating the arr-func executable
arr-func: arr-func.o
	$(CC) -o $@ $@.o

# Linking and creating the string executable
string: string.o
	$(CC) -o $@ $@.o

# Generic rule to compile .c files into .o files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up
clean:
	rm -f arr arr-func string *.o
