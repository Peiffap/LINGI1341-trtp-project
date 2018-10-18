#ifndef __UTILITIES_H
#define __UTILITIES_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <zlib.h>
#include <netdb.h>
#include <sys/time.h>
#include <time.h>
#include <poll.h>

#define STDIN 0
#define STDOUT 1

/* send_receive.c functions */
const char* real_address(const char *, struct sockaddr_in6 *);
int create_socket(struct sockaddr_in6 *, int, struct sockaddr_in6 *, int);
void read_write_loop(const int sfd);
int wait_for_client(int);

#endif  /* __UTILITIES_H */
