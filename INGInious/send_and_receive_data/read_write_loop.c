#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/time.h>

#define STDIN 0
#define STDOUT 1

/* Loop reading a socket and printing to stdout,
 * while reading stdin and writing to the socket
 * @sfd: The socket file descriptor. It is both bound and connected.
 * @return: as soon as stdin signals EOF
 */
void read_write_loop(const int sfd) {
    struct timeval tv;
    fd_set readfds;
    tv.tv_sec= 6;
    tv.tv_usec = 0;

    char buf1[1024];
    char buf2[1024];

    int end = 0;
    int err;
    while (end == 0) {
        memset((void *) buf1, 0, 1024);
        memset((void *) buf2, 0, 1024);
        FD_ZERO(&readfds);
        FD_SET(STDIN, &readfds);
        FD_SET(sfd, &readfds);

        select(sfd + 1, &readfds, NULL, NULL, &tv);

        if (FD_ISSET(STDIN, &readfds)) {
            err = read(STDIN, buf1, 1024);
            if (err > 0 && write(sfd, buf1, err) > 0)
                perror("error write STDOUT : ");
        }

        if (FD_ISSET(sfd, &readfds)) {
            err = read(sfd, buf2, 1024);

            if (err > 0 && write(STDOUT, buf2, err) < 0)
                perror("error writing STDOUT : ");
        }
        end = feof(stdin);
    }
}
