#include "utilities.h"

#define FILE_SIZE 64

int get_port(const char *portstring, const char *caller) {
	long l = -1;
	if ((l = strtol(portstring, NULL, 10)) == 0) {
		printf("[%s] Invalid port: %s\n", caller, portstring);
		return -1;
	} else if (l < 1024) {
		printf("[%s] Reserved port (value cannot be less than 1024): %s\n", caller, portstring);
		return -1;
	} else if (l > 65535) {
		printf("[%s] Invalid port (value must be less than 65535): %s\n", caller, portstring);
		return -1;
	}
	return (int) l;
}

int main(int argc, char *argv[]) {
	int file_specified = 0;
	int opt;
	char fn[FILE_SIZE];  // adjust size if needed

	if (argc >= 3) {
		while ((opt = getopt(argc, argv, "f:")) != -1) {
			switch (opt) {
				case 'f':
					memset(fn, 0, FILE_SIZE);
					memcpy(fn, optarg, strlen(optarg));
					file_specified = 1;
					break;
				default:
					return EXIT_FAILURE;
			}
		}
	}

	// need to specify at least 2 arguments: hostname and port
	if (argc < 2) {
		return EXIT_FAILURE;
	}

	char *hostname = argv[0];  // host name
	char *portstr = argv[1];  // port number argument

	// if a file is specified, it must exist
	FILE *f = NULL;
	if (file_specified) {
		struct stat fs = {0};
		if (stat(fn, &fs) == 0) {
			printf("[RECEIVER] File already exists: %s\n", fn);
			return EXIT_FAILURE;
		}
		f = fopen(fn, "a+");
		if (f == NULL)
			printf("[RECEIVER] Failed to open file: %s\n", fn);
	}

	// resolve address
	// port
	int port = get_port(portstr, "RECEIVER");
	if (port == -1) {
		return EXIT_FAILURE;
	}

	// hostname
	struct sockaddr_in6 addr;
	const char *err = real_address(hostname, &addr);
	if (err) {
		printf("[RECEIVER] Could not resolve hostname or IP: %s\n", err);
		return EXIT_FAILURE;
	}

	// bind to socket
	int sfd = create_socket(&addr, port, NULL, -1);
	if (sfd > 0 && wait_for_client(sfd) < 0) {
		printf("[RECEIVER] Could not connect to socket\n");
		close(sfd);
		return EXIT_FAILURE;
	}

	// listen in on the appropriate channel
	if (file_specified) {
		receive_data(stdout);
	} else {
		receive_data(sfd);
	}
	return EXIT_SUCCESS;
}
