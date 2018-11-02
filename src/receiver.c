#include "utilities.h"

char *lastack;

/**
 * This function parses a string with a port number and returns this number as an int, checking it for validity along the way
 */
int get_port(const char *portstring, const char *caller) {
	long l = -1;
	if ((l = strtol(portstring, NULL, 10)) <= 0) {
		fprintf(stderr, "[ERROR] [%s] Invalid port: %s\n", caller, portstring);
		return -1;
	} else if (l < 1024) {
		fprintf(stderr, "[ERROR] [%s] Reserved port (value cannot be less than 1024): %s\n", caller, portstring);
		return -1;
	} else if (l > 65535) {
		fprintf(stderr, "[ERROR] [%s] Invalid port (value must be less than or equal to 65535): %s\n", caller, portstring);
		return -1;
	}
	fprintf(stderr, "[LOG] [RECEIVER] Port initialised\n");
	return (int) l;
}

/**
 * Calculates the sequence number following the one given as input.
 */
int succ(int seqnum) {
	return (seqnum + 1) % 256;
}

/**
 * Determines whether the sequence number of a is greater than (in a special way) the sequence number of b
 * The seemingly arbitrary value of 200 is in fact just that, it makes sure that 0 is seen as greater than 255, which is necessary when packet loss occurs near the wrapping point of uint8_t
 */
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

/**
 * Determines whether the sequence numbers of a and b are equal
 */
int equal(const void *a, const void *b) {
	return ((pkt_t *) a)->seqnum == ((pkt_t *) b)->seqnum;
}

/**
 * Generic function that sends an acknowledgment (ACK or NACK) on the socket
 */
void send_acknowledgment(int sfd, pkt_t *packet, uint8_t wdw, uint8_t type) {
	uint8_t seqnum = pkt_get_seqnum(packet);
	uint16_t ts = pkt_get_timestamp(packet);
	if (type == PTYPE_ACK) {
		lastack = pkt_create(type, wdw, pkt_get_length(packet) == 0 ? seqnum : succ(seqnum), ts);
	} else {
		lastack = pkt_create(type, wdw, seqnum, ts);
	}
	if (write(sfd, lastack, 12) < 0) {
		fprintf(stderr, "[ERROR] [RECEIVER] Error while sending (N)ACK for packet %d\n", seqnum);
	} else {
		fprintf(stderr, "[LOG] [RECEIVER] (N)ACK sent for %d w/ seqnum %d\n", seqnum, succ(seqnum));
	}

}

/**
 * Writes data from a packet's payload to a file
 */
int write_data(FILE *f, pkt_t *packet) {
	fprintf(stderr, "[LOG] [RECEIVER] Writing payload for packet %d to file\n", pkt_get_seqnum(packet));
	size_t write = fwrite(pkt_get_payload(packet), sizeof(char), pkt_get_length(packet), f);
	if (write != pkt_get_length(packet)) {
		fprintf(stderr, "[ERROR] [RECEIVER] Error while writing payload for packet %d to file\n", pkt_get_seqnum(packet));
		return -1;
	}
	return 0;
}

/**
 * Receiver loop, this function reads from the socket, decodes the packet and determines what needs to be done with it (and does that thing)
 */
static void receive_data(FILE *f, int sfd) {
	bool cont = true;  // we want to keep receiving stuff
	char buffer[512+4*sizeof(uint32_t)]; // max packet size
	int last_seqnum = -1;
	uint8_t window_size = 31;
	minqueue_t *pkt_queue = minq_new(cmp, equal);

	// timer for disconnection (if no tranmission for a certain period of time)
	struct timespec last_time;
	int errc= clock_gettime(CLOCK_MONOTONIC, &last_time);
	if (errc != 0) {
		fprintf(stderr, "[LOG] [RECEIVER] Error with dc timer\n");
		return;
	}

	// timer for fast retransmission
	struct timespec rtstimer;
	int errcdef= clock_gettime(CLOCK_MONOTONIC, &rtstimer);
	if (errcdef != 0) {
		fprintf(stderr, "[LOG] [RECEIVER] Error with rts timer\n");
		return;
	}

	while (cont) {
		// read from socket
		ssize_t bytes_read = recv(sfd, buffer, 512+4*sizeof(uint32_t), 0);
		if (bytes_read == -1) {
			fprintf(stderr, "[ERROR] [RECEIVER] Invalid read from socket\n");
			continue;
		}
		// timer for disconnection
		struct timespec timee;
		int errcd= clock_gettime(CLOCK_MONOTONIC, &timee);
		if (errcd != 0) {
			fprintf(stderr, "Error with dc timer\n");
			return;
		}
		// disconnect if no transmission for a long period of time
		int timeout = 40;
		if (timee.tv_sec - last_time.tv_sec > timeout) {
			fprintf(stderr, "[LOG] [RECEIVER] No transmission for %d s; disconnect\n", timeout);
			return;
		}
		last_time = timee;

		struct timespec rts2timer;
		int errcde= clock_gettime(CLOCK_MONOTONIC, &rts2timer);
		if (errcde != 0) {
			fprintf(stderr, "Error with rts timer\n");
			return;
		}
		// retransmit ACK if nothing is receiver for a certain period of time
		int frts = 1;
		if (rts2timer.tv_sec - rtstimer.tv_sec > frts) {
			fprintf(stderr, "[LOG] [RECEIVER] Fast retransmit ACK\n");
			rtstimer = rts2timer;
			if (lastack != NULL) {
				int val = write(sfd, lastack, 12);
				val++;
			}
		}

		// decode packet
		pkt_t *packet = pkt_new();
		pkt_status_code pkt_status = pkt_decode(buffer, bytes_read, packet);
		uint32_t timestamp = pkt_get_timestamp(packet);
		timestamp = timestamp;
		struct timespec current_time;
		int err = clock_gettime(CLOCK_MONOTONIC,&current_time);

		if(err!=0){
			fprintf(stderr, "error clock in sending data");
			return;
		}

		// check if the packet is ok and if it lies in the receiver's window
		if (pkt_status == PKT_OK) {
			uint8_t seqnum = pkt_get_seqnum(packet);
			if ((seqnum > last_seqnum && seqnum < last_seqnum + window_size + 1) || ((last_seqnum + window_size > 255) && ((seqnum > last_seqnum) || (seqnum < (last_seqnum + window_size + 1) % 256)))) {
				minq_push(pkt_queue, packet);
				// while the next packet in the queue has the next expected seqnum, keep writing it to the file and popping it from the queue
				while (!minq_empty(pkt_queue) && ((pkt_t *) minq_peek(pkt_queue))->seqnum == succ(last_seqnum)) {
					packet = (pkt_t *) minq_peek(pkt_queue);
					if (pkt_get_length(packet) == 0) {
						fprintf(stderr, "[LOG] [RECEIVER] Terminating packet received with seqnum %d (window starts at %d and has length %d)\n", pkt_get_seqnum(packet), last_seqnum, window_size);
						cont = false;
					} else {
						fprintf(stderr, "[LOG] [RECEIVER] Data packet %d received\n", pkt_get_seqnum(packet));
						if (write_data(f, packet) != 0) {
							return;
						}
						free(lastack);
						window_size = (window_size == 31 ? window_size : window_size+1);
						send_acknowledgment(sfd, packet, window_size, PTYPE_ACK);
						last_seqnum = succ(last_seqnum);
					}
					minq_pop(pkt_queue);
					pkt_del(packet);
				}
			} else {
				fprintf(stderr, "[LOG] [RECEIVER] Sequence number %d not in window with start at %d and size %d\n", pkt_get_seqnum(packet), last_seqnum+1, window_size);
				if (lastack != NULL) {
					int eval = write(sfd, lastack, 12);
					eval++;
				}
				pkt_del(packet);
			}
		} else {
			// check if the packet was declared not ok because its truncation bit has the wrong value; if so, send an ACK and adjust the window_size to avoid congestion
			if (pkt_status == E_TR && pkt_get_type(packet) == PTYPE_DATA) {
				free(lastack);
				window_size = (window_size % 2 == 0 ? window_size/2 : (window_size+1)/2);
				send_acknowledgment(sfd, packet, window_size, PTYPE_NACK);
				fprintf(stderr, "[LOG] [RECEIVER] Packet %d truncated", pkt_get_seqnum(packet));
			} else {
				fprintf(stderr, "[LOG] [RECEIVER] Packet status not OK (error code %d) for packet %d \n", pkt_status, pkt_get_seqnum(packet));
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

	// read arguments
	if (argc >= 3) {
		while ((opt = getopt(argc, argv, "f:")) != -1) {
			switch (opt) {
				case 'f':
					memset(fn, 0, FILE_SIZE);
					memcpy(fn, optarg, strlen(optarg));
					file_specified = 1;
					fprintf(stderr, "[LOG] [RECEIVER] File specified: %s\n", fn);
					break;
				default:
					return EXIT_FAILURE;
			}
		}
	}

	// need to specify at least 2 arguments: hostname and port
	if (argc < 2) {
		fprintf(stderr, "[ERROR] [RECEIVER] Need 2 arguments\n");
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
			fprintf(stderr, "[ERROR] [RECEIVER] File already exists: %s\n", fn);
			return EXIT_FAILURE;
		}
		f = fopen(fn, "a+");
		if (f == NULL) {
			fprintf(stderr, "[ERROR] [RECEIVER] Failed to open file: %s\n", fn);
		} else {
			fprintf(stderr, "[LOG] [RECEIVER] Opened file: %s\n", fn);
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
		fprintf(stderr, "[ERROR] [RECEIVER] Could not resolve hostname or IP: %s\n", err);
		return EXIT_FAILURE;
	}

	// bind to socket
	int sfd = create_socket(&addr, port, NULL, -1);
	fprintf(stderr, "Connected to socket %d\n", sfd);

	if (sfd > 0 && wait_for_client(sfd) < 0) {
		fprintf(stderr, "[ERROR] [RECEIVER] Could not connect to socket\n");
		close(sfd);
		return EXIT_FAILURE;
	}

	// listen in on the appropriate channel
	if (file_specified) {
		fprintf(stderr, "[LOG] [RECEIVER] Ready to receive data\n");
		lastack = NULL;
		receive_data(f, sfd);
		fclose(f);
		free(lastack);
	} else {
		receive_data(stderr, sfd);
	}
	return EXIT_SUCCESS;
}
