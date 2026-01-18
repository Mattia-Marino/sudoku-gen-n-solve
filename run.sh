#!/bin/bash

TOTAL=$1
FILE_NAME=$2
CORES_PER_NODE=16
EXECUTABLE="./build/parallel_sudoku_solver_v3"

# Check arguments
if [ -z "$TOTAL" ] || [ -z "$FILE_NAME" ]; then
  echo "Usage: $0 <total_processes> <file_name>"
  echo "Example: $0 32 datasets/easy_50.txt"
  echo ""
  echo "Valid process counts:"
  echo "  Single node: 2-16 (1 MPI process + 1-15 threads)"
  echo "  Multi-node: 32, 48, 64, ... (multiples of 16)"
  exit 1
fi

# Check if executable exists
if [ ! -f "$EXECUTABLE" ]; then
  echo "Executable $EXECUTABLE not found. Running 'make'..."
  make
  
  if [ ! -f "$EXECUTABLE" ]; then
    echo "Error: Failed to build $EXECUTABLE"
    exit 1
  fi
  echo ""
fi

# Validate and calculate MPI processes and threads
if [ "$TOTAL" -lt 2 ]; then
  echo "Error: Total processes must be at least 2 (1 MPI process + 1 thread)"
  exit 1
fi

if [ "$TOTAL" -le "$CORES_PER_NODE" ]; then
  # Single node case: 2-16 total processes
  MPI_PROCS=1
  THREADS=$((TOTAL - 1))
else
  # Multi-node case: TOTAL must be a multiple of CORES_PER_NODE
  if [ $((TOTAL % CORES_PER_NODE)) -ne 0 ]; then
    echo "Error: For multi-node execution, total processes must be a multiple of $CORES_PER_NODE"
    echo "Valid values: 2-16 (single node), or 32, 48, 64, 80, ... (multi-node)"
    exit 1
  fi
  
  MPI_PROCS=$((TOTAL / CORES_PER_NODE))
  THREADS=$((TOTAL - MPI_PROCS))
fi

# Check if file exists
if [ ! -f "$FILE_NAME" ]; then
  echo "Error: File '$FILE_NAME' not found"
  exit 1
fi

echo "=========================================="
echo "Running Hybrid MPI+Pthreads Sudoku Solver"
echo "=========================================="
echo "  Total processes: $TOTAL"
echo "  MPI processes: $MPI_PROCS"
echo "  Threads: $THREADS"
echo "  Input file: $FILE_NAME"
echo "=========================================="
echo ""

mpirun --mca btl self,openib -np $MPI_PROCS --map-by node -machinefile machinefile.txt $EXECUTABLE -n $THREADS "$FILE_NAME"
