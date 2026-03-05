# Compiler and flags
CC := gcc
MPICC := mpicc
CFLAGS := -Wall -Wextra -std=c99 -pedantic -Iinclude -O3
LDFLAGS := -lm
PTHREAD_FLAG := -lpthread

# Program names
SERIAL_PROGRAM := serial_sudoku_solver

# Allow specifying output directory from top-level Makefile
BINDIR := .
SERIAL_OUTPUT := $(BINDIR)/$(SERIAL_PROGRAM)

# Source files organization
SRC_DIR := src
INCLUDE_DIR := include
BUILD_DIR := build
PTHREADS_DIR := pthreads
SERIAL_DIR := serial
COMMON_DIR := common

# Serial objects
SERIAL_OBJS := $(BUILD_DIR)/$(SERIAL_DIR)/main.o \
    $(BUILD_DIR)/$(SERIAL_DIR)/solver.o \
	$(BUILD_DIR)/$(COMMON_DIR)/bitarray.o \
	$(BUILD_DIR)/$(COMMON_DIR)/sudoku_utils.o

# Handle DEBUG flag from parent Makefile
ifdef DEBUG
    CFLAGS += -g -DDEBUG
else
    # Default target (minimal debug info)
    CFLAGS += -g
endif

all: clean serial

serial: $(SERIAL_OUTPUT)

# Build configurations
debug: CFLAGS += -g -DDEBUG
debug: clean serial

# Make sure build directories exist
$(BUILD_DIR) $(BUILD_DIR)/$(COMMON_DIR) $(BUILD_DIR)/$(SERIAL_DIR):
	mkdir -p $@

# Pattern rule for common object files
$(BUILD_DIR)/$(COMMON_DIR)/%.o: $(SRC_DIR)/$(COMMON_DIR)/%.c | $(BUILD_DIR)/$(COMMON_DIR)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -MMD -MP -c $< -o $@

# Pattern rule for serial directory object files
$(BUILD_DIR)/$(SERIAL_DIR)/%.o: $(SRC_DIR)/$(SERIAL_DIR)/%.c | $(BUILD_DIR)/$(SERIAL_DIR)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -MMD -MP -c $< -o $@

# Link the serial program
$(SERIAL_OUTPUT): $(SERIAL_OBJS)
	@mkdir -p "$(BINDIR)"
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

# Clean build files
clean:
	rm -f $(BINDIR)/$(SERIAL_PROGRAM)
	rm -f $(SERIAL_OBJS) $(SERIAL_OBJS:.o=.d)
	rm -rf $(BUILD_DIR)

# Show help information
help:
	@echo "Constraint Propagation Sudoku Solver Makefile"
	@echo "--------------------------------------------"
	@echo "Available targets:"
	@echo "  all      - Build both serial and parallel versions (default)"
	@echo "  serial   - Build only the serial version"
	@echo "  debug    - Build with debug symbols and assertions"
	@echo "  clean    - Remove build files"
	@echo "  help     - Show this help message"

# Include generated dependency files
-include $(SERIAL_OBJS:.o=.d)

# Mark targets that don't produce files with their names
.PHONY: all serial clean help