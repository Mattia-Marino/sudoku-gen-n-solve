#!/bin/bash

# --- Configuration ---
EXECUTABLE="./build/parallel_sudoku_solver_v5"
MACHINEFILE="machinefile.txt"
CORES_PER_NODE=16

# --- Input Validation ---
if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <n_procs> <file_name>"
    exit 1
fi

TOTAL_INPUT=$1
FILE_NAME=$2

# --- Logic Calculation ---
# MPI_PROCS is the number of nodes we need to engage (1-4)
MPI_PROCS=$(( (TOTAL_INPUT + CORES_PER_NODE - 1) / CORES_PER_NODE ))

# Ensure we don't exceed the 4 physical nodes of the cluster
if [ "$MPI_PROCS" -gt 4 ]; then
    MPI_PROCS=4
fi

# THREADS is the total input minus the number of MPI processes 
THREADS=$(( TOTAL_INPUT - MPI_PROCS ))

# --- Execution ---
echo "Running on $MPI_PROCS nodes with $THREADS total threads..."

mpirun --mca btl self,openib \
    -np "$MPI_PROCS" \
    --map-by node \
    -machinefile "$MACHINEFILE" \
    "$EXECUTABLE" -n "$THREADS" "$FILE_NAME"

# mpirun -np "$MPI_PROCS" \
#     --map-by node \
#     -machinefile "$MACHINEFILE" \
#     "$EXECUTABLE" -n "$THREADS" "$FILE_NAME"