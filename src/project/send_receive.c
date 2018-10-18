#include "utilities.h"

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
	if (err != 0)
		return gai_strerror(err);
	struct sockaddr_in6 *result = (struct sockaddr_in6 *) (res->ai_addr);
	*rval = *result;
	freeaddrinfo(res);
	return NULL;
}

int create_socket(struct sockaddr_in6 *source_addr, int src_port, struct sockaddr_in6 *dest_addr, int dst_port) {
	int sockfd;
	int err;
	sockfd = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	if (sockfd == -1) {
		perror("error while creating socket : ");
		return -1;
	}

	if (source_addr != NULL && src_port > 0) {
		source_addr->sin6_port = htons(src_port);

		err = bind(sockfd, (struct sockaddr *) source_addr, sizeof(struct sockaddr_in6));
		if (err != 0) {
			perror("error while binding socket 1 : ");
			return -1;
		}
	}

	if (dest_addr != NULL && dst_port > 0) {
		dest_addr->sin6_port = htons(dst_port);
		err = connect(sockfd, (struct sockaddr *) dest_addr, sizeof(struct sockaddr_in6));
		if (err != 0) {
			perror("error while connecting socket2 : ");
			return -1;
		}
	}
	return sockfd;
}

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

int wait_for_client(int sfd) {
	struct sockaddr_in6 their_addr;
	socklen_t sin_size = sizeof(struct sockaddr_in6);
	memset(&their_addr, 0, sin_size);
	char buf[1024];
	int err = recvfrom(sfd, buf, sizeof buf, MSG_PEEK, (struct sockaddr *) &their_addr, &sin_size);

	if (err == -1) {
		perror("error while receiving message : ");
		return -1;
	}
	err = connect(sfd, (struct sockaddr *) &their_addr, sin_size);
	if (err == -1) {
		perror("error while connecting : ");
		return -1;
	}

	return err;
}
