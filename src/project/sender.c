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

#define MAX_PAYLOAD_LENGTH 512
#define MAX_SEQNUM 256
#define TIMEOUT 999


uint8_t lastseqnum = 0;
uint8_t window = 15;
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
uint8_t lastackseqnum = 0;
int pkt_waiting = 0;


int remove_pkt(uint8_t seqnum){
	struct dataqueue *current = startofqueue;



	if(current != NULL && current->seqnum <= seqnum){
		startofqueue = current->next;
		pkt_waiting--;
		free(current->bufpkt);
		free(current);
	}
	else{
		current = current->next;
		struct dataqueue *before = startofqueue;
		while(current!=NULL){
			if(current->seqnum <= seqnum){

				before->next = current->next;
				pkt_waiting--;
				free(current->bufpkt);
				free(current);

			}

			else{
				before = before->next;
				current = current->next;
			}

		}
	}
	if(startofqueue != NULL && startofqueue->seqnum <= seqnum){
		current = startofqueue;
		startofqueue = startofqueue->next;
		pkt_waiting--;
		free(current->bufpkt);
		free(current);
	}

	return 0;
}

int send_pkt(const int sfd){

	if((firsttosend != NULL)&&
		(((firsttosend->seqnum >= firstseqnumwindow)
			&&(firsttosend->seqnum < firstseqnumwindow + window)
			&&(firstseqnumwindow+window< MAX_SEQNUM))
		||((firstseqnumwindow+window> MAX_SEQNUM)&&((firsttosend->seqnum >= firstseqnumwindow)
			||(firsttosend->seqnum < (firstseqnumwindow + window)%MAX_SEQNUM))))){

		int err = write(sfd, firsttosend->bufpkt, firsttosend->len);
		printf("[LOG] [SENDER] writing pkt on socket \n");
		printf("%d %d \n", firsttosend->bufpkt[0], firsttosend->bufpkt[100]);
		if(err <0){
			perror("error writing pkt");
			return -1;
		}

		err = clock_gettime(CLOCK_MONOTONIC,&(firsttosend->time));

		if(err!=0){
			perror("error clock in sending data");
			return -1;
		}
		printf("before %d \n", firsttosend->len);

		for(int i = 0; i<528; i++){
			printf("%x", firsttosend->bufpkt[i]);
		}

			printf("before\n");

		firsttosend = firsttosend->next;
	}

	// Else need an ack

	return 0;
}


int add_pkt_to_queue( char* buf, int len){
	printf("[LOG] [SENDER] adding pkt to queue \n");
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

	printf("[LOG] [SENDER] finished adding pkt to queue %d \n", bufpkt[0]);
	lastseqnum = (lastseqnum + 1)%MAX_SEQNUM;

	return 0;

}


int disconnect(int sfd){

	printf("[LOG] [SENDER] disconnection \n");

	int timestamp = 0; // How do we use the timestamp ??
	pkt_t * newpkt = pkt_create_sender(window, lastackseqnum, 0, timestamp, NULL);

	char bufpkt[3*sizeof(uint32_t)];
	size_t totlen = 3*sizeof(uint32_t);
	pkt_status_code ret = pkt_encode(newpkt, bufpkt, &totlen);
	if(ret != PKT_OK){
		perror("error encoding pkt in disconnect");
		return -1;
	}

	pkt_del(newpkt);

	int errw = write(sfd, bufpkt, totlen);
	if(errw <0){
		perror("error writing pkt in disconnect");

		return -1;
	}

	printf("[LOG] [SENDER] send disconnection pckt \n");


	struct timespec time1;
	errw = clock_gettime(CLOCK_MONOTONIC,&time1);

	if(errw!=0){
		perror("error clock in disconnect");

		return -1;
	}



	struct timeval tv;
	fd_set readfds;
	tv.tv_sec= 1;
	tv.tv_usec = 0;

	char buf2[sizeof(pkt_t)];

	int end = 0;
	int err;
	while (!end) {
		memset((void *) buf2, 0, sizeof(pkt_t));
		FD_ZERO(&readfds);
		FD_SET(sfd, &readfds);

		select(sfd + 1, &readfds, NULL, NULL, &tv);

		if (FD_ISSET(sfd, &readfds)) {
			printf("[LOG] [SENDER] response to disconnection request? \n");

			err = read(sfd, buf2, sizeof(pkt_t));
			printf("%d %d\n",err, errno);
			if (err <= 0){
				perror("error reading from socket in disconnection: ");
				return -1;
			}
			pkt_t *ack = pkt_new();
			err = pkt_decode(buf2, err, ack);
			if(err != PKT_OK){
				perror("error decoding ack in disconnection");
				pkt_del(ack);
				return -1;
			}
			int seqnum = pkt_get_seqnum(ack);


			if(pkt_get_type(ack) == PTYPE_ACK){

				if(seqnum == lastackseqnum){
					printf("[LOG] [SENDER] disconnection ok \n");

					pkt_del(ack);
					end = 1;
					return 0;
				}
			}

		}

		struct timespec time;
		int errc = clock_gettime(CLOCK_MONOTONIC,&time);
		if(errc!=0){
			perror("error get time in send data");
			return -1;
		}
		if(((time.tv_sec - time1.tv_sec)*1000000 + (time.tv_nsec - time1.tv_nsec)/1000)>TIMEOUT){

		//RESEND DATA
		printf("[LOG] [SENDER] disconnection timeout, resend request\n");

			int errw1 = write(sfd, bufpkt, totlen);
			if(errw1 <0){
				perror("error writing pkt in resending pkt");
				return -1;
			}

			errw1 = clock_gettime(CLOCK_MONOTONIC,&time1);

			if(errw1!=0){
				perror("error clock in resending data");
				return -1;
			}

		}

	}

	return -1;

}
int send_data(const int sfd, const int fd){

	struct timeval tv;
	fd_set readfds;
	tv.tv_sec= 0;
	tv.tv_usec = 100;

	char buf1[MAX_PAYLOAD_LENGTH];
	char buf2[sizeof(pkt_t)+MAX_PAYLOAD_LENGTH];


	int err;
	int stop = 1;
	while (pkt_waiting > 0 || stop) {
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
		if (FD_ISSET(fd, &readfds)) {
		    	err = read(fd, buf1, MAX_PAYLOAD_LENGTH);
			printf("[LOG] [SENDER] reading data %d\n", buf1[0]);
			if (err < 0 ){
				perror("error read fd in send data ");
				printf("[ERR] [SENDER] %d, %d \n", err, errno);
				return -1;
			}
			if(err >0){
				int erradd = add_pkt_to_queue(buf1, err);
				if(erradd != 0){
					perror("error adding pkt to queue");
					return -1;
				}
				pkt_waiting ++;
			}
			if(err == 0){
				stop = 0;
			}
			printf("[LOG] [SENDER] err read %d, %d \n", err, errno);
		}

		if(pkt_waiting >0){
			int errsend = send_pkt(sfd);

			printf("[LOG] [SENDER] sending data \n");
			if(errsend !=0){
				perror("error sending pkt");
				return -1;
			}

		}

		if (FD_ISSET(sfd, &readfds)) { // Data incoming from socket
			printf("[LOG] [SENDER] reading ack? from socket \n");


	    	err = read(sfd, buf2, sizeof(pkt_t)+MAX_PAYLOAD_LENGTH);

	    	if (err <= 0){
				perror("error reading from socket : ");
				return -1;
			}
			pkt_t *ack = pkt_new();
			err = pkt_decode(buf2, err, ack);
			if(err != PKT_OK){
				perror("error decoding ack");
				pkt_del(ack);
				return -1;
			}
			int seqnum = pkt_get_seqnum(ack);
			lastackseqnum = seqnum;

			if(pkt_get_type(ack) == PTYPE_ACK){
			printf("[LOG] [SENDER] this is an ack \n");

				firstseqnumwindow = (pkt_get_seqnum(ack)%MAX_SEQNUM);
				remove_pkt(seqnum);




			}
			if(pkt_get_type(ack) == PTYPE_NACK){

				printf("[LOG] [SENDER] this is a nack \n");

				struct dataqueue *current = startofqueue;
				int search = 1;
				while((current!=0)&&(search)){
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
				printf("[LOG] [SENDER] time out, resending data \n");
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
