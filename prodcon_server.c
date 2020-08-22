#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <pthread.h>
#include <netinet/in.h>
#include <semaphore.h>
#include <stdint.h>
#include <prodcon.h>

sem_t full;
sem_t empty;

pthread_mutex_t mutex;
pthread_mutex_t it_mutex;




ITEM **all_items;
int it_count = 0;
int consumers= 0;
int producers= 0;
int clients= 0;



// void close_socket(int sock){
// 	pthread_mutex_lock(&mutex);
// 	clients--;
// 	pthread_mutex_unlock(&mutex);
// 	close(sock);
// 	pthread_exit(NULL);

// }

void close_producer(int sock){
	pthread_mutex_lock(&mutex);
	clients--;
	producers--;
	pthread_mutex_unlock(&mutex);
	close(sock);
}
void close_consumer(int sock){
	pthread_mutex_lock(&mutex);
	clients--;
	consumers--;
	pthread_mutex_unlock(&mutex);
	close(sock);
	pthread_exit(NULL);

}

void* con_thread(void *socket) {	
	int ssock = (intptr_t)socket;
	sem_wait(&full);
	pthread_mutex_lock( &it_mutex );
	ITEM *remov_item = all_items[it_count-1];
	all_items[it_count-1] = NULL;
	it_count--;
	printf("Client items is: %i\n",it_count);
	pthread_mutex_unlock(&it_mutex);
	sem_post(&empty);
	int upd=ntohl(remov_item->size);
	write( ssock, &upd, 4);
	char *temp = (char*)malloc(sizeof(char)*(BUFSIZE+1));
	int curr= 0;
	int readed = 0;
	write(remov_item->prod_sd,"GO\r\n",6);
	while(readed<remov_item->size){
		curr = read( remov_item->prod_sd, temp, BUFSIZE);
		readed = readed+ curr;
		temp[curr] = '\0';
		    if(curr>0) {
		    	write(ssock, temp, curr);
		}
			else if(curr==0){
				break;
			}
	}

	free(temp);
	if(readed==remov_item->size){
		write( remov_item->prod_sd, "DONE\r\n", 6);
		close_producer(remov_item->prod_sd);
		
	}

	else{
    	close_producer(remov_item->prod_sd);
		pthread_exit(NULL);
	}
	free(remov_item);
	close_consumer(ssock);

}
void* prod_thread(void *socket) {
	int ssock = (intptr_t)socket;
	write( ssock, "GO\r\n", 4);
	ITEM *prod_item;
	uint32_t len;
	read( ssock, &len, 4);
	int length = ntohl(len);
	printf("length %i\n",length);
	prod_item = (ITEM*)malloc(sizeof(ITEM));
	prod_item->size = length;
	prod_item->prod_sd=ssock;
	sem_wait( &empty);
	pthread_mutex_lock(&it_mutex);
	all_items[it_count] = prod_item;
	it_count++;
	pthread_mutex_unlock(&it_mutex);
	sem_post( &full );
	pthread_exit(NULL);

}

int main( int argc, char *argv[] ) {
	char		*service;
	struct		sockaddr_in	fsin;
	socklen_t	alen;
	int			msock;
	int			rport = 0;
	int			cc;
	int			num_items;
	pthread_t   thread;
	fd_set rfds;
	fd_set afds;
	int fd;
	int nfds;
	char buffer[BUFSIZE];


	switch (argc){
		case 1:
			rport = 1;
			break;
		case 3:
			service = argv[1];
			num_items = atoi(argv[2]);
			break;
		default:
			fprintf( stderr, "usage: server [port] [N of items]\n" );
			exit(-1);
	}

	all_items=malloc(sizeof(ITEM*)*num_items);

	msock = passivesock( service, "tcp", QLEN, &rport );
	fflush( stdout );
	
	if (rport != 0){
		printf( "Server started at port %i\n", rport );	
		fflush( stdout );
	}

	sem_init(&full, 0, 0);
	sem_init(&empty,0,num_items);
	pthread_mutex_init( &mutex, 0);
	pthread_mutex_init( &it_mutex, 0);

	
	nfds = msock+1;
	FD_ZERO(&afds);
	FD_SET( msock, &afds );

	for (;;){

		memcpy((char *)&rfds, (char *)&afds, sizeof(rfds));

		if (select(nfds, &rfds, (fd_set *)0, (fd_set *)0,
				(struct timeval *)0) < 0)
		{
			fprintf( stderr, "server select: %s\n", strerror(errno) );
			exit(-1);
		}
	
		if (FD_ISSET( msock, &rfds)) 
		{
			int	ssock;
			alen = sizeof(fsin);
			ssock = accept( msock, (struct sockaddr *)&fsin, &alen );
			if (ssock < 0)
			{
				fprintf( stderr, "accept: %s\n", strerror(errno) );
				exit(-1);
			}

			FD_SET( ssock, &afds );

			if ( ssock+1 > nfds )
				nfds = ssock+1;
		}

		for ( fd = 0; fd < nfds; fd++ )
		{
			if (fd != msock && FD_ISSET(fd, &rfds))
			{

				if ( (cc = read( fd, buffer, BUFSIZE )) <= 0 )
				{
					printf( "The client has gone.\n" );
					close(fd);
					
					
					if ( nfds == fd+1 )
						nfds--;

				}
				else
				{ 
						buffer[cc]='\0';
						if (strcmp(buffer, "PRODUCE\r\n") == 0) {
			 				pthread_mutex_lock(&mutex);
							if(producers<MAX_PROD && clients<MAX_CLIENTS){
								producers++;
								clients++;
								pthread_mutex_unlock(&mutex);
								pthread_t pthread;
								pthread_create(&pthread,NULL, prod_thread,(void*)(intptr_t)fd);
							}
							else{
								pthread_mutex_unlock(&mutex);
			    				printf("Reject.There is no room for producer!\n");	
			    				close(fd);
							}
						}	

						else if (strcmp(buffer,"CONSUME\r\n") == 0 ){
							pthread_mutex_lock(&mutex);
							if(consumers<MAX_CON && clients<MAX_CLIENTS) {
								consumers++;
								clients++;
								pthread_mutex_unlock(&mutex);
								pthread_t pthread;
								pthread_create(&pthread,NULL,con_thread,(void*)(intptr_t)fd);
							}
							else{
								pthread_mutex_unlock(&mutex);
								printf("Reject.There is no room for consumer!\n");
								close(fd);
							}	
						}
							FD_CLR( fd, &afds );
							if ( nfds == fd+1 )
								nfds--;


				}
			}
		}
	}
	pthread_exit(NULL);
}
