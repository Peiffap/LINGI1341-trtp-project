# See gcc/clang manual to understand all flags
CFLAGS += -std=c99 # Define which version of the C standard to use
CFLAGS += -Wall # Enable the 'all' set of warnings
CFLAGS += -g
CFLAGS += -Werror # Treat all warnings as error
CFLAGS += -Wshadow # Warn when shadowing variables
CFLAGS += -Wextra # Enable additional warnings
CFLAGS += -O0 # Add canary code, i.e. detect buffer overflows
CFLAGS += -fstack-protector-all # Add canary code to detect stack smashing
CFLAGS += -D_POSIX_C_SOURCE=201112L -D_XOPEN_SOURCE # feature_test_macros for getpot and getaddrinfo

CC      = cc

# We have no libraries to link against except libc, but we want to keep
# the symbols for debugging
LDFLAGS= -rdynamic -lz -I$$HOME/local/include -L$$HOME/local/lib -lcunit -lSDL
# Default target
all: clean testsender testreceiver
	./testsender
	./testreceiver

COMMON = ../src/send_receive.c ../src/pkt.c ../src/min_queue.c

test_receiver.o: test_receiver.c
	${CC} ${CFLAGS} -c test_receiver.c ${COMMON} ${LDFLAGS}

testreceiver: test_receiver.o
	${CC} ${CFLAGS} -o testreceiver test_receiver.o ${COMMON} ${LDFLAGS}

test_sender.o: test_sender.c
	${CC} ${CFLAGS} -c test_sender.c ${COMMON} ${LDFLAGS}

testsender: test_sender.o
	${CC} ${CFLAGS} -o testsender test_sender.o ${COMMON} ${LDFLAGS}

.PHONY: clean

clean:
	@rm -f testreceiver testsender test_sender.o test_receiver.o send_receive.o pkt.o min_queue.o
