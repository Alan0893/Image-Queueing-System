# Makefile

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra


# Build target
all: scope

# Linking and creating the arr executable
scope: scope.o
	$(CC) -o $@ $@.o

# Generic rule to compile .c files into .o files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up
clean:
	rm -f scope *.o
