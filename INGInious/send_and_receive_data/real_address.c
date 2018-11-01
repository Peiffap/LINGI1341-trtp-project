#include "real_address.h"
#include <netinet/in.h>
#include <sys/types.h> // for sockaddr_in6
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Resolve the resource name to an usable IPv6 address
 * @address: The name to resolve
 * @rval: Where the resulting IPv6 address descriptor should be stored
 * @return: NULL if it succeeded, or a pointer towards
 *          a string describing the error if any.
 *          (const char* means the caller cannot modify or free the return value,
 *           so do not use malloc!)
 */
const char *real_address(const char *address, struct sockaddr_in6 *rval) {
    struct addrinfo hints;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags = 0;

    struct addrinfo *res;
    int err;
    err = getaddrinfo(address, NULL, &hints, &res);
    if (err != 0) {
        return gai_strerror(err);
    }
    struct sockaddr_in6 *result = (struct sockaddr_in6 *) (res->ai_addr);
    *rval = *result;
    freeaddrinfo(res);
    return NULL;
}
