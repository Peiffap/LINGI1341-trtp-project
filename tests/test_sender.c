#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include "../src/utilities.h"
#include <fcntl.h>
#include <errno.h>
#include <inttypes.h>
#include <CUnit/Basic.h>


#define MAX_PAYLOAD_LENGTH 512
#define MAX_SEQNUM 256
#define TIMEOUT 1000000
#define NOACKRECEIV 10

// Seqnum of last packet read
uint8_t lastseqnum = 0;

//Size of window
uint8_t window = 1;

//Seqnum of the first packet in window
uint8_t firstseqnumwindow = 0;

//Time of the last ack received
struct timespec timeoflastack;

//Queue to store all the pkt already encoded in char *
struct dataqueue {
	char *bufpkt;
	uint8_t seqnum;
	uint16_t len;
	struct timespec time;
	struct dataqueue *next;
};

//Start of the queue with pkts that the receiver has not received yet
struct dataqueue *startofqueue = NULL;

//Next pkt to send
struct dataqueue *firsttosend = NULL;

//End of the queue with pkts to send
struct dataqueue *lasttosend = NULL;

//Seqnum of the last ack received
uint8_t lastackseqnum = -1;

//Pkts not received by the receiver yet
int pkt_waiting = 0;

//Pkts not sent yet (for the 1st time)
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

//Remove pkt with pkt_get_seqnum(pkt) less than seqnum (according to cmp())
int remove_pkt(uint8_t seqnum){
	fprintf(stderr,"[LOG] [SENDER] Removing packet with sequence number %d\n", seqnum);
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
	if(startofqueue == NULL){
		lasttosend = NULL;
		firsttosend = NULL;
	}
	fprintf(stderr,"[LOG] [SENDER] Packet with sequence number %d removed\n", seqnum);
	return 0;
}

int add_pkt_to_queue( char* buf, int len){
	int timestamp = 0;
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

	fprintf(stderr,"[LOG] [SENDER] Finished adding packet %d to queue \n", lastseqnum);

	lastseqnum = (lastseqnum + 1)%MAX_SEQNUM;

	return 0;

}

void addandremove(){

	lastseqnum = 0;
	char *s ="hello";

	int err = add_pkt_to_queue(s, 5);

	CU_ASSERT_EQUAL(err, 0);

	pkt_t *packet = pkt_new();


	pkt_status_code status = pkt_decode(lasttosend->bufpkt, 5+4*sizeof(uint32_t), packet);

	const char *s3 = pkt_get_payload(packet);
	char s2[6];
	s2[0] = s3[0];
	s2[1] = s3[1];
	s2[2] = s3[2];
	s2[3] = s3[3];
	s2[4] = s3[4];
	CU_ASSERT_EQUAL(status, PKT_OK);
	CU_ASSERT_STRING_EQUAL(s, s2);
	CU_ASSERT_EQUAL(lastseqnum,1);

	err = remove_pkt(0);

	CU_ASSERT_EQUAL(err,0);
	CU_ASSERT_PTR_NULL(lasttosend);




}
int main(){
  CU_pSuite senderSuite = NULL;
	/* initialisation de la suite*/
	if(CUE_SUCCESS != CU_initialize_registry())
		return CU_get_error();

	/* création de la suite */
	senderSuite = CU_add_suite("Sender",NULL,NULL);
	if(NULL == senderSuite){
		CU_cleanup_registry();
		return CU_get_error();
	}

	/* Ajout à la suite */
	if(NULL == CU_add_test(senderSuite, "add and remove", addandremove) ) {
		CU_cleanup_registry();
		return CU_get_error();
	}

	/* Lancement des tests */
	CU_basic_set_mode(CU_BRM_VERBOSE);
	CU_basic_run_tests();
	CU_cleanup_registry();

	return CU_get_error();
}
