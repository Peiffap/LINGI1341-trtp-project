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
LDFLAGS= -rdynamic -lz

# Default target
all: clean sender receiver

COMMON = send_receive.c pkt.c min_queue.c

# Receiver
receiver: receiver.o
	${CC} ${CFLAGS} -o receiver receiver.o ${COMMON} ${LDFLAGS}

receiver.o : receiver.c
	${CC} ${CFLAGS} -c receiver.c ${COMMON} ${LDFLAGS}

# Sender
sender:	sender.o
	${CC} ${CFLAGS} -o sender sender.o ${COMMON} ${LDFLAGS}

sender.o : sender.c
	${CC} ${CFLAGS} -c sender.c ${COMMON} ${LDFLAGS}

tests:
	cd ../tests && export LD_LIBRARY_PATH=$$HOME/local/lib:$$LD_LIBRARY_PATH && make
	make clean
# If we run `make debug` instead, keep the debug symbols for gdb
# and define the DEBUG macro.
debug: CFLAGS += -g -DDEBUG -Wno-unused-parameter -fno-omit-frame-pointer
debug: clean sender receiver

.PHONY: clean

clean:
	@rm -f receiver sender receiver.o sender.o send_receive.o pkt.o min_queue.o
