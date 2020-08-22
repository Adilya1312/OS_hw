#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <math.h>
#include <pthread.h>
#include <prodcon.h>
#define MAX 2000


char *host;
char *port;
int connectsock( char *host, char *service, char *protocol);


double poissonRandomInterarrivalDelay( double r )
{
    return (log((double) 1.0 - 
			((double) rand())/((double) RAND_MAX)))/-r;
}


void fillChunk(char *chunk, int size){
	for(int i = 0; i<size; i++) {
		 chunk[i]= 'X';
	}
	chunk[size]='\0';

}

void *produce(void* prm){
	char *buffer=(char*)malloc(sizeof(char)*BUFSIZE);

	int csock;
	int st;

	if ((csock = connectsock( host, port, "tcp")) == 0){
			fprintf( stderr, "Cannot connect to server.\n" );
			pthread_exit( NULL );
	}

	int bad=(intptr_t)prm;
	if(bad==0){
		sleep(SLOW_CLIENT);
	}
	strcpy(buffer, "PRODUCE\r\n");
	st = write(csock, buffer, strlen(buffer));
	if ( st < 0 ){
		fprintf( stderr, "producer write: %s\n", strerror(errno) );
		pthread_exit(NULL);
	}
	printf("Producer is connected\n");
	buffer[0]='\0';
	int server = read(csock, buffer, BUFSIZE);
	buffer[server]='\0';

	int incremented=0;
	char *temp=(char*)malloc(sizeof(char)*(BUFSIZE+1));
	if(strcmp(buffer, "GO\r\n")==0){
			buffer[0]='\0';
			int length=1+rand()%MAX_LETTERS;
			printf("length is %i\n", length);
			int len=ntohl(length);
			write(csock, &len, sizeof(len));
			
			read(csock, buffer, 4);

			if(strcmp(buffer, "GO\r\n")==0){

				int size;
				while(incremented<length){
					if(incremented+BUFSIZE<=length){
						size=BUFSIZE;
					}
					else{
						size=length-incremented;
					}
					fillChunk(temp,size);
					incremented=incremented+size;
					write(csock, temp, size);
					
				}

			}
	}

	else{
		printf("There is no room in the buffer to place an item for producer");
		close(csock);
		pthread_exit(NULL);
	}
	buffer[0]='\0';
	read( csock, buffer, BUFSIZE);
	if (strcmp(buffer, "DONE\r\n") == 0) {
		printf( "Item is saved on the server\n" );
	}
	free(temp);
	free(buffer);
	close(csock);
	pthread_exit(NULL);
}

int main(int argc, char *argv[]){
	srand(time(NULL));
	int prod_number;
	double rate;
	int bad;
	switch(argc){
		case 6:
			host=argv[1];
			port=argv[2];
			prod_number=atoi(argv[3]);
			rate=atof(argv[4]);
			bad=atoi(argv[5]);
			break;
		default:
		 fprintf(stderr, "usage: producer [host] [port] [producer amount] [rate] [bad]\n");
		 exit(-1);

	}
	if(rate>0 && bad<=100 && bad>=0){

		pthread_t threads[prod_number];
		int st;
		int i=0;
		int j, temp;
    	int range=(prod_number*bad)/100;
    	int prod[prod_number];
    	srand(time(NULL));
    	for(i=0;i<prod_number;i++){
    		prod[i]=i;
	    }
	    for(i=0;i<prod_number;i++){
	    	j=rand()%prod_number;
	    	temp=prod[i];
	    	prod[i]=prod[j];
	    	prod[j]=temp;
	    }
	    i=0;


		while(i!=prod_number){
			bad=1;
			int num=prod[i];
			if(num<range){
				bad=0;

			}
			useconds_t t=poissonRandomInterarrivalDelay(rate)*1000000;
			printf("producer%i - %d - %d\n",i+1, t, bad);
			usleep(t);
			st=pthread_create(&threads[i],NULL,produce, (void*)(intptr_t)bad);
			if(st!=0){
				fprintf(stderr, "pthread_create returned error %i\n", st);
				exit(-1);
			}
			i++;
		}
		pthread_exit(0);
	}
		else{
			printf("invalid input\n");
			exit(-1);
		}
}