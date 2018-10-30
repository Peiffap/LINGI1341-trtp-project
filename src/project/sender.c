#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include "utilities.h"
#include <fcntl.h>
#include <errno.h>
#include <inttypes.h>

#define MAX_PAYLOAD_LENGTH 512
#define MAX_SEQNUM 256
#define TIMEOUT 1000000


uint8_t lastseqnum = 0;
uint8_t window = 1;
uint8_t firstseqnumwindow = 0;
struct dataqueue {
	char *bufpkt;
	uint8_t seqnum;
	uint16_t len;
	struct timespec time;
	struct dataqueue *next;
};
struct dataqueue *startofqueue = NULL;
struct dataqueue *firsttosend = NULL;
struct dataqueue *lasttosend = NULL;
uint8_t lastackseqnum = -1;
int pkt_waiting = 0;
int pkt_to_send = 0;

int cmp(const uint8_t seqa, const uint8_t seqb) {
	
	if (seqa > seqb && seqa - seqb > 200) {
		return 0;
	} else if (seqa <= seqb && seqb - seqa > 200) {
		return 1;
	} else if (seqa >= seqb) {
		return 1;
	} else {
		return 0;
	}
}

int remove_pkt(uint8_t seqnum){
	printf("[LOG] [SENDER] Removing packet with sequence number %d\n", seqnum);
	struct dataqueue *current = startofqueue;



	if(current != NULL && cmp(seqnum, current->seqnum)){
		startofqueue = current->next;
		pkt_waiting--;
		free(current->bufpkt);
		free(current);
		current = NULL;
	}
	else{
		current = current->next;
		struct dataqueue *before = startofqueue;
		while(current!=NULL){
			if (cmp(seqnum, current->seqnum)){

				before->next = current->next;
				pkt_waiting--;
				free(current->bufpkt);
				free(current);
				current = NULL;


			}

			else{
				before = before->next;
				current = current->next;
			}

		}
	}
	if(startofqueue != NULL && (cmp(seqnum, startofqueue->seqnum))){
		current = startofqueue;
		startofqueue = startofqueue->next;
		pkt_waiting--;
		free(current->bufpkt);
		free(current);
		current = NULL;

	}
	printf("[LOG] [SENDER] Packet with sequence number %d removed\n", seqnum);
	return 0;
}

int succ(int seqnum) {
	return (seqnum + 1) % 256;
}

int is_in_window(int seqnum){
	return ((((seqnum >= firstseqnumwindow)
			&&(seqnum < firstseqnumwindow + window)
			&&(firstseqnumwindow+window<= MAX_SEQNUM))
		||((firstseqnumwindow+window> MAX_SEQNUM)&&((seqnum >= firstseqnumwindow)
			||(seqnum < (firstseqnumwindow + window)%MAX_SEQNUM)))));
}

int send_pkt(const int sfd){

	if((firsttosend != NULL)&&(is_in_window(firsttosend->seqnum))){

		

		int err = clock_gettime(CLOCK_MONOTONIC,&(firsttosend->time));

		if(err!=0){
			perror("error clock in sending data");
			return -1;
		}

		// set timestamp as current time
		memcpy((firsttosend->bufpkt)+4,&(firsttosend->time.tv_sec), sizeof(uint32_t));


	 	// set crc1
		uint32_t testCrc1 = 0;
		char dataNonTr[8];
		memcpy(dataNonTr, firsttosend->bufpkt, sizeof(uint64_t));
		dataNonTr[0] = dataNonTr[0] & 0b11011111;
		testCrc1 = crc32(testCrc1, (Bytef *)(&dataNonTr), sizeof(uint64_t));

	    // crc1
		uint32_t crc1 = htonl(testCrc1);
		memcpy((firsttosend->bufpkt)+8, &crc1,sizeof(uint32_t));

		err = write(sfd, firsttosend->bufpkt, firsttosend->len);
		pkt_to_send--;
		printf("[LOG] [SENDER] Writing packet with seqnum %d to socket\n", firsttosend->seqnum);
		if(err <0){
			perror("error writing pkt");
			return -1;
		}



		firsttosend = firsttosend->next;
	}

	// Else need an ack

	return 0;
}


int add_pkt_to_queue( char* buf, int len){
	int timestamp = 0; // How do we use the timestamp ??
	pkt_t * newpkt = pkt_create_sender(window, lastseqnum, len, timestamp, buf);
	char *bufpkt = malloc(len+4*sizeof(uint32_t));
	if(bufpkt==NULL){
		perror("error malloc in add pkt to queue");
		return -1;
	}
	size_t totlen = len+4*sizeof(uint32_t);
	pkt_status_code ret = pkt_encode(newpkt, bufpkt, &totlen);

	if(ret != PKT_OK){
		perror("error encoding pkt in add_pkt_to_queue");
		return -1;
	}
	pkt_del(newpkt);

	struct dataqueue *newdata = (struct dataqueue *)malloc(sizeof(struct dataqueue));
	if(newdata==NULL){
		perror("error malloc in add pkt to queue");
		return -1;
	}
	if(lasttosend!=NULL){
		lasttosend->next = newdata;
		lasttosend = newdata;
		lasttosend->bufpkt = bufpkt;
		lasttosend->seqnum = lastseqnum;
		lasttosend->len = totlen;
		lasttosend->next = NULL;
	}
	else{
		lasttosend = newdata;
		lasttosend->bufpkt = bufpkt;
		lasttosend->seqnum = lastseqnum;
		lasttosend->len = totlen;
		lasttosend->next = NULL;

	}
	if(firsttosend == NULL){
		firsttosend = lasttosend;
	}
	if(startofqueue == NULL){
		startofqueue = firsttosend;
	}

	printf("[LOG] [SENDER] Finished adding packet %d to queue \n", lastseqnum);
	lastseqnum = (lastseqnum + 1)%MAX_SEQNUM;

	return 0;

}


int disconnect(int sfd){

	printf("[LOG] [SENDER] Entering disconnection\n");

	uint32_t timestamp = 0; // How do we use the timestamp ??
	struct timespec time;

	int err = clock_gettime(CLOCK_MONOTONIC,&time);

	timestamp = time.tv_sec;
	if(err!=0){
		perror("error clock in sending data");
		return -1;
	}


	pkt_t * newpkt = pkt_create_sender(window, succ(lastackseqnum), 0, timestamp, NULL);





	char bufpkt[3*sizeof(uint32_t)];
	size_t totlen = 3*sizeof(uint32_t);
	pkt_status_code ret = pkt_encode(newpkt, bufpkt, &totlen);
	if(ret != PKT_OK){
		perror("error encoding pkt in disconnect");
		return -1;
	}

	int errw = write(sfd, bufpkt, totlen);
	if(errw <0){
		perror("error writing pkt in disconnect");

		return -1;
	}

	printf("[LOG] [SENDER] Sent disconnection packet with seqnum %d\n", pkt_get_seqnum(newpkt));

	pkt_del(newpkt);


	struct timespec time1;
	errw = clock_gettime(CLOCK_MONOTONIC,&time1);

	if(errw!=0){
		perror("error clock in disconnect");

		return -1;
	}
	return 0;
}
int send_data(const int sfd, const int fd){

	struct timeval tv;
	fd_set readfds;
	tv.tv_sec= 0;
	tv.tv_usec = 100000;

	char buf1[MAX_PAYLOAD_LENGTH];
	char buf2[sizeof(pkt_t)+MAX_PAYLOAD_LENGTH];


	int err;
	int stop = 1;
	while ((pkt_waiting > 0 || stop) || succ(lastackseqnum) != lastseqnum) {
		memset((void *) buf1, 0, MAX_PAYLOAD_LENGTH);
		memset((void *) buf2, 0, sizeof(pkt_t)+MAX_PAYLOAD_LENGTH);
		FD_ZERO(&readfds);
		FD_SET(fd, &readfds);
		FD_SET(sfd, &readfds);

		int selerr = select(fd > sfd ? fd + 1 : sfd + 1, &readfds, NULL, NULL, &tv);

		if (selerr == -1){
			perror("error select");
			return -1;

		}
		if (FD_ISSET(fd, &readfds) && stop && pkt_waiting <= window) {
		    	err = read(fd, buf1, MAX_PAYLOAD_LENGTH);

			printf("Bytes read from input file %d \n", err);
			if (err < 0 ){
				perror("error read fd in send data ");
				printf("[ERROR] [SENDER] %d, %d \n", err, errno);
				return -1;
			}
			if(err >0){
				int erradd = add_pkt_to_queue(buf1, err);
				if(erradd != 0){
					perror("error adding pkt to queue");
					return -1;
				}
				pkt_waiting ++;
				pkt_to_send++;
			}
			if(err == 0){
				stop = 0;
			}
		}


		if(pkt_to_send >0){
			int errsend = send_pkt(sfd);
			if(errsend !=0){
				perror("error sending pkt");
				return -1;
			}

		}

		if (FD_ISSET(sfd, &readfds)) { // Data incoming from socket



		    	err = read(sfd, buf2, sizeof(pkt_t)+MAX_PAYLOAD_LENGTH);
			printf("[LOG] [SENDER] done reading ACK from socket\n");

		    	if (err <= 0){
				perror("error reading from socket : ");
				return -1;
			}
			pkt_t *ack = pkt_new();
			err = pkt_decode(buf2, err, ack);
			if(err != PKT_OK){
				perror("decoding ack");
				pkt_del(ack);
			}
			else{
				int seqnum = pkt_get_seqnum(ack);

				printf("Window start at %d, end at %d, seqnum %d\n", firstseqnumwindow, (firstseqnumwindow+window)%MAX_SEQNUM, pkt_get_seqnum(ack)-1);

				if((pkt_get_type(ack) == PTYPE_ACK)&& (is_in_window(seqnum-1))){
					lastackseqnum = seqnum-1;
					if (seqnum == 0){
						lastackseqnum = MAX_SEQNUM -1;
					}
					window = pkt_get_window(ack);
					printf("[LOG] [SENDER] ACK received for seqnum %d\n", lastackseqnum);
					firstseqnumwindow = (seqnum %MAX_SEQNUM);
					remove_pkt(lastackseqnum);




				}
				if(pkt_get_type(ack) == PTYPE_NACK){

					printf("[LOG] [SENDER] NACK received for seqnum %d\n", lastackseqnum);

					struct dataqueue *current = startofqueue;
					int search = 1;
					while((current!=0)&&(search)){

						printf("sender nack loop\n");
						if(current->seqnum == seqnum){
							search = 0;
						}
						else{
							current = current->next;
						}
					}
					int errw = write(sfd, current->bufpkt, current->len);
					if(errw <0){
						perror("error writing pkt in resending pkt");
						pkt_del(ack);

						return -1;
					}

					errw = clock_gettime(CLOCK_MONOTONIC,&(current->time));

					if(errw!=0){
						perror("error clock in resending data");
						pkt_del(ack);

						return -1;
					}
				}
				pkt_del(ack);
			}
		}



		struct timespec time;
		int errc = clock_gettime(CLOCK_MONOTONIC,&time);

		if(errc!=0){
			perror("error get time in send data");
			return -1;
		}

		struct dataqueue *current = startofqueue;
		while (current != firsttosend) {

			if(((time.tv_sec - current->time.tv_sec)*1000000 + (time.tv_nsec - current->time.tv_nsec)/1000)>TIMEOUT){

			//RESEND DATA


				int errclock = clock_gettime(CLOCK_MONOTONIC,&(current->time));

				if(errclock!=0){
					perror("error clock in sending data");
					return -1;
				}

				// set timestamp as current time
				memcpy((current->bufpkt)+4,&(current->time.tv_sec), sizeof(uint32_t));


			 	// set crc1
				uint32_t testCrc1 = 0;
				char dataNonTr[8];
				memcpy(dataNonTr, current->bufpkt, sizeof(uint64_t));
				dataNonTr[0] = dataNonTr[0] & 0b11011111;
				testCrc1 = crc32(testCrc1, (Bytef *)(&dataNonTr), sizeof(uint64_t));

			    // crc1
				uint32_t crc1 = htonl(testCrc1);
				memcpy((current->bufpkt)+8, &crc1,sizeof(uint32_t));


				printf("[LOG] [SENDER] Timeout for packet %"PRIu8"\n", *(current->bufpkt+1));
				int errw = write(sfd, current->bufpkt, current->len);
				if(errw <0){
					perror("error writing pkt in resending pkt");
					return -1;
				}

				

			}
			current = current->next;
		}


	}

	int errdis = disconnect(sfd);
	if(errdis !=0){
		perror("error disconnecting");
		return -1;
	}

	return 0;
}





int main( int argc, char * argv[]){
	if(argc <3){ // Not enough arguments
		perror("Not enough arguments");
		return -1;
	}

	char *hostname;
	char *filename = NULL;
	int port;

	if(strcmp(argv[1], "-f")==0){ // if there is a file
		filename = argv[2];
		struct stat sf;
		if(stat(filename, &sf)==-1){ // file does not exist
			perror("File does not exist");
			return -1;
		}

		hostname = argv[3];
		port = atoi(argv[4]);
	}
	else{
		hostname = argv[1];
		port = atoi(argv[2]);
	}

	if (port <= 0){
		perror("Invalid port");
		return -1;
	}
	struct sockaddr_in6 dest_addr;

	const char *err = real_address(hostname, &dest_addr);
	if(err != NULL){
		perror(err);
		return -1;
	}

	int sfd = create_socket(NULL, 0, &dest_addr, port);

	if( sfd ==-1){
		perror("Error creating the socket");
		return -1;
	}

	if(filename !=NULL){

		int fd = open(filename, O_RDONLY);
		if( fd<0){
			perror("Error opening the file");

			return -1;
		}

		int err1 = send_data(sfd, fd);

		if(err1 ==-1){
			perror("error sending data");
			close(fd);
			return -1;
		}

		close(fd);

	}
	else {

		int err1 = send_data(sfd, 0);
		if(err1 ==-1){
			perror("error sending data");
			return -1;
		}
	}


	return 0;

}
