# Compiler and flags
CC = gcc
CFLAGS = -std=c99 \
         -D_POSIX_C_SOURCE=200809L \
         -D_XOPEN_SOURCE=700 \
         -Wall -Wextra -Werror \
         -Wno-unused-parameter \
         -fno-asm \
         -Iinclude

# Target executable name
TARGET = shell.out

# Source files (inside src/)
SOURCES = src/main.c \
          src/signals.c \
          src/jobs.c \
          src/builtin_commands.c \
          src/parser.c \
          src/executor.c \
          src/logger.c \
          src/utils.c

# Object files (automatically generated from source files)
OBJECTS = $(SOURCES:.c=.o)

# Header files for dependencies
HEADERS = include/shell.h \
          include/signals.h \
          include/jobs.h \
          include/builtin_commands.h \
          include/parser.h \
          include/executor.h \
          include/logger.h \
          include/utils.h

# Default target
all: $(TARGET)

# Link object files to create executable
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS)

# Compile source files to object files
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(TARGET)

# Phony targets
.PHONY: all clean install uninstall

# Install target (optional)
install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

# Uninstall target (optional)
uninstall:
	rm -f /usr/local/bin/$(TARGET)
