# Compiler and flags
CC := gcc
MPICC := mpicc
CFLAGS := -Wall -Werror -std=c89
LDFLAGS := -lm

# Program names
SERIAL_PROGRAM := serial_sudoku_solver
PARALLEL_PROGRAM := parallel_sudoku_solver

# Allow specifying output directory from top-level Makefile
BINDIR := .
SERIAL_OUTPUT := $(BINDIR)/$(SERIAL_PROGRAM)
PARALLEL_OUTPUT := $(BINDIR)/$(PARALLEL_PROGRAM)

# Source files organization
SRC_DIR := src
BUILD_DIR := build
SERIAL_DIR := serial
PARALLEL_DIR := parallel

# Serial objects
SERIAL_OBJS := $(BUILD_DIR)/$(SERIAL_DIR)/main.o \
    $(BUILD_DIR)/sudoku.o \
    $(BUILD_DIR)/solver_comm.o \
    $(BUILD_DIR)/$(SERIAL_DIR)/solver_serial.o \
    $(BUILD_DIR)/Dancing-Links/dancing-links.o

# Parallel objects
PARALLEL_OBJS := $(BUILD_DIR)/$(PARALLEL_DIR)/main.o \
    $(BUILD_DIR)/sudoku.o \
    $(BUILD_DIR)/solver_comm.o \
    $(BUILD_DIR)/$(PARALLEL_DIR)/solver_parallel.o \
    $(BUILD_DIR)/Dancing-Links/dancing-links.o

# Handle DEBUG flag from parent Makefile
ifdef DEBUG
    CFLAGS += -g -DDEBUG
else
    # Default target (minimal debug info)
    CFLAGS += -g
endif

all: serial parallel

serial: $(SERIAL_OUTPUT)
parallel: $(PARALLEL_OUTPUT)

# Build configurations
debug: CFLAGS += -g -DDEBUG
debug: clean serial parallel

release: CFLAGS += -O3 -DNDEBUG
release: clean serial parallel

# Make sure build directories exist
$(BUILD_DIR) $(BUILD_DIR)/Dancing-Links $(BUILD_DIR)/$(SERIAL_DIR) $(BUILD_DIR)/$(PARALLEL_DIR):
	mkdir -p $@

# Pattern rule for serial object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR) $(BUILD_DIR)/Dancing-Links $(BUILD_DIR)/$(SERIAL_DIR)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# Pattern rule for parallel object files
$(BUILD_DIR)/$(PARALLEL_DIR)/%.o: $(SRC_DIR)/$(PARALLEL_DIR)/%.c | $(BUILD_DIR)/$(PARALLEL_DIR)
	$(MPICC) $(CFLAGS) -MMD -MP -c $< -o $@

# Link the serial program
$(SERIAL_OUTPUT): $(SERIAL_OBJS)
	@mkdir -p "$(BINDIR)"
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

# Link the parallel program
$(PARALLEL_OUTPUT): $(PARALLEL_OBJS)
	@mkdir -p "$(BINDIR)"
	$(MPICC) $(CFLAGS) $^ $(LDFLAGS) -o $@

# Install the programs
install: $(SERIAL_OUTPUT) $(PARALLEL_OUTPUT)
	install -d $(DESTDIR)/usr/local/bin
	install -m 755 $(SERIAL_OUTPUT) $(DESTDIR)/usr/local/bin/$(SERIAL_PROGRAM)
	install -m 755 $(PARALLEL_OUTPUT) $(DESTDIR)/usr/local/bin/$(PARALLEL_PROGRAM)

# Uninstall the programs
uninstall:
	rm -f $(DESTDIR)/usr/local/bin/$(SERIAL_PROGRAM)
	rm -f $(DESTDIR)/usr/local/bin/$(PARALLEL_PROGRAM)

# Clean build files
clean:
	rm -f $(BINDIR)/$(SERIAL_PROGRAM) $(BINDIR)/$(PARALLEL_PROGRAM)
	rm -f $(SERIAL_OBJS) $(PARALLEL_OBJS) $(SERIAL_OBJS:.o=.d) $(PARALLEL_OBJS:.o=.d)
	rm -rf $(BUILD_DIR)

# Show help information
help:
	@echo "Sudoku Solver Makefile"
	@echo "---------------------"
	@echo "Available targets:"
	@echo "  all      - Build both serial and parallel versions (default)"
	@echo "  serial   - Build only the serial version"
	@echo "  parallel - Build only the parallel version"
	@echo "  debug    - Build with debug symbols and assertions"
	@echo "  release  - Build with optimizations for production use"
	@echo "  clean    - Remove build files"
	@echo "  install  - Install the programs to /usr/local/bin"
	@echo "  uninstall- Remove the installed programs"
	@echo "  help     - Show this help message"

# Include generated dependency files
-include $(SERIAL_OBJS:.o=.d)
-include $(PARALLEL_OBJS:.o=.d)

# Mark targets that don't produce files with their names
.PHONY: all serial parallel debug release clean install uninstall help