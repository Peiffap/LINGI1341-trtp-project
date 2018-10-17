#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>


int main( int argc, char * argv[]){

	if(argc <3){
		perror("Not enough arguments");
		return -1;
	}

	char *hostname;
	char *filename = NULL;
	int port;
	if(strcmp(argv[1], "-f")==0){
		filename = argv[2];
		hostname = argv[3];
		port = atoi(argv[4]);
	}
	else{
		hostname = argv[1];
		port = atoi(argv[2]);	
	}

	struct sockaddr_in6 * dest_addr;

	char *err = real_address(hostname, dest_addr);
	if(err != NULL){
		perror(err);
		return -1;
	}

	int sfd = create_socket(NULL, 0, dest_addr, port);

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
		send_data(sfd, fd);
		
	}
	else {
		send_data(sfd, 0);
	}


	return 0;

}
