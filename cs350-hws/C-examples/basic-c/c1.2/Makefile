# Makefile

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -g

# Source files for each target
PRINTF_SRC = print.c
SCANF_SRC = scan.c

# Object files for each target
PRINTF_OBJ = $(PRINTF_SRC:.c=.o)
SCANF_OBJ = $(SCANF_SRC:.c=.o)

# Build target
all: print scan

# Linking and creating the printf executable
printf: $(PRINTF_OBJ)
	$(CC) -o print $(PRINTF_OBJ)

# Linking and creating the scanf executable
scanf: $(SCANF_OBJ)
	$(CC) -o scan $(SCANF_OBJ)

# Generic rule to compile .c files into .o files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up
clean:
	rm -f print scan $(PRINTF_OBJ) $(SCANF_OBJ)
