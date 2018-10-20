#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "packet_interface.h"
#include <time.h>
#include "utilities.h"
#include <fcntl.h>

#define MAX_PAYLOAD_LENGTH 512
#define MAX_SEQNUM 256
#define TIMEOUT 999

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

void send_pkt(const int sfd){

	if((firsttosend != NULL)&&
		(((firsttosend->seqnum >= firstseqnumwindow)
			&&(firsttosend->seqnum < firstseqnumwindow + window)
			&&(firstseqnumwindow+window< MAX_SEQNUM))
		||((firstseqnumwindow+window> MAX_SEQNUM)&&((firsttosend->seqnum >= firstseqnumwindow)
			||(firsttosend->seqnum < (firstseqnumwindow + window)%MAX_SEQNUM))))){
		
		int err = write(sfd, firsttosend->bufpkt, firsttosend->len);
		if(err <0){
			perror("error writing pkt");
		}

		err = clock_gettime(CLOCK_MONOTONIC,&(firsttosend->time));
		
		if(err!=0){
			perror("error clock in sending data");
		}
		firsttosend = firsttosend->next;
	}

	// Else need an ack


}


void add_pkt_to_queue( char* buf, int len){
	int timestamp = 0; // How do we use the timestamp ??
	pkt_t * newpkt = pkt_create(window, lastseqnum, len, timestamp, buf);
	char bufpkt[len+4*sizeof(uint32_t)];
	size_t totlen = len+4*sizeof(uint32_t);
	pkt_status_code ret = pkt_encode(newpkt, bufpkt, &totlen);
	if(ret != PKT_OK){
		perror("error encoding pkt in add_pkt_to_queue");
	}

	struct dataqueue *newdata = (struct dataqueue *)malloc(sizeof(struct dataqueue));
	lasttosend->next = newdata;
	lasttosend = newdata;		
	lasttosend->bufpkt = bufpkt;
	lasttosend->seqnum = lastseqnum;
	lasttosend->len = totlen;
	lasttosend->next = NULL;

	if(firsttosend == NULL){
		firsttosend = lasttosend;
		startofqueue = lasttosend;
	}

	lastseqnum = (lastseqnum + 1)%MAX_SEQNUM;
	

}
int send_data(const int sfd, const int fd){

	struct timeval tv;
	fd_set readfds;
	tv.tv_sec= 6;
	tv.tv_usec = 0;

	char buf1[MAX_PAYLOAD_LENGTH];
	char buf2[MAX_PAYLOAD_LENGTH];

	int pkt_waiting = 0;

	int err;
	while (feof(stdin)) {
		memset((void *) buf1, 0, MAX_PAYLOAD_LENGTH);
		memset((void *) buf2, 0, MAX_PAYLOAD_LENGTH);
		FD_ZERO(&readfds);
		FD_SET(fd, &readfds);
		FD_SET(sfd, &readfds);

		int selerr = select(sfd + 1, &readfds, NULL, NULL, &tv);
		
		if (selerr == -1){
			perror("error select");
			return -1;
			
		}		
		if (FD_ISSET(fd, &readfds)) {
		    	err = read(fd, buf1, MAX_PAYLOAD_LENGTH);
			if (err <= 0 ){
				perror("error read fd in send data ");
				return -1;
			}
			add_pkt_to_queue(buf1, err);
			pkt_waiting ++;
		}

		if(pkt_waiting >0){
			send_pkt(sfd);
			//pkt_waiting--;
		}

		if (FD_ISSET(sfd, &readfds)) { // Data incoming from socket
		    	err = read(sfd, buf2, sizeof(pkt_t));

		    	if (err <= 0){
				perror("error reading from socket : ");
				return -1;
			}
			pkt_t *ack = pkt_new();
			err = pkt_decode(buf2, sizeof(pkt_t), ack);
			if(err != PKT_OK){
				perror("error decoding ack");
				return -1;
			}
			if(pkt_get_type(ack) == PTYPE_ACK){
				firstseqnumwindow = pkt_get_seqnum(ack);			
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

				int errw = write(sfd, current->bufpkt, current->len);
				if(errw <0){
					perror("error writing pkt in resending pkt");
					return -1;
				}

				errw = clock_gettime(CLOCK_MONOTONIC,&(current->time));
				
				if(errw!=0){
					perror("error clock in resending data");
					return -1;
				}

			}
			current = current->next;
		}
		

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
			return -1;
		}
		
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
