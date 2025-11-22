CC := "gcc"
CFLAGS += -Wall -std=c99 -pedantic -lpthread
SOURCES = $(wildcard *.c)
OBJECTS = $(SOURCES:%.c=%.o)
DEPS = $(SOURCES:%.c=%.d)

$(info ${SOURCES})
$(info ${OBJECTS})
$(info ${DEPS})
all: test

test: ${OBJECTS}
	${CC} -o $@ $^

tests.o: tests.c

pthread_groups.o: pthread_groups.c pthread_groups.h 

.PHONY: clean

clean:
	rm -f test *.o *.d *.s

