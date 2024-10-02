# Makefile

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra


# Build target
all: 2d-arr-friendly 2d-arr 2d-arr-ptr 2d-arr-mem-eff dynamic-arr static-arr

# Linking and creating the arr executable
2d-arr-friendly: 2d-arr-friendly.o
	$(CC) -o $@ $@.o

# Linking and creating the 2d-arr executable
2d-arr: 2d-arr.o
	$(CC) -o $@ $@.o

# Linking and creating the 2d-arr executable
2d-arr-mem-eff: 2d-arr-mem-eff.o
	$(CC) -o $@ $@.o

dynamic-arr: dynamic-arr.o
	$(CC) -o $@ $@.o

static-arr: static-arr.o
	$(CC) -o $@ $@.o

# Linking and creating the 2d-arr-ptr executable
2d-arr-ptr: 2d-arr-ptr.o
	$(CC) -o $@ $@.o

# Generic rule to compile .c files into .o files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up
clean:
	rm -f 2d-arr-friendly 2d-arr 2d-arr-ptr 2d-arr-mem-eff dynamic-arr static-arr *.o
