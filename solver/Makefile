# Compiler and flags
CC := gcc
CFLAGS := -Wall -Werror -std=c89
LDFLAGS := -lm

# Program name
PROGRAM := serial_solver

# Source files organization
SRC_DIR := src
BUILD_DIR := build
OBJS := $(BUILD_DIR)/main.o \
	$(BUILD_DIR)/sudoku.o \
	$(BUILD_DIR)/solver.o \
	$(BUILD_DIR)/Dancing-Links/dancing-links.o

# Default target (debug build)
all: CFLAGS += -g
all: $(PROGRAM)

# Build configurations
debug: CFLAGS += -g -DDEBUG
debug: clean $(PROGRAM)

release: CFLAGS += -O3 -DNDEBUG
release: clean $(PROGRAM)

# Make sure build directories exist
$(BUILD_DIR) $(BUILD_DIR)/Dancing-Links:
	mkdir -p $@

# Pattern rule for object files with automatic dependency generation
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR) $(BUILD_DIR)/Dancing-Links
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# Link the program
$(PROGRAM): $(OBJS)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

# Install the program
install: $(PROGRAM)
	install -d $(DESTDIR)/usr/local/bin
	install -m 755 $(PROGRAM) $(DESTDIR)/usr/local/bin

# Uninstall the program
uninstall:
	rm -f $(DESTDIR)/usr/local/bin/$(PROGRAM)

# Clean build files
clean:
	rm -f $(PROGRAM) $(OBJS) $(OBJS:.o=.d)
	rm -rf $(BUILD_DIR)

# Show help information
help:
	@echo "Solver-X Makefile"
	@echo "------------------"
	@echo "Available targets:"
	@echo "  all      - Build the program with debug symbols (default)"
	@echo "  debug    - Same as all but cleans first"
	@echo "  release  - Build with optimizations for production use"
	@echo "  clean    - Remove build files"
	@echo "  install  - Install the program to /usr/local/bin"
	@echo "  uninstall- Remove the installed program"
	@echo "  help     - Show this help message"

# Include generated dependency files
-include $(OBJS:.o=.d)

# Mark targets that don't produce files with their names
.PHONY: all debug release clean install uninstall help