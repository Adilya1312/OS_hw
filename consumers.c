#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <prodcon.h>
#define MAX 2000

char *port;
char *host;
int connectsock( char *host, char *service, char *protocol);

double poissonRandomInterarrivalDelay( double r )
{
    return (log((double) 1.0 - 
			((double) rand())/((double) RAND_MAX)))/-r;
}

void *consume(void *prm){

	char *fname = (char*)malloc(sizeof(char)*200);
	pthread_t pid = pthread_self();
	sprintf(fname, "%lu.txt", pid);
	int fd = open(fname,  O_RDWR | O_CREAT, S_IRUSR);	
    int csock;
	int st;
	char *buffer=(char*)malloc(sizeof(char)*BUFSIZE);
	int bad=(intptr_t)prm;
	char *bytes=(char*)malloc(sizeof(char)*BUFSIZE);
	int dev_null=open("/dev/null", O_WRONLY);
	if ((csock = connectsock( host, port, "tcp")) == 0){
		fprintf( stderr, "Cannot connect to server.\n" );
		pthread_exit( NULL );
	}

	if(bad==0){
		sleep(SLOW_CLIENT);
	}
	strcpy(buffer,"CONSUME\r\n");
	st = write(csock, buffer, strlen(buffer));
	if ( st < 0 ){
		fprintf( stderr, "consumer write: %s\n", strerror(errno) );
		sprintf(bytes, "ERROR: REJECTED");
		write(fd, bytes, strlen(bytes));
		pthread_exit(NULL);
	}
	
	int tmp;
	st = read(csock, &tmp, 4);
	int len=ntohl(tmp);
	printf("length is %i\n", len);

	if( st < 0){
		printf( "The server has gone.\n" );
		sprintf(bytes, "ERROR: REJECTED");
		write(fd, bytes, strlen(bytes));
        close(csock);
        pthread_exit(NULL);
	}
	char *temp = (char*)malloc(sizeof(char)*(BUFSIZE+1));
	int readed=0;
	int cur=0; 
	while(readed<len){
		cur = read(csock,temp, BUFSIZE);
		readed= readed +cur;
		temp[cur]='\0';
			if(cur>0){
				write(dev_null, temp, cur);
			}
			else if(cur==0){
				break;
			}
	}


	
	if(readed == len){
		sprintf(bytes, "SUCCESS: bytes %d", len);
	}
	else{
		printf("can not read an item\n");
		close(csock);
		sprintf(bytes, "ERROR: bytes %d", readed);
	}


	free(temp);
	free(buffer);
	write(fd, bytes, strlen(bytes));
	free(bytes);
	close(fd);
	close( csock );
	pthread_exit( NULL);

}


int main(int argc, char* argv[]){

	host="localhost";
	int cons_number;
	double rate;
	int bad;
	switch(argc){
		case 6:
			host=argv[1];
			port=argv[2];
			cons_number=atoi(argv[3]);
			rate=atof(argv[4]);
			bad=atoi(argv[5]);
			break;
		default:
			fprintf(stderr, "usage: consumer [host] [port] [consumer amount] [rate] [bad]");
			exit(-1);
	}

	if(rate>0 && bad<=100 && bad>=0){
		pthread_t threads[cons_number];
		int st;
		int i=0;
		int j, temp;
	    int range=(cons_number*bad)/100;
	    int con[cons_number];
	    srand(time(NULL));
	    for(i=0;i<cons_number;i++){
	    	con[i]=i;
	    }
	    for(i=0;i<cons_number;i++){
	    	j=rand()%cons_number;
	    	temp=con[i];
	    	con[i]=con[j];
	    	con[j]=temp;
	    }
	    i=0;
		printf("consumer amount is %i\n", cons_number);
		while(i!=cons_number){
			bad=1;
			int num=con[i];
			if(num<range){
				bad=0;
			}
			useconds_t t=poissonRandomInterarrivalDelay(rate)*1000000;
			printf("consumer%i - %d - %i \n",i+1, t, bad);
			usleep(t);
			st=pthread_create(&threads[i],NULL,consume ,(void*)(intptr_t)bad);
			if(st!=0){
				fprintf(stderr, "pthread_create returned error %i \n", st);
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