# Compiler and flags
CC := gcc
CFLAGS := -Wall -Werror -std=c99
LDFLAGS := -lm

# Program name
PROGRAM := sudoku_generator

# Allow specifying output directory from top-level Makefile
BINDIR := .
OUTPUT := $(BINDIR)/$(PROGRAM)

# Source files organization
SRC_DIR := src
BUILD_DIR := build
OBJS := $(BUILD_DIR)/main.o \
	$(BUILD_DIR)/sudoku.o \
	$(BUILD_DIR)/solver.o \
	$(BUILD_DIR)/Dancing-Links/dancing-links.o

# Handle DEBUG flag from parent Makefile
ifdef DEBUG
    CFLAGS += -g -DDEBUG
else
    # Default target (minimal debug info)
    CFLAGS += -g
endif

all: $(OUTPUT)

# Build configurations
debug: CFLAGS += -g -DDEBUG
debug: clean $(OUTPUT)

release: CFLAGS += -O3 -DNDEBUG
release: clean $(OUTPUT)

# Make sure build directories exist
$(BUILD_DIR) $(BUILD_DIR)/Dancing-Links:
	mkdir -p $@

# Pattern rule for object files with automatic dependency generation
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR) $(BUILD_DIR)/Dancing-Links
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# Link the program
$(OUTPUT): $(OBJS)
	@mkdir -p "$(BINDIR)"
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

# Install the program
install: $(OUTPUT)
	install -d $(DESTDIR)/usr/local/bin
	install -m 755 $(OUTPUT) $(DESTDIR)/usr/local/bin/$(PROGRAM)

# Uninstall the program
uninstall:
	rm -f $(DESTDIR)/usr/local/bin/$(PROGRAM)

# Clean build files
clean:
	rm -f $(BINDIR)/$(PROGRAM) $(OBJS) $(OBJS:.o=.d)
	rm -rf $(BUILD_DIR)

# Show help information
help:
	@echo "Serial Solver Makefile"
	@echo "---------------------"
	@echo "Available targets:"
	@echo "  all      - Build the program (default)"
	@echo "  debug    - Build with debug symbols and assertions"
	@echo "  release  - Build with optimizations for production use"
	@echo "  clean    - Remove build files"
	@echo "  install  - Install the program to /usr/local/bin"
	@echo "  uninstall- Remove the installed program"
	@echo "  help     - Show this help message"

# Include generated dependency files
-include $(OBJS:.o=.d)

# Mark targets that don't produce files with their names
.PHONY: all debug release clean install uninstall help