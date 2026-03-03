CC := gcc
SRC_PATH := src
BUILD_PATH := build
COMMON_PATH := common
CFLAGS += -Wall -std=c99 -pedantic -lpthread -Iinclude

all: init build
clean: 
	rm -rf ${BUILD_PATH}
# run_tests: build test run_test clean

init: 
	@mkdir -p build

build: ${BUILD_PATH}/bitarray.o ${BUILD_PATH}/solver.o
	${CC} -o ${BUILD_PATH}/solver $^


${BUILD_PATH}/bitarray.o: ${COMMON_PATH}/bitarray.c
	${CC} -o $@ $^ -c -Iinclude


${BUILD_PATH}/solver.o: serial/solver_v2.c
	${CC} -o $@ $^ -c -Iinclude

.PHONY: clean
