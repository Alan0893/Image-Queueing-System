# Makefile

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra


# Build target
all: cmp for while

# Linking and creating the printf executable
cmp: cmp.o
	$(CC) -o $@ $@.o

# Linking and creating the scanf executable
for: for.o
	$(CC) -o $@ $@.o

while: while.o
	$(CC) -o $@ $@.o

# Generic rule to compile .c files into .o files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up
clean:
	rm -f cmp for while *.o
