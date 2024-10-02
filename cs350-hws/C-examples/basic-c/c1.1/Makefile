# Makefile

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -g

# Target executable
TARGET = hello

# Build target
all: $(TARGET)

# Linking and creating the executable
$(TARGET): hello.o
	$(CC) -o $(TARGET) hello.o

# Compiling source file
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up
clean:
	rm -f hello hello.o