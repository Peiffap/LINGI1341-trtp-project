#include "utilities.h"

int get_port(const char *portstring, const char *caller) {
	long l = -1;
	if ((l = strtol(portstring, NULL, 10)) == 0) {
		printf("[ERROR] [%s] Invalid port: %s\n", caller, portstring);
		return -1;
	} else if (l < 1024) {
		printf("[ERROR] [%s] Reserved port (value cannot be less than 1024): %s\n", caller, portstring);
		return -1;
	} else if (l > 65535) {
		printf("[ERROR] [%s] Invalid port (value must be less than 65535): %s\n", caller, portstring);
		return -1;
	}
	printf("[LOG] [RECEIVER] Port initialised\n");
	return (int) l;
}

int succ(int seqnum) {
	return (seqnum + 1) % 256;
}

int cmp(const void *a, const void *b) {
	return ((pkt_t *) a)->seqnum > ((pkt_t *) b)->seqnum;
}

int equal(const void *a, const void *b) {
	return ((pkt_t *) a)->seqnum == ((pkt_t *) b)->seqnum;
}

void send_acknowledgment(int sfd, pkt_t *packet, uint8_t wdw, uint8_t type) {
	uint8_t seqnum = pkt_get_seqnum(packet);
	uint16_t ts = pkt_get_timestamp(packet);
	char *buf;
	if (type == PTYPE_ACK) {
		buf = pkt_create(type, wdw, succ(seqnum), ts);
	} else {
		buf = pkt_create(type, wdw, seqnum, ts);
	}
	if (write(sfd, buf, 12) < 0) {
		printf("[ERROR] [RECEIVER] Error while sending (N)ACK\n");
	} else {
		printf("[LOG] [RECEIVER] (N)ACK sent\n");
	}
	free(buf);
}

int write_data(FILE *f, pkt_t *packet) {
	size_t write = fwrite(pkt_get_payload(packet), sizeof(uint8_t), pkt_get_length(packet), f);
	if (write != pkt_get_length(packet)) {
		printf("[ERROR] [RECEIVER] Error while writing payload to file\n");
		return -1;
	}
	return 0;
}

static void receive_data(FILE *f, int sfd) {
	bool cont = true;  // we want to keep receiving stuff
	char buffer[512+4*sizeof(uint32_t)];
	int last_seqnum = -1;
	uint8_t window_size = 31;
	minqueue_t *pkt_queue = minq_new(cmp, equal);

	pkt_t *test = pkt_new();
	pkt_set_type(test, PTYPE_DATA);
	pkt_set_tr(test, 0);
	pkt_set_window(test, 0b00011111);
	pkt_set_seqnum(test, 0b00000000);
	pkt_set_length(test, 7);
	pkt_set_timestamp(test, 0b00011111000111110001111100011111);
	pkt_set_crc1(test, 0b00011111000111110001111100011111);
	pkt_set_payload(test, "werner", 7);
	pkt_set_crc2(test, 0b00011111000111110001111100011111);

	printf("Packet defined\n");

	int i = 0;

	while (cont && i == 0) {
		i++;
		
		ssize_t bytes_read = recv(sfd, buffer, 512+4*sizeof(uint32_t), 0);
		if (bytes_read == -1) {
			printf("[ERROR] [RECEIVER] Invalid read from socket\n");
			continue;
		}
		/*

		size_t len;
		pkt_status_code pkt_stat = pkt_encode(test, buffer, &len);
		printf("%d", pkt_stat);
		size_t bytes_read = 23;*/

		printf("this is the buffer : %d\n", buffer[0]);

		pkt_t *packet = pkt_new();
		pkt_status_code pkt_status = pkt_decode(buffer, bytes_read, packet);

		if (pkt_status == PKT_OK) {
			if (window_size > 0) {
				minq_push(pkt_queue, packet);
				--window_size;
				while (((pkt_t *) minq_peek(pkt_queue))->seqnum == succ(last_seqnum)) {
					++window_size;
					if (pkt_get_length(packet) == 0) {
						printf("[LOG] [RECEIVER] Terminating packet received\n");
						cont = false;
					} else {
						printf("[LOG] [RECEIVER] Data packet %d received\n", pkt_get_seqnum(packet));
						if (write_data(f, packet) != 0) {
							return;
						}

						send_acknowledgment(sfd, packet, window_size, PTYPE_ACK);
						last_seqnum = succ(last_seqnum);
					}
					minq_pop(pkt_queue);
					pkt_del(packet);
				}
			} else {
				printf("[LOG] [RECEIVER] Buffer is full\n");
				pkt_del(packet);
			}
		} else {
			if (pkt_status == E_TR && pkt_get_type(packet) == PTYPE_DATA) {
				send_acknowledgment(sfd, packet, window_size, PTYPE_NACK);
				printf("[LOG] [RECEIVER] Packet %d truncated", pkt_get_seqnum(packet));
			} else {
						printf("after %ld\n", bytes_read);

				for(int j = 0; j<528; j++){
					printf("%x", buffer[j]);
				}
													printf("after\n");
				printf("[LOG] [RECEIVER] Packet status not OK, %d \n", pkt_status==E_CRC);
			}
			pkt_del(packet);
		}
	}
	minq_del(pkt_queue);
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
					printf("[LOG] [RECEIVER] File specified: %s\n", fn);
					break;
				default:
					return EXIT_FAILURE;
			}
		}
	}

	// need to specify at least 2 arguments: hostname and port
	if (argc < 2) {
		printf("[ERROR] [RECEIVER] Need 2 arguments\n");
		return EXIT_FAILURE;
	}

	argc -= optind;
	argv += optind;

	char *hostname = argv[0];  // host name
	char *portstr = argv[1];  // port number argument

	// if a file is specified, it must exist
	FILE *f = NULL;
	if (file_specified) {
		struct stat fs;
		if (stat(fn, &fs) == 0) {
			printf("[ERROR] [RECEIVER] File already exists: %s\n", fn);
			return EXIT_FAILURE;
		}
		f = fopen(fn, "a+");
		if (f == NULL) {
			printf("[ERROR] [RECEIVER] Failed to open file: %s\n", fn);
		} else {
			printf("[LOG] [RECEIVER] Opened file: %s\n", fn);
		}
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
		printf("[ERROR] [RECEIVER] Could not resolve hostname or IP: %s\n", err);
		return EXIT_FAILURE;
	}

	// bind to socket
	int sfd = create_socket(&addr, port, NULL, -1);
	printf("Connected to socket %d\n", sfd);
	/*
	if (sfd > 0 && wait_for_client(sfd) < 0) {
		printf("[ERROR] [RECEIVER] Could not connect to socket\n");
		close(sfd);
		return EXIT_FAILURE;
	}
	*/

	// listen in on the appropriate channel
	if (file_specified) {
		printf("[LOG] [RECEIVER] Ready to receive data\n");
		receive_data(f, sfd);
	} else {
		receive_data(stdout, sfd);
	}
	fclose(f);
	return EXIT_SUCCESS;
}
