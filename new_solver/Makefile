# Compiler and flags
CC := gcc
MPICC := mpicc
CFLAGS := -Wall -Wextra -std=c99 -pedantic -Iinclude -O3
LDFLAGS := -lm
PTHREAD_FLAG := -lpthread

# Program names
SERIAL_PROGRAM := serial_sudoku_solver
PARALLEL_PROGRAM_V1 := parallel_sudoku_solver_v1

# Allow specifying output directory from top-level Makefile
BINDIR := .
SERIAL_OUTPUT := $(BINDIR)/$(SERIAL_PROGRAM)
PARALLEL_OUTPUT_V1 := $(BINDIR)/$(PARALLEL_PROGRAM_V1)

# Source files organization
SRC_DIR := src
INCLUDE_DIR := include
BUILD_DIR := build
PTHREADS_DIR := pthreads
SERIAL_DIR := serial
COMMON_DIR := common
PARALLEL_DIR_V1 := parallel_v1

# Serial objects
SERIAL_OBJS := $(BUILD_DIR)/$(SERIAL_DIR)/main.o \
    $(BUILD_DIR)/$(SERIAL_DIR)/solver.o \
    $(BUILD_DIR)/$(COMMON_DIR)/bitarray.o \
    $(BUILD_DIR)/$(COMMON_DIR)/sudoku_utils.o

# Parallel objects
PARALLEL_OBJS_V1 := $(BUILD_DIR)/$(PARALLEL_DIR_V1)/main.o \
    $(BUILD_DIR)/$(COMMON_DIR)/bitarray.o \
    $(BUILD_DIR)/$(COMMON_DIR)/sudoku_utils.o \
    $(BUILD_DIR)/$(PTHREADS_DIR)/pthread_groups.o \
    $(BUILD_DIR)/$(PTHREADS_DIR)/queue.o \
    $(BUILD_DIR)/$(PTHREADS_DIR)/commons.o \
    $(BUILD_DIR)/$(SERIAL_DIR)/solver.o


# Handle DEBUG flag from parent Makefile
ifdef DEBUG
    CFLAGS += -g -DDEBUG
else
    # Default target (minimal debug info)
    CFLAGS += -g
endif

all: clean serial parallel_v1

serial: $(SERIAL_OUTPUT)
parallel_v1: $(PARALLEL_OUTPUT_V1)

# Build configurations
debug: CFLAGS += -g -DDEBUG
debug: clean serial parallel_v1

# Make sure build directories exist
$(BUILD_DIR) $(BUILD_DIR)/$(COMMON_DIR) $(BUILD_DIR)/$(PTHREADS_DIR) $(BUILD_DIR)/$(SERIAL_DIR) $(BUILD_DIR)/$(PARALLEL_DIR_V1):
	mkdir -p $@

# Pattern rule for common object files
$(BUILD_DIR)/$(COMMON_DIR)/%.o: $(SRC_DIR)/$(COMMON_DIR)/%.c | $(BUILD_DIR)/$(COMMON_DIR)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -MMD -MP -c $< -o $@

# Pattern rule for pthreads object files
$(BUILD_DIR)/$(PTHREADS_DIR)/%.o: $(SRC_DIR)/pthreads/%.c | $(BUILD_DIR)/$(PTHREADS_DIR)
	$(CC) $(PTHREAD_FLAG) $(CFLAGS) -I$(INCLUDE_DIR) -MMD -MP -c $< -o $@

# Pattern rule for serial directory object files
$(BUILD_DIR)/$(SERIAL_DIR)/%.o: $(SRC_DIR)/$(SERIAL_DIR)/%.c | $(BUILD_DIR)/$(SERIAL_DIR)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -MMD -MP -c $< -o $@

# Pattern rule for parallel object files (V1)
$(BUILD_DIR)/$(PARALLEL_DIR_V1)/%.o: $(SRC_DIR)/$(PARALLEL_DIR_V1)/%.c | $(BUILD_DIR)/$(PARALLEL_DIR_V1)
	$(MPICC) $(PTHREAD_FLAG) $(CFLAGS) -I$(INCLUDE_DIR) -MMD -MP -c $< -o $@

# Link the serial program
$(SERIAL_OUTPUT): $(SERIAL_OBJS)
	@mkdir -p "$(BINDIR)"
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

# Link the parallel program (V1)
$(PARALLEL_OUTPUT_V1): $(PARALLEL_OBJS_V1)
	@mkdir -p "$(BINDIR)"
	$(MPICC) $(PTHREAD_FLAG) $(CFLAGS) $^ $(LDFLAGS) -o $@

# Clean build files
clean:
	rm -f $(BINDIR)/$(SERIAL_PROGRAM)
	rm -f $(BINDIR)/$(PARALLEL_PROGRAM_V1)
	rm -f $(SERIAL_OBJS) $(SERIAL_OBJS:.o=.d)
	rm -f $(PARALLEL_OBJS_V1) $(PARALLEL_OBJS_V1:.o=.d)
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
.PHONY: all serial parallel_v1 clean help