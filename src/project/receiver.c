#include "utilities.h"

char *lastack;

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
// return true if a comes after b
int cmp(const void *a, const void *b) {
	uint8_t seqa = ((pkt_t *) a)->seqnum;
	uint8_t seqb = ((pkt_t *) b)->seqnum;
	if (seqa >= seqb && seqa - seqb > 200) {
		return 0;
	} else if (seqa < seqb && seqb - seqa > 200) {
		return 1;
	} else if (seqa > seqb) {
		return 1;
	} else {
		return 0;
	}
}

int equal(const void *a, const void *b) {
	return ((pkt_t *) a)->seqnum == ((pkt_t *) b)->seqnum;
}

void send_acknowledgment(int sfd, pkt_t *packet, uint8_t wdw, uint8_t type) {
	uint8_t seqnum = pkt_get_seqnum(packet);
	uint16_t ts = pkt_get_timestamp(packet);
	if (type == PTYPE_ACK) {
		lastack = pkt_create(type, wdw, pkt_get_length(packet) == 0 ? seqnum : succ(seqnum), ts);
	} else {
		lastack = pkt_create(type, wdw, seqnum, ts);
	}
	if (write(sfd, lastack, 12) < 0) {
		printf("[ERROR] [RECEIVER] Error while sending (N)ACK for packet %d\n", seqnum);
	} else {
		printf("[LOG] [RECEIVER] (N)ACK sent for %d w/ seqnum %d\n", seqnum, succ(seqnum));
	}
	
}

int write_data(FILE *f, pkt_t *packet) {
	printf("[LOG] [RECEIVER] Writing payload for packet %d to file\n", pkt_get_seqnum(packet));
	size_t write = fwrite(pkt_get_payload(packet), sizeof(char), pkt_get_length(packet), f);
	if (write != pkt_get_length(packet)) {
		printf("[ERROR] [RECEIVER] Error while writing payload for packet %d to file\n", pkt_get_seqnum(packet));
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

	struct timespec last_time;
	int errc= clock_gettime(CLOCK_MONOTONIC, &last_time);
	if (errc != 0) {
		perror("Error with dc timer\n");
		return;
	}

	struct timespec rtstimer;
	int errcdef= clock_gettime(CLOCK_MONOTONIC, &rtstimer);
	if (errcdef != 0) {
		perror("Error with dc timer\n");
		return;
	}

	while (cont) {

		ssize_t bytes_read = recv(sfd, buffer, 512+4*sizeof(uint32_t), 0);
		if (bytes_read == -1) {
			printf("[ERROR] [RECEIVER] Invalid read from socket\n");
			continue;
		}

		struct timespec timee;
		int errcd= clock_gettime(CLOCK_MONOTONIC, &timee);
		if (errcd != 0) {
			perror("Error with dc timer\n");
			return;
		}

		int timeout = 400;
		if (timee.tv_sec - last_time.tv_sec > timeout) {
			printf("[LOG] [RECEIVER] No transmission for %d s; disconnect\n", timeout);
			return;
		}
		last_time = timee;

		struct timespec rts2timer;
		int errcde= clock_gettime(CLOCK_MONOTONIC, &rts2timer);
		if (errcde != 0) {
			perror("Error with dc timer\n");
			return;
		}

		int frts = 1;
		if (rts2timer.tv_sec - rtstimer.tv_sec > frts) {
			printf("[LOG] [RECEIVER] Fast retransmit ACK\n");
			rtstimer = rts2timer;
			if (lastack != NULL) {
				int val = write(sfd, lastack, 12);
				val++;
			}
		}

		pkt_t *packet = pkt_new();
		pkt_status_code pkt_status = pkt_decode(buffer, bytes_read, packet);
		uint32_t timestamp = pkt_get_timestamp(packet);
		timestamp = timestamp;
		struct timespec current_time;
		int err = clock_gettime(CLOCK_MONOTONIC,&current_time);

		if(err!=0){
			perror("error clock in sending data");
			return;
		}

		if (pkt_status == PKT_OK) {
			uint8_t seqnum = pkt_get_seqnum(packet);
			printf("before if%d\n", seqnum);
			if ((seqnum > last_seqnum && seqnum < last_seqnum + window_size + 1) || ((last_seqnum + window_size > 255) && ((seqnum > last_seqnum) || (seqnum < (last_seqnum + window_size + 1) % 256)))) {
				minq_push(pkt_queue, packet);
				printf("%d, %d\n", ((pkt_t *) minq_peek(pkt_queue))->seqnum, succ(last_seqnum));
				while (!minq_empty(pkt_queue) && ((pkt_t *) minq_peek(pkt_queue))->seqnum == succ(last_seqnum)) {
					packet = (pkt_t *) minq_peek(pkt_queue);
					if (pkt_get_length(packet) == 0) {
						printf("[LOG] [RECEIVER] Terminating packet received with seqnum %d (window starts at %d and has length %d)\n", pkt_get_seqnum(packet), last_seqnum, window_size);
						printf("%d", (seqnum > last_seqnum && seqnum < last_seqnum + window_size + 1) || ((last_seqnum + window_size > 255) && ((seqnum > last_seqnum) || (seqnum < (last_seqnum + window_size + 1) % 256))));
						cont = false;
					} else {
						printf("[LOG] [RECEIVER] Data packet %d received\n", pkt_get_seqnum(packet));
						if (write_data(f, packet) != 0) {
							return;
						}
						free(lastack);
						window_size = (window_size == 31 ? window_size : window_size+1);
						send_acknowledgment(sfd, packet, window_size, PTYPE_ACK);
						printf("$%d = %d\n", last_seqnum, succ(last_seqnum));
						last_seqnum = succ(last_seqnum);
					}
					minq_pop(pkt_queue);
					pkt_del(packet);
				}
			} else {
				printf("[LOG] [RECEIVER] Sequence number %d not in window with start at %d and size %d\n", pkt_get_seqnum(packet), last_seqnum+1, window_size);
				if (lastack != NULL) {
					int eval = write(sfd, lastack, 12);
					eval++;
				}
				pkt_del(packet);
			}
		} else {
			if (pkt_status == E_TR && pkt_get_type(packet) == PTYPE_DATA) {
				free(lastack);
				window_size = (window_size % 2 == 0 ? window_size/2 : (window_size+1)/2);
				send_acknowledgment(sfd, packet, window_size, PTYPE_NACK);
				printf("[LOG] [RECEIVER] Packet %d truncated", pkt_get_seqnum(packet));
			} else {
				printf("[LOG] [RECEIVER] Packet status not OK (erro code %d) for packet %d \n", pkt_status==E_CRC, pkt_get_seqnum(packet));
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

	if (sfd > 0 && wait_for_client(sfd) < 0) {
		printf("[ERROR] [RECEIVER] Could not connect to socket\n");
		close(sfd);
		return EXIT_FAILURE;
	}

	// listen in on the appropriate channel
	if (file_specified) {
		printf("[LOG] [RECEIVER] Ready to receive data\n");
		lastack = NULL;
		receive_data(f, sfd);
		fclose(f);
		free(lastack);
	} else {
		receive_data(stdout, sfd);
	}
	return EXIT_SUCCESS;
}
