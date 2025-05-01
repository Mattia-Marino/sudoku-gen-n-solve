# Top-level Makefile

# Define directories and executables
GENERATOR_DIR := generator
GENERATOR_EXECUTABLE := sudoku_generator
ROOT_GEN_EXECUTABLE := $(GENERATOR_EXECUTABLE)

SOLVER_DIR := solver/algorithm_x
SERIAL_SOLVER_EXECUTABLE := serial_sudoku_solver
PARALLEL_SOLVER_EXECUTABLE := parallel_sudoku_solver
ROOT_SERIAL_SOL_EXECUTABLE := $(SERIAL_SOLVER_EXECUTABLE)
ROOT_PARALLEL_SOL_EXECUTABLE := $(PARALLEL_SOLVER_EXECUTABLE)

# Define phony targets
.PHONY: all generate solve clean debug test help

# Default target
all: generate solve

# Help target - must be first for default help behavior
help:
	@echo "Sudoku Generator and Solver Build System"
	@echo "========================================"
	@echo "Available targets:"
	@echo "  all      - Build generator and solver (default)"
	@echo "  generate - Build only the generator"
	@echo "  solve    - Build only the solver"
	@echo "  debug    - Build all components with debug flags"
	@echo "  clean    - Clean all build artifacts"
	@echo "  test     - Run tests (if available)"

# Build the generator
generate:
	@echo ""
	@echo "=============================================="
	@echo "BUILDING GENERATOR"
	@echo "=============================================="
	@echo "Directory: $(GENERATOR_DIR)"
	@echo "Output location: $(CURDIR)"
	@echo "=============================================="
	@echo ""

	$(MAKE) -C $(GENERATOR_DIR) || exit 1

	@echo ""
	@echo "Moving $(GENERATOR_DIR)/$(GENERATOR_EXECUTABLE) to $(CURDIR)"
	@echo ""

	mv $(GENERATOR_DIR)/$(GENERATOR_EXECUTABLE) "$(CURDIR)/"

	@echo ""
	@echo "=============================================="
	@echo "GENERATOR BUILD COMPLETED"
	@echo "=============================================="
	@echo ""

# Build the solver
solve:
	@echo ""
	@echo "=============================================="
	@echo "BUILDING SOLVER"
	@echo "=============================================="
	@echo "Directory: $(SOLVER_DIR)"
	@echo "Output location: $(CURDIR)"
	@echo "=============================================="
	@echo ""

	$(MAKE) -C $(SOLVER_DIR) || exit 1

	@echo ""
	@echo "Moving solver executables to $(CURDIR)"
	@echo ""

	mv $(SOLVER_DIR)/$(SERIAL_SOLVER_EXECUTABLE) "$(CURDIR)/"
	mv $(SOLVER_DIR)/$(PARALLEL_SOLVER_EXECUTABLE) "$(CURDIR)/"

	@echo ""
	@echo "=============================================="
	@echo "SOLVER BUILD COMPLETED"
	@echo "=============================================="
	@echo ""

# Debug build of all components
debug:
	@echo ""
	@echo "=============================================="
	@echo "BUILDING GENERATOR (DEBUG MODE)"
	@echo "=============================================="
	@echo ""

	$(MAKE) -C $(GENERATOR_DIR) DEBUG=1 || exit 1

	@echo ""
	@echo "Moving $(GENERATOR_DIR)/$(GENERATOR_EXECUTABLE) to $(CURDIR)"
	@echo ""

	mv $(GENERATOR_DIR)/$(GENERATOR_EXECUTABLE) "$(CURDIR)/"

	@echo ""
	@echo "=============================================="
	@echo "BUILDING SOLVER (DEBUG MODE)"
	@echo "=============================================="
	@echo ""

	$(MAKE) -C $(SOLVER_DIR) DEBUG=1 || exit 1

	@echo ""
	@echo "Moving solver executables to $(CURDIR)"
	@echo ""

	mv $(SOLVER_DIR)/$(SERIAL_SOLVER_EXECUTABLE) "$(CURDIR)/"
	mv $(SOLVER_DIR)/$(PARALLEL_SOLVER_EXECUTABLE) "$(CURDIR)/"

	@echo ""
	@echo "=============================================="
	@echo "DEBUG BUILD COMPLETED"
	@echo "=============================================="
	@echo ""

# Run tests (placeholder - create test scripts as needed)
test: all
	@echo "Running tests..."
    # Add test execution commands here
    # ./run_tests.sh

# Clean all build artifacts
clean:
	@echo ""
	@echo "=============================================="
	@echo "CLEANING PROJECT"
	@echo "=============================================="
	@echo ""

	$(MAKE) -C $(GENERATOR_DIR) clean
	$(MAKE) -C $(SOLVER_DIR) clean
	rm -f $(ROOT_GEN_EXECUTABLE) $(ROOT_SERIAL_SOL_EXECUTABLE) $(ROOT_PARALLEL_SOL_EXECUTABLE)

	@echo ""
	@echo "=============================================="
	@echo "CLEAN COMPLETED"
	@echo "=============================================="
	@echo ""
