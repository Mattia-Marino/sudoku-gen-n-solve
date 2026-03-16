# Compiler and flags
CC := gcc
MPICC := mpicc
CFLAGS := -Wall -Wextra -std=c99 -pedantic -Iinclude -O3
LDFLAGS := -lm
PTHREAD_FLAG := -lpthread

# Program names
SERIAL_PROGRAM := serial_sudoku_solver
SERIAL_PROGRAM_V2 := serial_sudoku_solver_v2
SERIAL_PROGRAM_V3 := serial_sudoku_solver_v3
PARALLEL_PROGRAM_V1 := parallel_sudoku_solver_v1
PARALLEL_PROGRAM_V2 := parallel_sudoku_solver_v2

# Allow specifying output directory from top-level Makefile
BINDIR := .
SERIAL_OUTPUT := $(BINDIR)/$(SERIAL_PROGRAM)
SERIAL_OUTPUT_V2 := $(BINDIR)/$(SERIAL_PROGRAM_V2)
SERIAL_OUTPUT_V3 := $(BINDIR)/$(SERIAL_PROGRAM_V3)
PARALLEL_OUTPUT_V1 := $(BINDIR)/$(PARALLEL_PROGRAM_V1)
PARALLEL_OUTPUT_V2 := $(BINDIR)/$(PARALLEL_PROGRAM_V2)

# Source files organization
SRC_DIR := src
INCLUDE_DIR := include
BUILD_DIR := build
PTHREADS_DIR := pthreads
SERIAL_DIR := serial
SERIAL_V2_DIR := serial_v2
SERIAL_V3_DIR := serial_v3
COMMON_DIR := common
PARALLEL_DIR_V1 := parallel_v1
PARALLEL_DIR_V2 := parallel_v2

# Serial objects
SERIAL_OBJS := $(BUILD_DIR)/$(SERIAL_DIR)/main.o \
    $(BUILD_DIR)/$(SERIAL_DIR)/solver.o \
    $(BUILD_DIR)/$(COMMON_DIR)/bitarray.o \
    $(BUILD_DIR)/$(COMMON_DIR)/sudoku_utils.o

# Serial objects
SERIAL_OBJS_V2 := $(BUILD_DIR)/$(SERIAL_V2_DIR)/main.o \
    $(BUILD_DIR)/$(SERIAL_V2_DIR)/solver.o \
    $(BUILD_DIR)/$(COMMON_DIR)/bitarray.o \
    $(BUILD_DIR)/$(COMMON_DIR)/sudoku_utils.o

SERIAL_OBJS_V3 := $(BUILD_DIR)/$(SERIAL_V3_DIR)/main.o \
    $(BUILD_DIR)/$(SERIAL_V3_DIR)/solver.o \
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

PARALLEL_OBJS_V2 := $(BUILD_DIR)/$(PARALLEL_DIR_V2)/main.o \
    $(BUILD_DIR)/$(COMMON_DIR)/bitarray.o \
    $(BUILD_DIR)/$(COMMON_DIR)/sudoku_utils.o \
    $(BUILD_DIR)/$(PTHREADS_DIR)/pthread_groups.o \
    $(BUILD_DIR)/$(PTHREADS_DIR)/queue.o \
    $(BUILD_DIR)/$(PTHREADS_DIR)/commons.o \
    $(BUILD_DIR)/$(SERIAL_DIR)/solver.o \
    $(BUILD_DIR)/$(PARALLEL_DIR_V2)/solver.o


# Handle DEBUG flag from parent Makefile
ifdef DEBUG
    CFLAGS += -g -DDEBUG
else
    # Default target (minimal debug info)
    CFLAGS += -g
endif

all: clean serial serial_v2 serial_v3 parallel_v1 parallel_v2

serial: $(SERIAL_OUTPUT)
serial_v2: $(SERIAL_OUTPUT_V2)
serial_v3: $(SERIAL_OUTPUT_V3)
parallel_v1: $(PARALLEL_OUTPUT_V1)
parallel_v2: $(PARALLEL_OUTPUT_V2)

# Build configurations
debug: CFLAGS += -g -DDEBUG
debug: clean serial serial_v2 serial_v3 parallel_v1 parallel_v2

# Make sure build directories exist
$(BUILD_DIR) $(BUILD_DIR)/$(COMMON_DIR) $(BUILD_DIR)/$(PTHREADS_DIR) $(BUILD_DIR)/$(SERIAL_DIR) $(BUILD_DIR)/$(SERIAL_V2_DIR) $(BUILD_DIR)/$(SERIAL_V3_DIR) $(BUILD_DIR)/$(PARALLEL_DIR_V1) $(BUILD_DIR)/$(PARALLEL_DIR_V2):
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

# Pattern rule for serial_v2 directory object files
$(BUILD_DIR)/$(SERIAL_V2_DIR)/%.o: $(SRC_DIR)/$(SERIAL_V2_DIR)/%.c | $(BUILD_DIR)/$(SERIAL_V2_DIR)
	$(CC) $(CFLAGS) -g -DDEBUG -I$(INCLUDE_DIR) -MMD -MP -c $< -o $@
	
# Pattern rule for serial_v3 directory object files
$(BUILD_DIR)/$(SERIAL_V3_DIR)/%.o: $(SRC_DIR)/$(SERIAL_V3_DIR)/%.c | $(BUILD_DIR)/$(SERIAL_V3_DIR)
	$(CC) $(CFLAGS) -g -DDEBUG -I$(INCLUDE_DIR) -MMD -MP -c $< -o $@

# Pattern rule for parallel object files (V1)
$(BUILD_DIR)/$(PARALLEL_DIR_V1)/%.o: $(SRC_DIR)/$(PARALLEL_DIR_V1)/%.c | $(BUILD_DIR)/$(PARALLEL_DIR_V1)
	$(MPICC) $(PTHREAD_FLAG) $(CFLAGS) -I$(INCLUDE_DIR) -MMD -MP -c $< -o $@

# Pattern rule for parallel object files (V2)
$(BUILD_DIR)/$(PARALLEL_DIR_V2)/%.o: $(SRC_DIR)/$(PARALLEL_DIR_V2)/%.c | $(BUILD_DIR)/$(PARALLEL_DIR_V2)
	$(MPICC) $(PTHREAD_FLAG) $(CFLAGS) -I$(INCLUDE_DIR) -MMD -MP -c $< -o $@

# Link the serial program
$(SERIAL_OUTPUT): $(SERIAL_OBJS)
	@mkdir -p "$(BINDIR)"
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

# Link the serial_v2 program
$(SERIAL_OUTPUT_V2): $(SERIAL_OBJS_V2)
	@mkdir -p "$(BINDIR)"
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

# Link the serial_v3 program
$(SERIAL_OUTPUT_V3): $(SERIAL_OBJS_V3)
	@mkdir -p "$(BINDIR)"
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

# Link the parallel program (V1)
$(PARALLEL_OUTPUT_V1): $(PARALLEL_OBJS_V1)
	@mkdir -p "$(BINDIR)"
	$(MPICC) $(PTHREAD_FLAG) $(CFLAGS) $^ $(LDFLAGS) -o $@

# Link the parallel program (V2)
$(PARALLEL_OUTPUT_V2): $(PARALLEL_OBJS_V2)
	@mkdir -p "$(BINDIR)"
	$(MPICC) $(PTHREAD_FLAG) $(CFLAGS) $^ $(LDFLAGS) -o $@

# Clean build files
clean:
	rm -f $(BINDIR)/$(SERIAL_PROGRAM)
	rm -f $(BINDIR)/$(SERIAL_PROGRAM_V2)
	rm -f $(BINDIR)/$(PARALLEL_PROGRAM_V1)
	rm -f $(BINDIR)/$(PARALLEL_PROGRAM_V2)
	rm -f $(SERIAL_OBJS) $(SERIAL_OBJS:.o=.d)
	rm -f $(SERIAL_OBJS_V2) $(SERIAL_OBJS_V2:.o=.d)
	rm -f $(SERIAL_OBJS_V3) $(SERIAL_OBJS_V3:.o=.d)
	rm -f $(PARALLEL_OBJS_V1) $(PARALLEL_OBJS_V1:.o=.d)
	rm -f $(PARALLEL_OBJS_V2) $(PARALLEL_OBJS_V2:.o=.d)
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
-include $(SERIAL_OBJS_V2:.o=.d)
-include $(SERIAL_OBJS_V3:.o=.d)

# Mark targets that don't produce files with their names
.PHONY: all serial serial_v2 serial_v3 parallel_v1 parallel_v2 clean help
