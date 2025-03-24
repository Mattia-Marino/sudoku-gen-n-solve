# Sudoku Generator and Solver

Project realised for the course of High Performance Parallel Computing, held by professor Umberto Villano at University of Sannio for the Academic Year 2024/2025.

This project aims to implement a parallel sudoku solver using MPI and pthreads, and compare its performance to a serial version of the code to evaluate. To make it more testable and accessible, we also included a sudoku generator to create some tables with which test the system. The generator is based on a sudoku solver realised by Dimitris Boutzounis, where he implemented Donald Knuth's Algorithm X as a sudoku solver.

### Prerequisites
- C Compiler (GCC recommended)
- Make
  
### Building the Project
It is possible to build the entire project by using the main Makefile, by simply running `make` from a terminal in the root directory of the project. This will generate two executables: `sudoku_generator` and `serial_sudoku_solver`.

### Running the Sudoku Generator
To generate a Sudoku puzzle, after building with `make`, run:

```
./sudoku_generator [size]
```

Where `[size]` is the size of the desired puzzle. It must be a perfect square, such as 4, 9, 16, 25, ...

The output will be a text file called `output_[size].txt`

An example of `output_9.txt`:
```
9
0 0 0 5 0 0 0 4 0 
1 0 4 0 8 0 2 0 0 
0 0 0 0 0 0 3 0 0 
2 0 0 0 0 0 0 6 3 
3 7 0 2 0 0 9 0 0 
0 9 0 0 1 0 0 0 0 
0 2 0 4 0 0 0 0 8 
7 0 0 0 2 0 6 0 0 
8 0 1 6 0 0 0 7 0 
```

The first line here is the size of the board, and below it there is the board to solve. The empty cells are represented with 0.

### Running the Solver
To solve a Sudoku puzzle using the serial solver, run:
```
./serial_sudoku_solver [filename]
```

Where `[filename]` is the name of the file containing the generated sudoku puzzle that we want to solve.
