# Top-level Makefile

GENERATOR_DIR = generator
GENERATOR_EXECUTABLE = sudoku_generator
ROOT_EXECUTABLE = $(GENERATOR_EXECUTABLE) # or ./$(GENERATOR_EXECUTABLE) if needed.

TARGETS = generate

all: ${TARGETS}


generate:
	$(MAKE) -C $(GENERATOR_DIR)
	mv $(GENERATOR_DIR)/$(GENERATOR_EXECUTABLE) $(ROOT_EXECUTABLE)


clean:
	$(MAKE) -C $(GENERATOR_DIR) clean
	rm -f $(ROOT_EXECUTABLE)